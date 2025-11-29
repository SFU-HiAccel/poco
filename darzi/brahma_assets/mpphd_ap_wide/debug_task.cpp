
void debug_task(tapa::istreams<sb_msg_t, SB_NDEBUGQS>& debugstreams)
{
  sb_msg_t msg[SB_NDEBUGQS] = {0};
  for(bool valid[SB_NDEBUGQS];;)
  {
    for (int i = 0; i < SB_NDEBUGQS; i++)
    {
      msg[i] = debugstreams[i].peek(valid[i]);
      if(valid[i])
      {
        debugstreams[i].read();
      }
    }
  }
}

