#ifdef DEBUG_OHD
#define DEBUG_PRINT_OHD(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_OHD(...)
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
void ohd(int seq,
        tapa::istream<sb_apkt_t>& wai_to_ohd_write,
        tapa::ostream<sb_apkt_t>& ohd_to_wao_write,
        tapa::obuffers<sb_hmsg_t[SB_WORDS_PER_PAGE], (SB_PAGES_PER_XCSR), 1, tapa::array_partition<tapa::block<SB_NUM_PARTITIONS>>, tapa::memcore<tapa::bram>>& backend_pages) {
  
  bool valid, xfer_ctrl_valid;
  bool burst_done = true, rsp_done = true, err_state = false;
  sb_dmsg_t msgdata = {0};
  sb_portid_t xctrid;
  uint8_t nmsgs, pageid, msgs_rxed, last_pageid = 0xFF;
  uint16_t start_index = 0;
  sb_apkt_t req, rsp;
  uint8_t code;
  bool last_buffer_released = true;
  
  OHD_MAIN: for(;;)
  {
    #pragma HLS latency max=0
    valid = wai_to_ohd_write.try_peek(req);   // peek the request stream

    if(valid)                                 // this must be a new request
    {
      code        = req.msg.control.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB);       // get the code
      nmsgs       = req.msg.control.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB);   // get number of messages to write
      start_index = req.msg.control.range(SB_CMSG_ADDR_MSB, SB_CMSG_ADDR_LSB);       // get starting index of where to write to
      xctrid      = req.msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB);       // get the xctrid from xcxr field
      pageid      = req.msg.control.range(SB_CMSG_PAGE_MSB, SB_CMSG_PAGE_LSB);       // get the page index
      err_state   = (last_buffer_released) ? 0 : (pageid != last_pageid);
      rsp.msg.control = req.msg.control;                // store the req ctrl pkt in the rsp ctrl pkt, modify it along the way
      assert(pageid < SB_PAGES_PER_XCSR);

      if(last_buffer_released)
      {
        DEBUG_PRINT_OHD_BUFF("[OHD][xcsr:%2d][T]: Acquiring Buffer\n", seq);
        auto section = backend_pages[pageid].create_section();
        backend_pages[pageid].acquire(section);
        last_pageid = pageid;
        last_buffer_released = false;
      }
      else
      {
        auto& page_ref = (backend_pages[pageid].create_section())();
        msgdata = req.msg.std_msg;            // extract the message
        uint16_t access_index = 2*start_index;
        // write it in the buffer
        for(uint16_t i = 0; i < SB_NUM_PARTITIONS; i++)
        {
          #pragma HLS unroll
          #pragma HLS dependence variable=page_ref dependent=false type=intra direction=waw
          #pragma HLS dependence variable=page_ref dependent=false type=inter direction=waw
          page_ref[i*(SB_WORDS_PER_PAGE/SB_NUM_PARTITIONS) + access_index+0] = (sb_hmsg_t)(msgdata.msg[i] & 0xFFFFFFFF);  // port 1
          page_ref[i*(SB_WORDS_PER_PAGE/SB_NUM_PARTITIONS) + access_index+1] = (sb_hmsg_t)(msgdata.msg[i] >> SB_HMSG_W);  // port 2          
        }

        // set code
        rsp.msg.control.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = SB_RSP_DONE | code;
        // patch back the index of the xcsr
        rsp.msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB) = seq;
        // update tag with the xctrid extracted in the burst-prep phase above
        rsp.tag       = xctrid;
        // try to send response
        bool written  = ohd_to_wao_write.try_write(rsp);

        if(written)
        {
          sb_apkt_t nc;
          wai_to_ohd_write.try_read(nc);
          DEBUG_PRINT_OHD("[OHD][xcsr:%2d][W]: xctr: %d, pageid: %d, addr: %d\n", seq, xctrid, pageid, start_index);
          // DEBUG_PRINT_OHD("[OHD][xcsr:%2d][W]: Message: %lx\n", seq, (uint64_t)msgdata.msg[0]);
          // DEBUG_PRINT_OHD("[OHD][xcsr:%2d][W]: Message: %lx %x %x\n", seq, (uint64_t)msgdata.msg[0], page_ref[2*(start_index + msgs_rxed)], page_ref[2*(start_index + msgs_rxed)+1]);
        }

        if(code & SB_RW_CONT_MASK)            // if this is a continued transaction
        {                                     // don't release buffer again
          // DEBUG_PRINT_OHD("[OHD][xcsr:%2d][T]: Continued-write flag set\n", seq);
          last_buffer_released = false;
        } else {                              // else release buffer
          DEBUG_PRINT_OHD_BUFF("[OHD][xcsr:%2d][T]: Releasing Buffer\n", seq);
          (backend_pages[pageid].create_section()).release_section();
          last_buffer_released = true;
        }
      }
    }
    else {
      #ifndef __SYNTHESIS__
      if(valid && !ohd_to_wao_write.full())
      {
        DEBUG_PRINT_OHD("[OHD][xcsr:%2d][W]: Bad request: burst_done:%d, rsp_done:%d\n", seq, burst_done, rsp_done);
        DEBUG_PRINT_OHD("[OHD][xcsr:%2d][X]: req.c_dn         = %d\n", seq, (int)req.msg.c_dn);
        DEBUG_PRINT_OHD("[OHD][xcsr:%2d][X]: req.fields.code  = %x\n", seq, req.msg.control.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB));
        DEBUG_PRINT_OHD("[OHD][xcsr:%2d][X]: req.page         = %d\n", seq, req.msg.control.range(SB_CMSG_PAGE_MSB, SB_CMSG_PAGE_LSB));
        DEBUG_PRINT_OHD("[OHD][xcsr:%2d][X]: req.xctr         = %d\n", seq, req.msg.control.range(SB_CMSG_XCXR_MSB, SB_CMSG_XCXR_LSB));
        DEBUG_PRINT_OHD("[OHD][xcsr:%2d][X]: req.std_msg      = %lx\n", seq, req.msg.std_msg);
        assert(false);  // must never encounter this scenario
      }
      #endif  // __SYNTHESIS__
    }
  }
}
