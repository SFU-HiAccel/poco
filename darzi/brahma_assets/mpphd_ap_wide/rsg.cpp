#ifdef DEBUG_RSG
#define DEBUG_PRINT_RSG(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_RSG(...)
#endif

void rsg(tapa::istream<sb_cmsg_t>& rqp_to_rsg_grab,
        tapa::istream<sb_cmsg_t>& rqp_to_rsg_free,
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_rsg_read,
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqp_to_rsg_write,
        tapa::istreams<sb_apkt_t, SB_NXCTRS>& ihd_to_rsg_read,
        tapa::istreams<sb_apkt_t, SB_NXCTRS>& ohd_to_rsg_write,
        tapa::ostreams<sb_rsp_t, SB_NXCTRS>& btxqs) {

  // Free > Grab > Read > Write
  sb_std_t  f_fwd_rsp = {0}, g_fwd_rsp = {0};
  sb_std_t  rc_fwd_rsp[SB_NXCTRS], wc_fwd_rsp[SB_NXCTRS];
  sb_apkt_t rd_fwd_rsp[SB_NXCTRS], wd_fwd_rsp[SB_NXCTRS];
  bool vld_rsg_g, vld_rsg_f;
  bool vld_rsg_rc[SB_NXCTRS], vld_rsg_rd[SB_NXCTRS], burst_done[SB_NXCTRS];
  bool vld_rsg_wc[SB_NXCTRS], vld_rsg_wd[SB_NXCTRS];
  uint8_t burst_size[SB_NXCTRS] = {0};
  //#pragma HLS array_partition variable=r_fwd_rsp  type=complete
  //#pragma HLS array_partition variable=w_fwd_rsp  type=complete
  //#pragma HLS array_partition variable=vld_rsg_r  type=complete
  //#pragma HLS array_partition variable=vld_rsg_w  type=complete
  //#pragma HLS array_partition variable=burst_size type=complete
  //#pragma HLS array_partition variable=burst_done type=complete
  //#pragma HLS array_partition variable=skid_read  type=complete

  RSG_INIT: for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
  {
    burst_done[xctr] = true;
  }

  // // peek       --> r_pvld + r_data
  // // try_write  --> w_tvld
  // // read       <-- if(w_tvld)

  // rvld = istream.try_peek(data);
  // if(rvld)
  //   wvld = ostream.try_write(w_tvld);
  // else
  //   wvld = false;
  // if(wvld)
  //   istream.read(nullptr);

  bool cons_free = false, cons_grab = false;

  RSG_LOOP: for(;;)
  {
    #pragma HLS latency max=0
    vld_rsg_f = rqp_to_rsg_free.try_peek(f_fwd_rsp.control);
    vld_rsg_g = rqp_to_rsg_grab.try_peek(g_fwd_rsp.control);

    // TODO: usage of this must be fixed later. Current assumption is that DP is prioritised higher than CP
    // at the task level, so no data transactions are pending before a control request on a xctr is issued.
    cons_free = false;
    cons_grab = false;
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
    {
      #pragma HLS unroll
      // if(!skid_read[xctr]) { vld_rsg_r[xctr] = rqp_to_rsg_read[xctr].try_read(r_fwd_rsp[xctr]);
      vld_rsg_wc[xctr] = (rqp_to_rsg_write[xctr].try_peek(wc_fwd_rsp[xctr]));
      vld_rsg_wd[xctr] = (ohd_to_rsg_write[xctr].try_peek(wd_fwd_rsp[xctr]));
      vld_rsg_rc[xctr] = (rqp_to_rsg_read[xctr].try_peek(rc_fwd_rsp[xctr]));
      vld_rsg_rd[xctr] = (ihd_to_rsg_read[xctr].try_peek(rd_fwd_rsp[xctr]));

      const bool vld_free_pkt  = (vld_rsg_f && ((sb_portid_t)f_fwd_rsp.control.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB) == xctr));
      const bool vld_grab_pkt  = (vld_rsg_g && ((sb_portid_t)g_fwd_rsp.control.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB) == xctr));
      const bool vld_read_pkt  = (vld_rsg_rd[xctr] && vld_rsg_rc[xctr]);
      const bool vld_write_pkt = (vld_rsg_wd[xctr] && vld_rsg_wc[xctr]);
      const bool vld_fwd = !btxqs[xctr].full();

      if(vld_fwd && vld_read_pkt)             // READS
      {
        btxqs[xctr].try_write(std_to_rsp(rd_fwd_rsp[xctr].msg));  // this try_write must succeed since its guarded with vld_fwd
        sb_std_t nc_rc; sb_apkt_t nc_rd;
        rqp_to_rsg_read[xctr].try_read(nc_rc);
        ihd_to_rsg_read[xctr].try_read(nc_rd);
        DEBUG_PRINT_RSG("[RSG][xctr:%2d][R]: TX response\n", xctr);
      }
      else if(vld_fwd && vld_write_pkt)       // WRITES
      {
        btxqs[xctr].try_write(std_to_rsp(wd_fwd_rsp[xctr].msg));
        sb_std_t nc_wc; sb_apkt_t nc_wd;
        rqp_to_rsg_write[xctr].try_read(nc_wc);
        ohd_to_rsg_write[xctr].try_read(nc_wd);
        DEBUG_PRINT_RSG("[RSG][xctr:%2d][W]: TX response\n", xctr);
      }
      else if(vld_fwd && vld_free_pkt)        // FREE
      {
        cons_free |= true;
        btxqs[xctr].try_write(std_to_rsp(f_fwd_rsp));
        DEBUG_PRINT_RSG("[RSG][xctr:%2d][F]: TX response\n", xctr);
      }
      else if(vld_fwd && vld_grab_pkt)        // GRAB
      {
        cons_grab |= true;
        btxqs[xctr].try_write(std_to_rsp(g_fwd_rsp));
        DEBUG_PRINT_RSG("[RSG][xctr:%2d][G]: TX response\n", xctr);
      }
      else{}
    }
    if(cons_free)
    {
      sb_cmsg_t nc_free;
      rqp_to_rsg_free.try_read(nc_free);
    }
    if(cons_grab)
    {
      sb_cmsg_t nc_grab;
      rqp_to_rsg_grab.try_read(nc_grab);
    }
  }
}
