.invoke<tapa::detach>(rqr,
        sb_rx,
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
        rai_arbqs,
        wai_arbqs) 
.invoke<tapa::detach, kStageCount>(rai, tapa::seq(), rai_arbqs, rai_arbqs)
.invoke<tapa::detach, kStageCount>(wai, tapa::seq(), wai_arbqs, wai_arbqs)
.invoke<tapa::detach, expr_sb_num_xcsrs>(ihd, tapa::seq(),
        rai_arbqs,
        rao_arbqs,
        backend_pages)
.invoke<tapa::detach, expr_sb_num_xcsrs>(ohd, tapa::seq(),
        wai_arbqs,
        wao_arbqs,
        backend_pages)
.invoke<tapa::detach, kStageCount>(rao, tapa::seq(), rao_arbqs, rao_arbqs)
.invoke<tapa::detach, kStageCount>(wao, tapa::seq(), wao_arbqs, wao_arbqs)
.invoke<tapa::detach>(rsg,
        crp_to_rsg_grab,
        crp_to_rsg_free,
        drp_to_rsg_read,
        drp_to_rsg_write,
        rao_arbqs,
        wao_arbqs,
        sb_tx)
