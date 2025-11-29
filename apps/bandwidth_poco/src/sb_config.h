#ifndef __SB_CONFIG_H__
#define __SB_CONFIG_H__

#include "tapa.h"
#include "ap_int.h"

#define LOG2_1(n)   	(((n) >= 2) ? 1 : 0)
#define LOG2_2(n)   	(((n) >= 1<<2) ? (2 + LOG2_1((n)>>2)) : LOG2_1(n))
#define LOG2_4(n)   	(((n) >= 1<<4) ? (4 + LOG2_2((n)>>4)) : LOG2_2(n))
#define LOG2_8(n)   	(((n) >= 1<<8) ? (8 + LOG2_4((n)>>8)) : LOG2_4(n))
#define LOG2(n)     	(((n) >= 1<<16) ? (16 + LOG2_8((n)>>16)) : LOG2_8(n))
#define SIZE_BITS(n) 	(LOG2(n) + !!((n) & ((n) - 1)))

///////////////////
///  SB CONFIG  ///
///////////////////

// #define SB_NXCTRS           (4)
#define SB_NXCSRS           (SB_NXCTRS)
#define SB_NXCSRS_LOG2      (SIZE_BITS(SB_NXCSRS))
#define SB_NRX              (SB_NXCTRS)
#define SB_NTX              (SB_NXCTRS)
#define SB_NDEBUGQS         (2)
#define SB_BURST_SIZE       (32)
#define SB_XCTRQ_DEPTH      (4)
#define SB_DQS_DEPTH        (1)//SB_BURST_SIZE)
#define SB_CQS_DEPTH        (2)
#ifndef __SYNTHESIS__
  #define SB_DRPTORSG_DEPTH (32)
  #define SB_DRPTORSG_DEPTH (32)
#else
  #define SB_DRPTORSG_DEPTH (32)
  #define SB_DRPTORSG_DEPTH (32)
#endif
#define SB_RARBQS_DEPTH     (2)
#define SB_WARBQS_DEPTH     (2)
#define SB_NUNUSED          (4)

#define SB_NUM_PAGES        (4)
// #define SB_NUM_PARTITIONS   (8)
#define SB_PAGES_PER_XCSR   (1)
#define SB_MSG_SIZE         (64)       // bytes 4*HBM across 8 partitions = 4x64/8 = 32 bytes = 2048 bits
#define SB_WORD_SIZE        (32)       // bytes
#define SB_WORD_SIZE_BITS   (128)      // TODO: SBIF_WORD_SIZE << 3

#define SB_ADDRESSABLE_HEAP (1024*1024*1024)    // bytes
#define SB_NUM_PHYPAGES     (SB_ADDRESSABLE_HEAP/SB_PAGE_SIZE)
#define SB_TAG_FACTOR       (SB_NUM_PHYPAGES/SB_NUM_PAGES)
#define SB_TAG_SIZE_BITS    (SIZE_BITS(SB_TAG_FACTOR))

#endif // __SB_CONFIG_H__

