#ifdef DEBUG_IHD
#define DEBUG_PRINT_IHD(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_IHD(...)
#endif


/**
 * Task     : I/OHD
 * Purpose  : The IOHD is responsible for the grunt work of the data transfer
 *              related to the requests. The input and output streams
 *              from RQP and to RSG will later be widened based on NRX and NTX,
 *              allowing multiple requests to be parsed in parallel.
 *              THIS PATH MUST BE OPTIMISED FOR PERFORMANCE.
 *
*/
void ihd(int seq,
        tapa::istream<sb_apkt_t>& rai_to_ihd_read,
        tapa::ostream<sb_apkt_t>& ihd_to_rao_read,
        tapa::ibuffers<sb_hmsg_t[SB_WORDS_PER_PAGE], (SB_PAGES_PER_XCSR), 1, tapa::array_partition<tapa::block<SB_NUM_PARTITIONS>>, tapa::memcore<tapa::bram>>& backend_pages) {

  bool burst_done=true, rsp_done=true, valid=false, xfer_ctrl_valid=false;
  //#ifndef __SYNTHESIS__
  sb_dmsg_t msgdata = {0};
  //#endif
  sb_portid_t xctrid;
  uint8_t pageid, nmsgs, msgs_txed, last_pageid = 0xFF;
  uint16_t start_index = 0;
  sb_apkt_t req, rsp;
  uint8_t code;
  bool last_buffer_released = true, pause_read_for_buffer_release = false, written = true;

  IHD_MAIN: for(;;)
  {
    #pragma HLS latency max=0
    if(written)
    {
      valid = rai_to_ihd_read.try_read(req);
    }

    //DEBUG_PRINT("[IHD][xctr:%2d][R]: Repeat %d %d %d %d\n", xctr, valid[xctr], req[xctr].c_dn, burst_done[xctr], rsp_done[xctr]);
    if(valid)
    {
      code         = req.msg.control.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB);       // get the code
      nmsgs        = req.msg.control.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB);   // get number of messages to read
      start_index  = req.msg.control.range(SB_CMSG_ADDR_MSB, SB_CMSG_ADDR_LSB);       // get starting index of where to read from
      xctrid       = req.msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB);       // extract the xctrid from pageid field
      pageid       = req.msg.control.range(SB_CMSG_PAGE_MSB, SB_CMSG_PAGE_LSB);       // store pageid to access
      // DEBUG_PRINT_IHD("[IHD][xcsr:%2d][R]: xctr: %d, addr: %d, nmsgs: %d\n", seq, req.tag, start_index, nmsgs);
      rsp.msg.control = req.msg.control;                // store the req ctrl pkt in the rsp ctrl pkt, modify it along the way
      // printf("[IHD][xcsr:%2d][D] pageid : %d\n", seq, pageid);
      assert(pageid < SB_PAGES_PER_XCSR);

      if(last_buffer_released)
      {
        DEBUG_PRINT_IHD_BUFF("[IHD][xcsr:%2d][T]: Acquiring Buffer\n", seq);
        auto section = backend_pages[pageid].create_section();
        backend_pages[pageid].acquire(section);
        last_pageid = pageid;
        last_buffer_released = false;
        written = false;
      }
      else
      {
        // get the pageref
        auto& page_ref = (backend_pages[pageid].create_section())();
        // rsp.msg.std_msg = page_ref[msgs_txed];
        // prefetch stuff into the 0th index
        uint16_t access_index = 2*start_index;
        for(uint16_t i = 0; i < SB_NUM_PARTITIONS; i++)
        {
          #pragma HLS unroll
          sb_hmsg_t var0 = page_ref[i*(SB_WORDS_PER_PAGE/SB_NUM_PARTITIONS) + access_index+0];
          sb_hmsg_t var1 = page_ref[i*(SB_WORDS_PER_PAGE/SB_NUM_PARTITIONS) + access_index+1];
          msgdata.msg[i] = ((sb_msg_t)var1 << SB_HMSG_W) | var0;
        }

        // set code
        rsp.msg.control.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = SB_RSP_DONE | code;
        // patch back the index of the xcsr
        rsp.msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB) = seq;
        // update tag with the xctrid for rerouting back to RSG
        rsp.tag         = xctrid;
        // try writing this data
        rsp.msg.std_msg = msgdata;
        written = ihd_to_rao_read.try_write(rsp);
        // DEBUG_PRINT_IHD("[IHD][xcsr:%2d][R]: Sent message (%d): %lx %x %x\n", seq, msgs_txed, (uint64_t)msgdata[prefetch_index], (uint32_t)var1[prefetch_index], (uint32_t)var0[prefetch_index]);

        if(written)
        {
          // sb_apkt_t nc;
          // rai_to_ihd_read.try_read(nc); // this read will always succeed since there must be data to reach, we just want to consume the token with II=1
          // last_token_consumed = true;
          DEBUG_PRINT_IHD("[IHD][xcsr:%2d][R]: xctr: %d, pageid: %d, addr: %d\n", seq, xctrid, pageid, start_index);
          // DEBUG_PRINT_IHD("[IHD][xcsr:%2d][R]: Message: %lx\n", seq, (uint64_t)msgdata.msg[0]);
          // DEBUG_PRINT_IHD("[IHD][xcsr:%2d][R]: Message: %lx %x %x\n", seq, (uint64_t)msgdata.msg[0], page_ref[2*(start_index + msgs_rxed)], page_ref[2*(start_index + msgs_rxed)+1]);
        }

        if(code & SB_RW_CONT_MASK)          // if this is a continued transaction
        {                                   // request control again
          // DEBUG_PRINT_IHD("[IHD][xcsr:%2d][T]: Continued-read flag set\n", seq);
          last_buffer_released = false;
        } else {                            // else release buffer
          DEBUG_PRINT_IHD_BUFF("[IHD][xcsr:%2d][T]: Releasing Buffer\n", seq);
          (backend_pages[pageid].create_section()).release_section();
          last_buffer_released = true;
        }
      }
    }
  }
}
