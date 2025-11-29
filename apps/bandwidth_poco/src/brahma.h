#ifndef __SB_H__
#define __SB_H__
#include <stdint.h>
#include <iostream>
#include "tapa.h"
#include "sb_config.h"
#include "sb_structs.h"
#include "sbif.h"

// used by PGM
extern uint8_t byte_lut[255];

// used to set pageids in metadata when that page is requested for the first time
sb_pageid_t pageid_init_counter = {0};

uint8_t arbit_rx = 0;
uint8_t arbit_tx = 0;
bool tx_available = 0; // must update with notif counter checking  

// declare all buffer types
using buffercore_t  = tapa::buffers<sb_msg_t[SB_MSGS_PER_PAGE], SB_NUM_PAGES, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
using ibuffercore_t = tapa::ibuffers<sb_msg_t[SB_MSGS_PER_PAGE], SB_NUM_PAGES, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
using obuffercore_t = tapa::obuffers<sb_msg_t[SB_MSGS_PER_PAGE], SB_NUM_PAGES, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::uram>>;
// declare all stream types
using brxqs_t  = tapa::streams<sb_req_t, SB_NXCTRS>;
using btxqs_t  = tapa::streams<sb_rsp_t, SB_NXCTRS>;
using brxq_t   = tapa::stream<sb_req_t>;
using btxq_t   = tapa::stream<sb_rsp_t>;
// brxqs_t* brxqs_p;// = nullptr;
// btxqs_t* btxqs_p;// = nullptr;
// brxq_t* arbit_rxq_p;// = nullptr;
// btxq_t* arbit_txq_p;// = nullptr;


#ifndef __SYNTHESIS__
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

#ifdef DEBUG_RQR
#define DEBUG_PRINT_RQR(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_RQR(...)
#endif

#ifdef DEBUG_PGM
#define DEBUG_PRINT_PGM(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_PGM(...)
#endif

#ifdef DEBUG_CRP
#define DEBUG_PRINT_CRP(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_CRP(...)
#endif

#ifdef DEBUG_DRP
#define DEBUG_PRINT_DRP(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_DRP(...)
#endif

#ifdef DEBUG_RSG
#define DEBUG_PRINT_RSG(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_RSG(...)
#endif

#ifdef DEBUG_IHD
#define DEBUG_PRINT_IHD(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_IHD(...)
#endif

#ifdef DEBUG_IHD_BUFF
#define DEBUG_PRINT_IHD_BUFF(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_IHD_BUFF(...)
#endif

#ifdef DEBUG_OHD
#define DEBUG_PRINT_OHD(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_OHD(...)
#endif

#ifdef DEBUG_OHD_BUFF
#define DEBUG_PRINT_OHD_BUFF(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_OHD_BUFF(...)
#endif

#endif //__SB_H__
