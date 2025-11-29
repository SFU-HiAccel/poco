#ifdef DEBUG_DRP
#define DEBUG_PRINT_DRP(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_DRP(...)
#endif

/**
 * Task     : Data Request Parser
 * Purpose  : The Data Request Parser is the intercept between RQR and I/OHD
 * */
void drp(
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqr_to_drp_read,
        tapa::istreams<sb_std_t, SB_NXCTRS>& rqr_to_drp_write,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& drp_to_rsg_read,
        tapa::ostreams<sb_std_t, SB_NXCTRS>& drp_to_rsg_write,
        tapa::ostreams<sb_apkt_t, SB_NXCTRS>& drp_to_ihd_read,
        tapa::ostreams<sb_apkt_t, SB_NXCTRS>& drp_to_ohd_write) {

  bool vld_rqr_w[SB_NXCTRS] = {0};
  bool vld_rqr_r[SB_NXCTRS] = {0};
  bool burst_done[SB_NXCTRS];
  bool skid_read[SB_NXCTRS];
  sb_pageid_t pageid[SB_NXCTRS] = {0};
  uint8_t burst_size[SB_NXCTRS] = {0};
  uint8_t xcsrid[SB_NXCSRS] = {0};
  sb_apkt_t w_fwd_req[SB_NXCTRS];
  uint8_t tags[SB_NXCSRS][SB_PAGES_PER_XCSR] = {0};
  // ap_uint<SB_TAG_WIDTH> tags[SB_NXCSRS][SB_PAGES_PER_XCSR] =  {0};
  #pragma HLS array_partition variable=vld_rqr_r type=complete
  #pragma HLS array_partition variable=vld_rqr_w type=complete

  enum drp_writestate_e {BURST_START=0, BURST_CONT, BURST_STOP};
  drp_writestate_e cs_w[SB_NXCTRS], ns_w[SB_NXCTRS];

  DRP_INIT: for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
  {
    burst_done[xctr] = true;
    skid_read[xctr] = false;
    cs_w[xctr] = BURST_START;
    ns_w[xctr] = BURST_START;
  }

  DRP_LOOP: for(;;)
  {
    #pragma HLS latency max=0
    for(sb_portid_t xctr = 0; xctr < SB_NXCTRS; xctr++)
    {
      #pragma HLS unroll
      vld_rqr_w[xctr] = rqr_to_drp_write[xctr].try_peek(w_fwd_req[xctr].msg);
      if(vld_rqr_w[xctr]/* && burst_done[xctr]*/) {               // new write request
        // prepare for burst
        xcsrid[xctr] = w_fwd_req[xctr].msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB);     // extract the xcsr from the pageid
        pageid[xctr] = w_fwd_req[xctr].msg.control.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB); // extract the full pageid
        
        // set arbiter-pkt's tag to the xcsr to route this packet to relevant I/OHD
        w_fwd_req[xctr].tag = xcsrid[xctr];
        // swap xcsr information in packet with xctr information
        w_fwd_req[xctr].msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB) = xctr;

        const bool written_w = !drp_to_rsg_write[xctr].full() && !drp_to_ohd_write[xctr].full();
        if(written_w)
        {
          DEBUG_PRINT_DRP("[DRP][xctr:%2d][W]: fwd packet\n", xctr);
          drp_to_rsg_write[xctr] << w_fwd_req[xctr].msg;              // ctrl pkt for RSG to track
          drp_to_ohd_write[xctr] << w_fwd_req[xctr];                  // control packet for OHD to track
          rqr_to_drp_write[xctr].read(nullptr);
        }
      }

      // READS
      sb_apkt_t r_fwd_req;
      r_fwd_req.msg = rqr_to_drp_read[xctr].peek(vld_rqr_r[xctr]);
      // 1. swap xcsr information with xctr information
      // 1.1 route this packet to relevant I/OHD
      r_fwd_req.tag = r_fwd_req.msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB);
      // 1.2 make the pageid field store the `xctr` value so that it can be routed back to RSG
      r_fwd_req.msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB) = xctr;                
      if(vld_rqr_r[xctr])
      {
        uint8_t msgs = r_fwd_req.msg.control.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB);  // TODO: doesn't look like `msgs` or `nb_fwd_req`  need to be created as an array because it is internal to the loop
        // DEBUG_PRINT_DRP("[DRP][xctr:%2d][R]: req.nmsgs:%2d\n", xctr, msgs);
        drp_to_ihd_read[xctr] << r_fwd_req;  // for data
        DEBUG_PRINT_DRP("[DRP][xctr:%2d][R]: fwd req --> IHD\n", xctr);
        drp_to_rsg_read[xctr] << r_fwd_req.msg;  // for RSG to track
        DEBUG_PRINT_DRP("[DRP][xctr:%2d][R]: fwd req --> RSG\n", xctr);
        r_fwd_req.msg = rqr_to_drp_read[xctr].read();
      }
    }
  }
}
