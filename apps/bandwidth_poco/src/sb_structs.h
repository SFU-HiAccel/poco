#ifndef __SB_STRUCTS_H__
#define __SB_STRUCTS_H__

#include "sb_config.h"

#define SB_MSG_W          (SB_MSG_SIZE << 3)  // from #bytes to #bits
#define SB_HMSG_W         (SB_MSG_W >> 1)
#define SB_HMSG_SIZE      (SB_MSG_SIZE) // (/8/2 = /16)
#define SB_HMSG_MASK      ((1 << SB_WORD_SIZE) - 1)

using sb_portid_t     = uint8_t;
using sb_pageid_t     = ap_uint<16>;
using sb_stream_t     = ap_uint<SB_WORD_SIZE_BITS>;
using sb_msg_t        = ap_uint<SB_MSG_W>;
using sb_hmsg_t       = ap_uint<SB_HMSG_W>;
using sb_cmsg_t       = ap_uint<48>;

// let lower two bits define READ/WRITE direction
#define SB_RW_MASK        (0x3)
// continued R/W or not - this controls whether dummy xfer_ctrl b/w IOHD is sent
#define SB_RW_CONT_MASK   (0x4)
#define SB_REQ_WRITE_S    (0x1)   // single WRITE
#define SB_REQ_READ_S     (0x2)   // single READ
#define SB_REQ_WRITE_M    (SB_REQ_WRITE_S | SB_RW_CONT_MASK)   // multi  WRITE
#define SB_REQ_READ_M     ( SB_REQ_READ_S | SB_RW_CONT_MASK)   // multi  READ

#define SB_REQ_GRAB_PAGE  (0x4)
#define SB_REQ_FREE_PAGE  (0x8)

#define SB_RSP_DONE       (0xD<<4)
#define SB_RSP_WAIT       (0xE<<4)
#define SB_RSP_FAIL       (0xF<<4)

#define SB_CMSG_CODE_MSB    (47)   // `code` is 8 bits wide
#define SB_CMSG_CODE_LSB    (40)
#define SB_CMSG_LENGTH_MSB  (39)  // `length` is 8 bits wide
#define SB_CMSG_LENGTH_LSB  (32)
#define SB_CMSG_PAGEID_MSB  (31)  // `pageid` is 16 bits wide  
#define SB_CMSG_PAGEID_LSB  (16)
#define SB_CMSG_PAGE_MSB    (31)  // `page` is 8 bits wide  
#define SB_CMSG_PAGE_LSB    (24)
#define SB_CMSG_XCXR_MSB    (23)  // `xcxr` is 8 bits wide  
#define SB_CMSG_XCXR_LSB    (16)
#define SB_CMSG_ADDR_MSB    (15)  // `addr` is 16 bits wide
#define SB_CMSG_ADDR_LSB    (0)

#define SB_PAGEID_PAGE_MSB    (15)  // `page` is 8 bits wide  
#define SB_PAGEID_PAGE_LSB    (8)
#define SB_PAGEID_XCXR_MSB    (7)   // `xcxr` is 8 bits wide  
#define SB_PAGEID_XCXR_LSB    (0)

typedef struct{
  sb_msg_t msg[SB_NUM_PARTITIONS];
}sb_dmsg_t;

typedef struct {
  sb_cmsg_t control;
  sb_dmsg_t req_msg;
  bool c_dn;
}sb_req_t;

typedef struct {
  sb_cmsg_t control;
  sb_dmsg_t rsp_msg;
  bool c_dn;
}sb_rsp_t;

typedef struct {
  sb_cmsg_t control;
  sb_dmsg_t std_msg;
  bool c_dn;
}sb_std_t;

/**
 * Arbiter Packet Type:
 * 
 * <---sb_xapkt_t---->
 * |  8  |   ....    |
 *  <tag> <sb_std_t>
 *
 * tag      : stores the xctr that this packet must be routed to to.
 *            Used by the arbiter that performs the page-->xctr arbitration.
 *            The tag is added by the RQP (TODO: update terminology (RQR))
 * sb_std_t : the standard internal message
 * 
 */ 
typedef struct {
  sb_std_t    msg;
  sb_portid_t tag;
}sb_apkt_t;

#endif // __SB_STRUCTS_H__
