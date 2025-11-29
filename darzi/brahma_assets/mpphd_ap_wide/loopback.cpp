/**
 * Task     : loopback
 * Purpose  : A task that loops back for all SB-RX --> SB-TX streams
 *              on the BRAHMA core interface.
 *              Intended to be used for testing the harness/testbench.
 *
*/
void loopback(tapa::istreams<sb_req_t, SB_NXCTRS>& brxqs,
              tapa::ostreams<sb_rsp_t, SB_NXCTRS>& btxqs)
{
    for(bool valid[SB_NXCTRS];;)
    {
        for(uint8_t xctr = 0; xctr < SB_NXCTRS; xctr++) // this check is being done for each xctr stream being rxed
        {
        #pragma HLS unroll    // full unroll by a factor of SB_NXCTRS
            sb_req_t req = brxqs[xctr].peek(valid[xctr]);
            sb_rsp_t rsp = {0};
            if(valid[xctr])
            {
                rsp.c_dn = 1;
                rsp.fields.code = req.fields.code;
                rsp.fields.length = req.fields.length;
                btxqs[xctr] << rsp;
                req = brxqs[xctr].read();
            }
        }
    }
}
