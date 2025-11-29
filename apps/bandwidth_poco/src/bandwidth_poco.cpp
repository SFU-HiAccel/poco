#include <mpmc_buffer.h>
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

using mybuf = tapa::mpmcbuffer<sb_hmsg_t[32], tapa::array_partition<tapa::block<1>>, tapa::memcore<tapa::bram>, tapa::blocks<4>, tapa::pages<8>>;

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
            tapa::mpmc<mybuf, 2> sb,
            tapa::istreams<bool, SB_NXCTRS>& task2_to_task1,
            tapa::ostream<bool>& trigger_timer)
{

  io_section:{
    #pragma HLS protocol fixed

    // Start timer
    trigger_timer << 1;
    
    DEBUG_PRINT("[TASK1][W]: Writing...\n");
    
    uint64_t data = values.read();
    
    T1_WXCTR_LOOP: for(uint8_t xctr = 0; xctr < xctr_looper; xctr++)
    {
      #pragma HLS unroll
      // DEBUG_PRINT("[TASK1][%d]: Iteration %d; Page %2d; Data %3d\n", xctr, i, page, d);
      sb_pageid_t pageid = {0};
      sb_dmsg_t dmsg = {{0xdeadbeef}};
      sb_rsp_t rsp;
      pageid.range(SB_PAGEID_PAGE_MSB, SB_PAGEID_PAGE_LSB) = 0;
      pageid.range(SB_PAGEID_XCXR_MSB, SB_PAGEID_XCXR_LSB) = xctr;  // xctr:xcsr mapping
      sb_rsp_t wrsp = sb[0].do_write(dmsg, pageid, xctr, 1, false);
    }
    trigger_timer << 1;
  } // io_section
} // task1

void task2( tapa::mpmc<mybuf, 2> sb,
            tapa::ostreams<bool, SB_NXCTRS>& task2_to_task1)
{
  uint32_t counter[SB_NXCTRS] = {0};
  #pragma HLS array_partition variable=counter type=complete
  T2_XCTR_LOOP: for(uint8_t xctr = 0; xctr < SB_NXCTRS; xctr++)
  {
    #pragma HLS unroll
    #pragma HLS latency max=0
    sb_pageid_t pageid = {0};
    pageid.range(SB_PAGEID_PAGE_MSB, SB_PAGEID_PAGE_LSB) = 0;
    pageid.range(SB_PAGEID_XCXR_MSB, SB_PAGEID_XCXR_LSB) = xctr;  // xctr:xcsr mapping
    sb_rsp_t data_rxsb = sb[0].do_read(pageid, xctr, 1, false);
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

void bandwidth( tapa::mmap<const uint64_t> ivector_values,
                tapa::mmap<uint64_t> ovector_timer,
                int dummy) {
  
  tapa::stream<uint64_t> values("values");
  tapa::stream<uint64_t> timer("timer");
  tapa::stream<bool> trigger_timer("trigger_timer");
  tapa::streams<bool, SB_NXCTRS> task2_to_task1("task2_to_task1");

  mybuf sb;
  tapa::task()
    .invoke<tapa::detach>(Mmap2Stream64, ivector_values, values, SB_MSGS_PER_PAGE*SB_NUM_PAGES/SB_NXCSRS)
    .invoke(task1, values, sb, task2_to_task1, trigger_timer)
    .invoke<tapa::detach>(task2, sb, task2_to_task1)
    .invoke<tapa::detach>(tick_timer, trigger_timer, ovector_timer);
}

///////////////////////////////////////////////////////////////////////////////
