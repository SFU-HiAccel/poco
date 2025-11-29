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
