// #include <mpmc_buffer.h>
#include <tapa.h>
#include "bandwidth.h"
#include "sb_config.h"
#include "brahma.h"


// using tapa::cyclic;
// using tapa::block;
// using tapa::complete;
// using tapa::array_partition;
// using tapa::memcore;
// using tapa::blocks;
// using tapa::pages;
// using tapa::bram;

// using mybuf = tapa::mpmcbuffer<sb_hmsg_t[32], tapa::array_partition<tapa::block<1>>, tapa::memcore<tapa::bram>, tapa::blocks<4>, tapa::pages<8>>;

/////////////////////////////////
/// MMAPS
/////////////////////////////////


void Mmap2Stream64(tapa::mmap<const uint64_t> mmap,
                 tapa::ostream<uint64_t>& stream,
                 uint32_t num_elements) {
  for (uint32_t i = 0; i < num_elements; ++i) {
    stream << mmap[i];
  }
}

void Mmap2Stream32(tapa::mmap<const uint32_t> mmap,
                 tapa::ostream<uint32_t>& stream,
                 uint32_t num_elements) {
  for (uint32_t i = 0; i < num_elements; ++i) {
    stream << mmap[i];
  }
}

void Stream2Mmap(tapa::istream<uint32_t>& stream,
                  tapa::mmap<uint32_t> mmap) {
  for (uint64_t i = 0; i < 1; ++i) {
    stream >> mmap[i];
  }
}

void Stream2MmapTimer(tapa::istream<uint64_t>& stream,
                  tapa::mmap<uint64_t> mmap) {
  for (uint64_t i = 0; i < 4; ++i) {
    stream >> mmap[i];
  }
}

/////////////////////////////////
/// TASKS
/////////////////////////////////

const uint32_t iter_looper = 1;
const uint32_t page_looper = SB_NUM_PAGES/SB_NXCSRS;
const uint32_t data_looper = 1;
const uint32_t xctr_looper = SB_NXCTRS;

void task1( tapa::istream<uint64_t>& values,
            tapa::istreams<sb_rsp_t, 2>& sb_rx, tapa::ostreams<sb_req_t, 2>& sb_tx,
            tapa::istreams<bool, SB_NXCTRS>& task2_to_task1,
            tapa::ostream<bool>& trigger_timer)
{

  io_section:{
    #pragma HLS protocol fixed

    // Start timer
    trigger_timer << 1;
    
    DEBUG_PRINT("[TASK1][W]: Writing...\n");
    
    uint64_t data = values.read();
    
    T1_WXCTR_LOOP: for(uint8_t data = 0; data < 8; data++) {
      REQ_LOOP_W0: for(uint8_t xctr = 0; xctr < 2; xctr++) {
      
      #pragma HLS unroll
      // DEBUG_PRINT("[TASK1][%d]: Iteration %d; Page %2d; Data %3d\n", xctr, i, page, d);
      sb_pageid_t pageid = {0};
      sb_dmsg_t dmsg = {{0xdeadbeef}};
      sb_rsp_t rsp;
      pageid.range(SB_PAGEID_PAGE_MSB, SB_PAGEID_PAGE_LSB) = 0;
      pageid.range(SB_PAGEID_XCXR_MSB, SB_PAGEID_XCXR_LSB) = xctr;  // xctr:xcsr mapping
      
        sb_req_t _req_write_0 = sb_request_write(dmsg, pageid, xctr, 1, data!=7);
        sb_tx[0].write(_req_write_0);
        // DEBUG_PRINT("[TASK1][W]: %d %d\n", data, xctr);
      }
      RSP_LOOP_W0: for(uint8_t xctr = 0; xctr < 2; xctr++) {
        sb_rsp_t wrsp = sb_rx[0].read();
      }
    }

    trigger_timer << 1;
  } // io_section
} // task1

void task2( tapa::istreams<sb_rsp_t, 2>& sb_rx, tapa::ostreams<sb_req_t, 2>& sb_tx,
            tapa::ostreams<bool, SB_NXCTRS>& task2_to_task1)
{
  uint32_t counter[SB_NXCTRS] = {0};
  #pragma HLS array_partition variable=counter type=complete
  T2_XCTR_LOOP: for(uint8_t data = 0; data < 8; data++) {
    REQ_LOOP_R1: for(uint8_t xctr = 0; xctr < 2; xctr++) {
    
    #pragma HLS unroll
    #pragma HLS latency max=0
    sb_pageid_t pageid = {0};
    pageid.range(SB_PAGEID_PAGE_MSB, SB_PAGEID_PAGE_LSB) = 0;
    pageid.range(SB_PAGEID_XCXR_MSB, SB_PAGEID_XCXR_LSB) = xctr;  // xctr:xcsr mapping
    
      sb_req_t _req_read_1 = sb_request_read(pageid, xctr, 1, data!=7);
      sb_tx[0].write(_req_read_1);
    }
    RSP_LOOP_R1: for(uint8_t xctr = 0; xctr < 2; xctr++) {
      sb_rsp_t data_rxsb = sb_rx[0].read();
    }
  }
}

void tick_timer(tapa::istream<bool>& rx_trigger,
                tapa::mmap<uint64_t> tx_timer)
{
  uint64_t time = 0;
  uint8_t index = 0;
  bool read_val, vld;
  for(;;)
  {
    vld = rx_trigger.try_read(read_val);
    if(vld)
    {
      tx_timer[index] = time;
      index++;
    }
    time++;
  }
}


//////////////////////
/// KERNEL WRAPPER ///
//////////////////////
// void bandwidth( tapa::mmap<const ap_uint<512>> ivector_values0,
//   tapa::mmap<const ap_uint<512>> ivector_values1,
//   tapa::mmap<const ap_uint<512>> ivector_values2,
//   tapa::mmap<const ap_uint<512>> ivector_values3,
//   tapa::mmap<const ap_uint<512>> ivector_values4,
//   tapa::mmap<const ap_uint<512>> ivector_values5,
//   tapa::mmap<const ap_uint<512>> ivector_values6,
//   tapa::mmap<const ap_uint<512>> ivector_values7,
//   tapa::mmap<uint64_t> ovector_timer,
//   tapa::mmap<uint64_t> ovector_dummy,
//   int dummy) {

//////////////////////////////////////
/// BRAHMA: TASKS
//////////////////////////////////////

constexpr int kN = SB_NXCSRS;  // kN x kN network
constexpr int kStageCount = SB_NXCSRS_LOG2;

#ifndef SB_ARBITER_PIPE_DEPTH
#define SB_ARBITER_PIPE_DEPTH (1)
#endif

// switch for page-->xctr arbitration
void Switch2x2(int b, tapa::istream<sb_apkt_t>& pkt_in_q0, tapa::istream<sb_apkt_t>& pkt_in_q1,
               tapa::ostreams<sb_apkt_t, 2>& pkt_out_q) {
  uint8_t priority = 0;

  b = kStageCount - 1 - b;

  sb_apkt_t pipe0[SB_ARBITER_PIPE_DEPTH];
  sb_apkt_t pipe1[SB_ARBITER_PIPE_DEPTH];
  
  for (bool valid_0, valid_1;;) {
    // enable the pipe here, cycling data, latency should be at the same level
    #pragma HLS latency max = 0
    // pipe0[0] = pkt_in_q0.peek(valid_0);
    // pipe1[0] = pkt_in_q1.peek(valid_1);
    // in_0 = pipe0[SB_ARBITER_PIPE_DEPTH-1];
    // in_1 = pipe1[SB_ARBITER_PIPE_DEPTH-1];
    sb_apkt_t in_0 = pkt_in_q0.peek(valid_0);
    sb_apkt_t in_1 = pkt_in_q1.peek(valid_1);
    auto pkt_0 = in_0.tag;
    auto pkt_1 = in_1.tag;
    bool fwd_0_0 = valid_0 && (pkt_0 & (1 << b)) == 0;
    bool fwd_0_1 = valid_0 && (pkt_0 & (1 << b)) != 0;
    bool fwd_1_0 = valid_1 && (pkt_1 & (1 << b)) == 0;
    bool fwd_1_1 = valid_1 && (pkt_1 & (1 << b)) != 0;

    bool conflict =
        valid_0 && valid_1 && fwd_0_0 == fwd_1_0 && fwd_0_1 == fwd_1_1;
    bool prioritize_1 = priority & 1;

    bool read_0 = !((!fwd_0_0 && !fwd_0_1) || (prioritize_1 && conflict));
    bool read_1 = !((!fwd_1_0 && !fwd_1_1) || (!prioritize_1 && conflict));
    bool write_0 = fwd_0_0 || fwd_1_0;
    bool write_1 = fwd_1_1 || fwd_0_1;
    bool write_0_0 = fwd_0_0 && (!fwd_1_0 || !prioritize_1);
    bool write_1_1 = fwd_1_1 && (!fwd_0_1 || prioritize_1);

    // if can forward through (0->0 or 1->1), do it
    // otherwise, check for conflict
    const bool written_0 =
        write_0 && pkt_out_q[0].try_write(write_0_0 ? in_0 : in_1);
    const bool written_1 =
        write_1 && pkt_out_q[1].try_write(write_1_1 ? in_1 : in_0);

    // if can forward through (0->0 or 1->1), do it
    // otherwise, round robin priority of both ins
    if (read_0 && (write_0_0 ? written_0 : written_1)) {
      pipe0[0] = pkt_in_q0.read(nullptr);
    }
    if (read_1 && (write_1_1 ? written_1 : written_0)) {
      pipe1[0] = pkt_in_q1.read(nullptr);
    }

    if (conflict) ++priority;
  }
}

void InnerStage(int b,
                tapa::istreams<sb_apkt_t, kN / 2>& in_q0,
                tapa::istreams<sb_apkt_t, kN / 2>& in_q1,
                tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
}

void rai(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}

void rao(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}

void wai(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}

void wao(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}

// Control Request Parser
#ifdef DEBUG_CRP
#define DEBUG_PRINT_CRP(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_CRP(...)
#endif

void crp( tapa::istream<sb_cmsg_t>& pgm_to_crp_sts,
          tapa::ostream<sb_cmsg_t>& crp_to_pgm_free,
          tapa::ostream<sb_cmsg_t>& crp_to_pgm_grab,
          tapa::ostream<sb_cmsg_t>& crp_to_rsg_free,
          tapa::ostream<sb_cmsg_t>& crp_to_rsg_grab,
          tapa::istreams<sb_cmsg_t, SB_NXCTRS>& rqr_to_crp_free,
          tapa::istreams<sb_cmsg_t, SB_NXCTRS>& rqr_to_crp_grab) {

  sb_portid_t xctr = 0;
  bool vld_free, vld_grab, vld_pgm_sts;
  sb_cmsg_t fwd_free, fwd_grab, pgm_rsp;
  for(;;)
  {
    fwd_free = rqr_to_crp_free[xctr].peek(vld_free);
    fwd_grab = rqr_to_crp_grab[xctr].peek(vld_grab);
    pgm_rsp = pgm_to_crp_sts.peek(vld_pgm_sts);
    if(vld_pgm_sts)
    {
      // read code and compare the lower 4 bits to check request type
      if((pgm_rsp.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) & 0xF) == SB_REQ_FREE_PAGE)
      {
        crp_to_rsg_free << pgm_to_crp_sts.read();
        DEBUG_PRINT_CRP("[CRP][xctr:%2d][F]: fwd rsp --> RSG\n", pgm_rsp.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int());
      }
      else if((pgm_rsp.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) & 0xF) == SB_REQ_GRAB_PAGE)
      {
        crp_to_rsg_grab << pgm_to_crp_sts.read();
        DEBUG_PRINT_CRP("[CRP][xctr:%2d][G]: fwd rsp --> RSG\n", pgm_rsp.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int());
      }
    }
    else if(vld_free)
    {
      // free request; pageid: index of page; length: xctr
      fwd_free.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB) = (sb_pageid_t)xctr;   // write xctr index into the index field
      bool w_vld_free = crp_to_pgm_free.try_write(fwd_free);
      if(w_vld_free)
      {
        DEBUG_PRINT_CRP("[CRP][xctr:%2d][F]: fwd req --> PGM\n", fwd_free.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int());
        rqr_to_crp_free[xctr].read();
      }
    }
    else if(vld_grab)
    {
      // grab request; pageid: number of pages; index of page; length: xctr
      fwd_grab.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB) = (sb_pageid_t)xctr;   // write xctr index into the index field
      bool w_vld_grab = crp_to_pgm_grab.try_write(fwd_grab);
      if(w_vld_grab)
      {
        DEBUG_PRINT_CRP("[CRP][xctr:%2d][G]: fwd req --> PGM\n", fwd_grab.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int());
        rqr_to_crp_grab[xctr].read();
      }
    }

    // wrap counter from 0 to (SB_NXCTRS-1)
    xctr = ((xctr+1) == SB_NXCTRS) ? 0 : xctr+1;
  } 
}


void debug_task(tapa::istreams<sb_msg_t, SB_NDEBUGQS>& debugstreams)
{
  sb_msg_t msg[SB_NDEBUGQS] = {0};
  for(bool valid[SB_NDEBUGQS];;)
  {
    for (int i = 0; i < SB_NDEBUGQS; i++)
    {
      msg[i] = debugstreams[i].peek(valid[i]);
      if(valid[i])
      {
        debugstreams[i].read();
      }
    }
  }
}


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

#ifdef DEBUG_PGM
#define DEBUG_PRINT_PGM(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_PGM(...)
#endif

// Function to find the index of the first 0 bit in a number
inline uint8_t find_first_zero_bit_index(uint8_t num) {
  for (uint8_t i = 0; i < 8; i++) {
    if (!(num & (1 << i))) {
      return i;
    }
  }
  return 0xFF;  // error code
}

uint8_t byte_lut[255] = {
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 
0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,};

/**
 * Task     : Page Manager
 * Purpose  : The page-manager is responsible for maintaining all information
 *              related to the page allocation/deallocations
 *              THIS PATH IS NOT OPTIMISED FOR PERFORMANCE.
 *
*/
void pgm(tapa::istream<sb_cmsg_t>& rqp_to_pgm_grab,
        tapa::istream<sb_cmsg_t>& rqp_to_pgm_free,
        tapa::ostream<sb_cmsg_t>& pgm_to_rqp_sts) {

  // sb_metadata_t metadata[SB_NUM_PAGES] = {0};
  uint8_t valid[(SB_NUM_PAGES>>3) ? (SB_NUM_PAGES>>3) : 8] = {0};

  // sb_metadata_t free_md, grab_md;
  uint16_t free_vld8_pre, free_vld8_new, free_pageid_in8;
  uint16_t grab_vld8_pre, grab_vld8_new, grab_pageid_in8, grab_avl_idx;
  bool grab_avl;
  sb_cmsg_t fwd_rsp_f, fwd_rsp_g, rsp;
  uint16_t grab_pageid;
  uint16_t free_pageid;

  // initialise lookup array
  uint8_t avl_page_lut[255] = {0};
  for (uint8_t i = 0; i < 255; i++)
  {
    avl_page_lut[i] = find_first_zero_bit_index(i);
  }

  for(bool vld_g, vld_f;;)
  {
    // clear existing response
    // rsp.c_dn = 1;
    rsp = 0;
    fwd_rsp_f = rqp_to_pgm_free.peek(vld_f);
    fwd_rsp_g = rqp_to_pgm_grab.peek(vld_g);
    if(vld_f)       // handle page deallocation
    {
      DEBUG_PRINT_PGM("[PGM][xctr:%2d][F]: pageid %d\n", fwd_rsp_f.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int(), fwd_rsp_f.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int());
      
      // update page info
      free_pageid     = fwd_rsp_f.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB);
      free_vld8_pre   = valid[free_pageid>>3];                      // get the 8-bit valid byte
      free_pageid_in8 = free_pageid & 0x7;                          // get the 3LSBs from `pageid`
      free_vld8_new   = free_vld8_pre & ~(1 << free_pageid_in8);    // unset this specific bit
      valid[free_pageid>>3] = free_vld8_new;

      // send response
      rsp.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB)   = SB_RSP_DONE | SB_REQ_FREE_PAGE;
      rsp.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB)  = fwd_rsp_f.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB);
      pgm_to_rqp_sts << rsp;

      rqp_to_pgm_free.read(); // consume the token

      DEBUG_PRINT_PGM("[PGM][xctr:%2d][F]: fwd rsp --> RQP\n", rsp.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int());
    }

    else if(vld_g)  // handle page allocation
    {
      DEBUG_PRINT_PGM("[PGM][xctr:%2d][G]: %d page(s)\n", fwd_rsp_g.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB).to_int(), fwd_rsp_g.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB).to_int());

      grab_avl = false;
      // loop around all bytes and find the available index
      PGM_G_BIN_SEARCH: for(uint16_t i = 0; i < (SB_NUM_PAGES>>3); i++) {
        if(valid[i] != 0xFF) {
          grab_avl_idx = i;
          grab_avl = true;
          break;
        }
      }
      if(grab_avl)
      {
        // update page info
        grab_vld8_pre   = valid[grab_avl_idx];                        // get the 8-bit valid byte
        grab_pageid_in8 = avl_page_lut[grab_vld8_pre];                // find a page which is keeping the bin empty
        grab_vld8_new   = grab_vld8_pre | (1 << grab_pageid_in8);     // set this specific bit
        valid[grab_avl_idx] = grab_vld8_new;

        grab_pageid     = (grab_avl_idx << 3) | grab_pageid_in8;      // form the pageid
        rsp.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = SB_RSP_DONE | SB_REQ_GRAB_PAGE;
        rsp.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB) = grab_pageid;
      }
      else
      {
        // generate the response
        rsp.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = SB_RSP_FAIL | SB_REQ_GRAB_PAGE;
        // hardcoded to return bad pageid
        rsp.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB) = 0xFFFF;
      }

      // finalise grab request and send it
      rsp.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB) = fwd_rsp_g.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB);
      pgm_to_rqp_sts << rsp;
      // consume the token
      rqp_to_pgm_grab.read();

      DEBUG_PRINT_PGM("[PGM][xctr:%2d][G]: fwd rsp --> RQP\n", rsp.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB));
    }
    else {}
  }
}

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


void bandwidth( tapa::mmap<const uint64_t> ivector_values,
                tapa::mmap<uint64_t> ovector_timer,
                int dummy) {
  
  tapa::stream<uint64_t> values("values");
  tapa::stream<uint64_t> timer("timer");
  tapa::stream<bool> trigger_timer("trigger_timer");
  tapa::streams<bool, SB_NXCTRS> task2_to_task1("task2_to_task1");

  // mybuf sb;
  constexpr int expr_sb_num_pages = SB_NUM_PAGES;
  constexpr int expr_sb_num_xcsrs = SB_NXCSRS;
  
  tapa::streams<sb_req_t, SB_NXCTRS, SB_BURST_SIZE> sb_rx("sb_rxqs");
  tapa::streams<sb_rsp_t, SB_NXCTRS, SB_BURST_SIZE> sb_tx("sb_txqs");
  
  tapa::streams<uint64_t, SB_NDEBUGQS> debugstreams("debug_streams");
  // RQR  <--->  RQP
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> rqr_to_drp_read("rqr_to_drp_read");
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> rqr_to_drp_write("rqr_to_drp_write");
  // RQR  <--->  CRA
  tapa::streams<sb_cmsg_t, SB_NXCTRS> rqr_to_crp_grab("rqr_to_crp_grab");
  tapa::streams<sb_cmsg_t, SB_NXCTRS> rqr_to_crp_free("rqr_to_crp_free");
  // CRP  <--->  RQP
  tapa::stream<sb_cmsg_t> crp_to_pgm_grab("crp_to_pgm_grab");
  tapa::stream<sb_cmsg_t> crp_to_pgm_free("crp_to_pgm_free");
  tapa::stream<sb_cmsg_t> pgm_to_crp_sts("pgm_to_crp_sts");
  // CRP  <--->  RSG
  tapa::stream<sb_cmsg_t> crp_to_rsg_grab("crp_to_rsg_grab");
  tapa::stream<sb_cmsg_t> crp_to_rsg_free("crp_to_rsg_free");
  
  /// PERFORMANCE CRITICAL STREAMS ///
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DRPTORSG_DEPTH> drp_to_rsg_read("drp_to_rsg_read");
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DRPTORSG_DEPTH> drp_to_rsg_write("drp_to_rsg_write");
  // RQP  <--->  RAI <---> IHD
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_RARBQS_DEPTH> rai_arbqs("rai_arbqs");
  // IHD  <--->  RAO <---> RSG
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_RARBQS_DEPTH> rao_arbqs("rao_arbqs");
  // RQP  <--->  WAI <---> OHD
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_WARBQS_DEPTH> wai_arbqs("wai_arbqs");
  // OHD  <--->  WAO <---> RSG
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_WARBQS_DEPTH> wao_arbqs("wao_arbqs");
  
  // backend pages
  tapa::buffers<ap_uint<256>[32], 8, 1, tapa::array_partition<tapa::block<1>>, tapa::memcore<tapa::bram>> backend_pages;

  tapa::task()
    .invoke<tapa::detach>(Mmap2Stream64, ivector_values, values, SB_MSGS_PER_PAGE*SB_NUM_PAGES/SB_NXCSRS)
    .invoke<tapa::detach>(rqr,
            sb_rx,
            rqr_to_crp_grab,
            rqr_to_crp_free,
            rqr_to_drp_read,
            rqr_to_drp_write)
    .invoke<tapa::detach>(crp,
            pgm_to_crp_sts,
            crp_to_pgm_free,
            crp_to_pgm_grab,
            crp_to_rsg_free,
            crp_to_rsg_grab,
            rqr_to_crp_free,
            rqr_to_crp_grab)
    .invoke<tapa::detach>(pgm,
            crp_to_pgm_grab,
            crp_to_pgm_free,
            pgm_to_crp_sts)
    .invoke<tapa::detach>(drp,
            rqr_to_drp_read,
            rqr_to_drp_write,
            drp_to_rsg_read,
            drp_to_rsg_write,
            rai_arbqs,
            wai_arbqs) 
    .invoke<tapa::detach, kStageCount>(rai, tapa::seq(), rai_arbqs, rai_arbqs)
    .invoke<tapa::detach, kStageCount>(wai, tapa::seq(), wai_arbqs, wai_arbqs)
    .invoke<tapa::detach, expr_sb_num_xcsrs>(ihd, tapa::seq(),
            rai_arbqs,
            rao_arbqs,
            backend_pages)
    .invoke<tapa::detach, expr_sb_num_xcsrs>(ohd, tapa::seq(),
            wai_arbqs,
            wao_arbqs,
            backend_pages)
    .invoke<tapa::detach, kStageCount>(rao, tapa::seq(), rao_arbqs, rao_arbqs)
    .invoke<tapa::detach, kStageCount>(wao, tapa::seq(), wao_arbqs, wao_arbqs)
    .invoke<tapa::detach>(rsg,
            crp_to_rsg_grab,
            crp_to_rsg_free,
            drp_to_rsg_read,
            drp_to_rsg_write,
            rao_arbqs,
            wao_arbqs,
            sb_tx)
    .invoke(task1, values, sb_tx, sb_rx, task2_to_task1, trigger_timer)
    .invoke<tapa::detach>(task2, sb_tx, sb_rx, task2_to_task1)
    .invoke<tapa::detach>(tick_timer, trigger_timer, ovector_timer);
}

///////////////////////////////////////////////////////////////////////////////
