#ifdef DEBUG_RQR
#define DEBUG_PRINT_RQR(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_RQR(...)
#endif

// Request Router
void rqr(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
        tapa::ostreams<sb_cmsg_t, SB_NXCTRS>& rqr_to_rqp_grab,
        tapa::ostreams<sb_cmsg_t, SB_NXCTRS>& rqr_to_rqp_free,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_read,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& rqr_to_rqp_write) {

  bool valid[SB_NXCTRS];
  bool fwd_rqp_free[SB_NXCTRS];
  bool fwd_rqp_grab[SB_NXCTRS];
  bool fwd_rqp_read[SB_NXCTRS];
  bool fwd_rqp_write[SB_NXCTRS];
  bool burst_done[SB_NXCTRS];
  bool skid_read[SB_NXCTRS];
  bool written_f = false, written_g = false, written_r = false, written_w = false;
  uint8_t burst_size[SB_NXCTRS] = {0};
  sb_req_t req[SB_NXCTRS];
  sb_std_t std_req[SB_NXCTRS];
  bool c_dn[SB_NXCTRS]; uint8_t code[SB_NXCTRS];

  #pragma HLS array_partition variable=fwd_rqp_free type=complete
  #pragma HLS array_partition variable=fwd_rqp_grab type=complete
  #pragma HLS array_partition variable=fwd_rqp_read type=complete
  #pragma HLS array_partition variable=fwd_rqp_write type=complete
  #pragma HLS array_partition variable=skid_read type=complete
  #pragma HLS array_partition variable=burst_done type=complete
  #pragma HLS array_partition variable=burst_size type=complete
  #pragma HLS array_partition variable=req type=complete
  #pragma HLS array_partition variable=std_req type=complete

  RQR_INIT: for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
  {
    burst_done[xctr] = true;
    skid_read[xctr] = false;
  }

  RQR_LOOP: for(;;)
  {
    #pragma HLS latency max = 0
    for(uint8_t xctr = 0; xctr < SB_NXCTRS; xctr++) // this check is being done for each xctr stream being rxed
    {
      #pragma HLS unroll    // full unroll by a factor of SB_NXCTRS
      // try reading whether value is available only if the previous value is read already
      req[xctr] = brxqs[xctr].peek(valid[xctr]);
      code[xctr] = req[xctr].control.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB);
      c_dn[xctr] = req[xctr].c_dn;
      fwd_rqp_free[xctr]  = valid[xctr] && ( code[xctr] == SB_REQ_FREE_PAGE);
      fwd_rqp_grab[xctr]  = valid[xctr] && ( code[xctr] == SB_REQ_GRAB_PAGE);
      fwd_rqp_read[xctr]  = valid[xctr] && ((code[xctr] & SB_RW_MASK) == SB_REQ_READ_S) ;   // lower two bits signify R/W
      fwd_rqp_write[xctr] = valid[xctr] && ((code[xctr] & SB_RW_MASK) == SB_REQ_WRITE_S);   // lower two bits signify R/W

      std_req[xctr] = req_to_std(req[xctr]);

      const bool written_f =  fwd_rqp_grab[xctr] && rqr_to_rqp_free[xctr].try_write(std_req[xctr].control);
      const bool written_g =  fwd_rqp_grab[xctr] && rqr_to_rqp_grab[xctr].try_write(std_req[xctr].control);
      const bool written_r =  fwd_rqp_read[xctr] && rqr_to_rqp_read[xctr].try_write(std_req[xctr]);
      const bool written_w = fwd_rqp_write[xctr] && rqr_to_rqp_write[xctr].try_write(std_req[xctr]);
      if((written_f || written_g || written_r || written_w) && valid[xctr])
      {
        brxqs[xctr].read(nullptr);
      }

      #ifndef __SYNTHESIS__
      if(valid[xctr])
      {
        if(written_f) {
          DEBUG_PRINT_RQR("[RQR][F]: Page: %x\n", std_req[xctr].control.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB));
        } else if(written_g) {
          DEBUG_PRINT_RQR("[RQR][G]: Page: %x\n", std_req[xctr].control.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB));
        } else if(written_w) {
          DEBUG_PRINT_RQR("[RQR][W]: Address: %x, Data: %lx\n", std_req[xctr].control.range(SB_CMSG_ADDR_MSB, SB_CMSG_ADDR_LSB).to_int(), std_req[xctr].std_msg.msg[0]);
        } else if(written_r) {
          DEBUG_PRINT_RQR("[RQR][R]: Address: %x, Data: %lx\n", std_req[xctr].control.range(SB_CMSG_ADDR_MSB, SB_CMSG_ADDR_LSB).to_int(), std_req[xctr].std_msg.msg[0]);
        }
      }
      #endif
    }
  }
}
