void dummytask(tapa::istreams<sb_rsp_t, SB_NUNUSED>& rx,
                tapa::ostreams<sb_req_t, SB_NUNUSED>& tx)
{
  for(;;)
  {
    for (sb_portid_t unused = 0; unused < SB_NUNUSED; unused++)
    {
      bool valid;
      sb_rsp_t msg;
      valid = rx[unused].try_read(msg);
    }
  }
}