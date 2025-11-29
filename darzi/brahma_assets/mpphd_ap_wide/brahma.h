#ifndef __BRAHMA_H__
#define __BRAHMA_H__
#include <stdint.h>
#include <iostream>
#include "tapa.h"
#include "../inc/sb_config.h"
#include "sb_structs.h"
#include "sb_stubs.h"

// used to set pageids in metadata when that page is requested for the first time
sb_pageid_t pageid_init_counter = 0;

// used by PGM
extern uint8_t byte_lut[255];

uint8_t arbit_rx = 0;
uint8_t arbit_tx = 0;
bool tx_available = 0; // must update with notif counter checking  

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

#ifdef DEBUG_OHD
#define DEBUG_PRINT_OHD(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define DEBUG_PRINT_OHD(...)
#endif


void loopback(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
              tapa::ostreams<sb_rsp_t, SB_NXCTRS>& btxqs);

// task wrapper that should be invoked at the kernel's top wrapper
void sb_task(tapa::istreams<sb_req_t, SB_NXCTRS>& sb_rxqs,
             tapa::ostreams<sb_rsp_t, SB_NXCTRS>& sb_txqs);

#endif //__BRAHMA_H__
