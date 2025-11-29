// task wrapper that should be invoked at the kernel's top wrapper
void sb_task(tapa::istreams<sb_req_t, SB_NXCTRS>& sb_rxqs,
             tapa::ostreams<sb_rsp_t, SB_NXCTRS>& sb_txqs)
{
  // RQR  <--->  RQP
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> rqr_to_drp_read("rqr_to_drp_read");
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> rqr_to_drp_write("rqr_to_drp_write");
  // RQR  <--->  CRP
  tapa::streams<sb_std_t, SB_NXCTRS> rqr_to_crp_grab("rqr_to_crp_grab");
  tapa::streams<sb_std_t, SB_NXCTRS> rqr_to_crp_free("rqr_to_crp_free");
  // CRP  <--->  RQP
  tapa::stream<sb_std_t> crp_to_pgm_grab("crp_to_pgm_grab");
  tapa::stream<sb_std_t> crp_to_pgm_free("crp_to_pgm_free");
  tapa::stream<sb_std_t> pgm_to_crp_sts("pgm_to_crp_sts");
  // CRP  <--->  RSG
  tapa::stream<sb_std_t> crp_to_rsg_grab("crp_to_rsg_grab");
  tapa::stream<sb_std_t> crp_to_rsg_free("crp_to_rsg_free");

  /// PERFORMANCE CRITICAL STREAMS ///
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> drp_to_rsg_read("drp_to_rsg_read");
  tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> drp_to_rsg_write("drp_to_rsg_write");
  // RQP  <--->  RAI <---> IHD
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_RAIQS_DEPTH> rarbqs1("rarbqs1");
  // IHD  <--->  RAO <---> RSG
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_RAOQS_DEPTH> rarbqs2("rarbqs2");
  // RQP  <--->  WAI <---> OHD
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_WAIQS_DEPTH> warbqs1("warbqs1");
  // OHD  <--->  WAO <---> RSG
  tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_WAOQS_DEPTH> warbqs2("warbqs2");

  // actual buffers
  // buffercore_t backend_pages;
  tapa::buffers<sb_hmsg_t[SB_MSGS_PER_PAGE], SB_NUM_PAGES, 1, tapa::array_partition<tapa::normal>, tapa::memcore<tapa::bram>> backend_pages;

  tapa::task()
    .invoke<tapa::detach>(rqr,
            sb_rxqs,
            rqr_to_crp_grab,
            rqr_to_crp_free,
            rqr_to_drp_read,
            rqr_to_drp_write)
    .invoke<tapa::detach>(crp,
            pgm_to_crp_sts,
            crp_to_pgm_free,
            crp_to_pgm_grab,
            crp_to_rsg_free,
            crp_to_rsg_grab,
            rqr_to_crp_free,
            rqr_to_crp_grab)
    .invoke<tapa::detach>(pgm,
            crp_to_pgm_grab,
            crp_to_pgm_free,
            pgm_to_crp_sts)
    .invoke<tapa::detach>(drp,
            rqr_to_drp_read,
            rqr_to_drp_write,
            drp_to_rsg_read,
            drp_to_rsg_write,
            rarbqs1,
            warbqs1) 
    .invoke<tapa::detach, kStageCount>(rai, tapa::seq(), rarbqs1, rarbqs1)
    .invoke<tapa::detach, kStageCount>(wai, tapa::seq(), warbqs1, warbqs1)
    .invoke<tapa::detach, expr_sb_num_xcsrs>(ihd, tapa::seq(),
            rarbqs1,
            rarbqs2,
            // ohd_to_ihd_xfer_ctrl,
            // ihd_to_ohd_xfer_ctrl,
            backend_pages)
    .invoke<tapa::detach, expr_sb_num_xcsrs>(ohd, tapa::seq(),
            warbqs1,
            warbqs2,
            // ihd_to_ohd_xfer_ctrl,
            // ohd_to_ihd_xfer_ctrl,
            backend_pages)
    .invoke<tapa::detach, kStageCount>(rao, tapa::seq(), rarbqs2, rarbqs2)
    .invoke<tapa::detach, kStageCount>(wao, tapa::seq(), warbqs2, warbqs2)
    .invoke<tapa::detach>(rsg,
            crp_to_rsg_grab,
            crp_to_rsg_free,
            drp_to_rsg_read,
            drp_to_rsg_write,
            rarbqs2,
            warbqs2,
            sb_txqs);
}
