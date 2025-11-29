constexpr int expr_sb_num_pages = SB_NUM_PAGES;
constexpr int expr_sb_num_xcsrs = SB_NXCSRS;

tapa::streams<sb_req_t, SB_NXCTRS, SB_BURST_SIZE> sb_rx("sb_rxqs");
tapa::streams<sb_rsp_t, SB_NXCTRS, SB_BURST_SIZE> sb_tx("sb_txqs");

tapa::streams<uint64_t, SB_NDEBUGQS> debugstreams("debug_streams");
// RQR  <--->  RQP
tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> rqr_to_drp_read("rqr_to_drp_read");
tapa::streams<sb_std_t, SB_NXCTRS, SB_DQS_DEPTH> rqr_to_drp_write("rqr_to_drp_write");
// RQR  <--->  CRA
tapa::streams<sb_cmsg_t, SB_NXCTRS> rqr_to_crp_grab("rqr_to_crp_grab");
tapa::streams<sb_cmsg_t, SB_NXCTRS> rqr_to_crp_free("rqr_to_crp_free");
// CRP  <--->  RQP
tapa::stream<sb_cmsg_t> crp_to_pgm_grab("crp_to_pgm_grab");
tapa::stream<sb_cmsg_t> crp_to_pgm_free("crp_to_pgm_free");
tapa::stream<sb_cmsg_t> pgm_to_crp_sts("pgm_to_crp_sts");
// CRP  <--->  RSG
tapa::stream<sb_cmsg_t> crp_to_rsg_grab("crp_to_rsg_grab");
tapa::stream<sb_cmsg_t> crp_to_rsg_free("crp_to_rsg_free");

/// PERFORMANCE CRITICAL STREAMS ///
tapa::streams<sb_std_t, SB_NXCTRS, SB_DRPTORSG_DEPTH> drp_to_rsg_read("drp_to_rsg_read");
tapa::streams<sb_std_t, SB_NXCTRS, SB_DRPTORSG_DEPTH> drp_to_rsg_write("drp_to_rsg_write");
// RQP  <--->  RAI <---> IHD
tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_RARBQS_DEPTH> rai_arbqs("rai_arbqs");
// IHD  <--->  RAO <---> RSG
tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_RARBQS_DEPTH> rao_arbqs("rao_arbqs");
// RQP  <--->  WAI <---> OHD
tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_WARBQS_DEPTH> wai_arbqs("wai_arbqs");
// OHD  <--->  WAO <---> RSG
tapa::streams<sb_apkt_t, kN*(kStageCount + 1), SB_WARBQS_DEPTH> wao_arbqs("wao_arbqs");

// backend pages
