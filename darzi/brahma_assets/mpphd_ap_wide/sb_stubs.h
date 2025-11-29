#ifndef __SB_INLINE_FUNCS_H__
#define __SB_INLINE_FUNCS_H__

#include "sb_structs.h"

////////////////////////////////////////
///          BRAHMA INTERNAL         ///
////////////////////////////////////////

// converts a sb_req_t to a sb_std_t format
sb_std_t req_to_std(sb_req_t rx_req)
{
    #pragma HLS inline
    sb_std_t tx_std = {0};
    tx_std.c_dn = rx_req.c_dn;
    tx_std.control = rx_req.control;
    tx_std.std_msg = rx_req.req_msg;
    return tx_std;
}

// converts a sb_std_t to a sb_rsp_t format
sb_rsp_t std_to_rsp(sb_std_t rx_std)
{
    #pragma HLS inline
    sb_rsp_t tx_rsp = {0};
    tx_rsp.c_dn = rx_std.c_dn;
    tx_rsp.control = rx_std.control;
    tx_rsp.rsp_msg = rx_std.std_msg;
    return tx_rsp;
}

sb_rsp_t req_to_rsp(sb_req_t rx_std)
{
    #pragma HLS inline
    sb_rsp_t tx_rsp = {0};
    tx_rsp.c_dn = rx_std.c_dn;
    tx_rsp.control = rx_std.control;
    tx_rsp.rsp_msg = rx_std.req_msg;
    return tx_rsp;
}

sb_req_t rsp_to_req(sb_rsp_t rx_std)
{
    #pragma HLS inline
    sb_req_t tx_req = {0};
    tx_req.c_dn = rx_std.c_dn;
    tx_req.control = rx_std.control;
    tx_req.req_msg = rx_std.rsp_msg;
    return tx_req;
}

////////////////////////////////////////
///          BRAHMA INTERFACE        ///
////////////////////////////////////////

sb_req_t sb_request_grab()
{
  #pragma HLS inline
  sb_req_t request = {0};
  request.c_dn = 1;
  sb_cmsg_t cmsg = 0;
  cmsg.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = SB_REQ_GRAB_PAGE;
  cmsg.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB) = 1;
  request.control = cmsg;
  return request;
}

sb_req_t sb_request_free(sb_pageid_t page_id)
{
  #pragma HLS inline
  sb_req_t request = {0};
  request.c_dn = 1;
  sb_cmsg_t msg = 0;
  cmsg.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = SB_REQ_FREE_PAGE;
  cmsg.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB) = page_id;
  request.control = cmsg;
  return request;
}

sb_req_t sb_request_write(sb_pageid_t page_id, sb_pageid_t start_index, uint8_t burst_length, bool keep_mutex)
{
  #pragma HLS inline
  sb_req_t request = {0};
  request.c_dn = 1;                           // assert control packet
  sb_cmsg_t cmsg = 0;
  cmsg.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = (keep_mutex) ? SB_REQ_WRITE_M : SB_REQ_WRITE_S;
  cmsg.range(SB_DMSG_IDX0_MSB, SB_DMSG_IDX0_LSB) = start_index;      // start from a specific index
  cmsg.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB) = burst_length;   // how many messages to write
  cmsg.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB) = page_id;         // on this page
  request.control = cmsg;
  return request;
}

sb_req_t sb_request_read(sb_pageid_t page_id, uint16_t start_index, uint8_t burst_length, bool keep_mutex)
{
  #pragma HLS inline
  sb_req_t request = {0};
  request.c_dn = 1;                           // assert control packet
  sb_cmsg_t cmsg = 0;
  cmsg.range(SB_CMSG_CODE_MSB, SB_CMSG_CODE_LSB) = (keep_mutex) ? SB_REQ_READ_M : SB_REQ_READ_S;
  cmsg.range(SB_DMSG_IDX0_MSB, SB_DMSG_IDX0_LSB) = start_index;      // start from a specific index
  cmsg.range(SB_CMSG_LENGTH_MSB, SB_CMSG_LENGTH_LSB) = burst_length;   // how many messages to write
  cmsg.range(SB_CMSG_PAGEID_MSB, SB_CMSG_PAGEID_LSB) = page_id;         // on this page
  request.control = cmsg;
  return request;
}



#endif //__SB_INLINE_FUNCS_H__
