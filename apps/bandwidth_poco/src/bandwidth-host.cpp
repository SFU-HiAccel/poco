#include <iostream>
#include <vector>
#include <cstdlib>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>
#include <algorithm>


#include <gflags/gflags.h>
#include <tapa.h>

#include "bandwidth.h"

template <typename T>
using aligned_vector = std::vector<T, tapa::aligned_allocator<T>>;

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char *argv[]) {

  std::vector<uint64_t> values;

  gflags::ParseCommandLineFlags(&argc, &argv, false);

  int len_values = SB_MSGS_PER_PAGE*SB_NUM_PAGES/SB_NXCSRS;

  aligned_vector<uint64_t> array_values0(len_values);
  aligned_vector<uint64_t> array_values1(len_values);
  aligned_vector<uint64_t> array_values2(len_values);
  aligned_vector<uint64_t> array_values3(len_values);
  aligned_vector<uint64_t> array_values4(len_values);
  aligned_vector<uint64_t> array_values5(len_values);
  aligned_vector<uint64_t> array_values6(len_values);
  aligned_vector<uint64_t> array_values7(len_values);
  aligned_vector<uint64_t> vector_dummy(32);
  aligned_vector<uint64_t> timer_fpga(512);

  // data initialisation
  srand(0);
  for (int i = 0; i < len_values; i++)
  {
    array_values0[i] = static_cast<uint64_t>(rand());
    array_values1[i] = static_cast<uint64_t>(rand());
    array_values2[i] = static_cast<uint64_t>(rand());
    array_values3[i] = static_cast<uint64_t>(rand());
    array_values4[i] = static_cast<uint64_t>(rand());
    array_values5[i] = static_cast<uint64_t>(rand());
    array_values6[i] = static_cast<uint64_t>(rand());
    array_values7[i] = static_cast<uint64_t>(rand());
  }

  printf("[HOST]: SB_NUM_PAGES      : %d\n", SB_NUM_PAGES);
  printf("[HOST]: SB_NXCSRS         : %d\n", SB_NXCSRS);
  printf("[HOST]: SB_MSGS_PER_PAGE  : %d\n", SB_MSGS_PER_PAGE);
  printf("[HOST]: SB_WORDS_PER_PAGE : %d\n", SB_WORDS_PER_PAGE);
  printf("[HOST]: SB_MSG_SIZE       : %d bytes\n", SB_MSG_SIZE);
  printf("[HOST]: SB_PAGE_SIZE      : %d bytes\n", SB_PAGE_SIZE);

  int64_t kernel_time_us = tapa::invoke(bandwidth, FLAGS_bitstream,
    // tapa::read_only_mmap<uint64_t>(array_values0).reinterpret<const ap_uint<512>>(),
    // tapa::read_only_mmap<uint64_t>(array_values1).reinterpret<const ap_uint<512>>(),
    // tapa::read_only_mmap<uint64_t>(array_values2).reinterpret<const ap_uint<512>>(),
    // tapa::read_only_mmap<uint64_t>(array_values3).reinterpret<const ap_uint<512>>(),
    // tapa::read_only_mmap<uint64_t>(array_values4).reinterpret<const ap_uint<512>>(),
    // tapa::read_only_mmap<uint64_t>(array_values5).reinterpret<const ap_uint<512>>(),
    // tapa::read_only_mmap<uint64_t>(array_values6).reinterpret<const ap_uint<512>>(),
    // tapa::read_only_mmap<uint64_t>(array_values7).reinterpret<const ap_uint<512>>(),
    tapa::read_only_mmap<const uint64_t>(array_values0),
    tapa::write_only_mmap<uint64_t>(timer_fpga),
    1);

  for (int i = 0; i < 10; i++) {
    printf("[HOST]: Timer[%d]: %ld\n", i, timer_fpga[i]);
  }
  std::cout << "[HOST]: Done!" << std::endl;
  return 0;
}
