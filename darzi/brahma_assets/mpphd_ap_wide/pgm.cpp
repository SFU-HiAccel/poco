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
