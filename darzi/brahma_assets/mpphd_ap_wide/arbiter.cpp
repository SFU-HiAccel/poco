constexpr int kN = SB_NXCSRS;  // kN x kN network
constexpr int kStageCount = SB_NXCSRS_LOG2;

#ifndef SB_ARBITER_PIPE_DEPTH
#define SB_ARBITER_PIPE_DEPTH (1)
#endif

// switch for page-->xctr arbitration
void Switch2x2(int b, tapa::istream<sb_apkt_t>& pkt_in_q0, tapa::istream<sb_apkt_t>& pkt_in_q1,
               tapa::ostreams<sb_apkt_t, 2>& pkt_out_q) {
  uint8_t priority = 0;

  b = kStageCount - 1 - b;

  sb_apkt_t pipe0[SB_ARBITER_PIPE_DEPTH];
  sb_apkt_t pipe1[SB_ARBITER_PIPE_DEPTH];
  
  for (bool valid_0, valid_1;;) {
    // enable the pipe here, cycling data, latency should be at the same level
    #pragma HLS latency max = 0
    // pipe0[0] = pkt_in_q0.peek(valid_0);
    // pipe1[0] = pkt_in_q1.peek(valid_1);
    // in_0 = pipe0[SB_ARBITER_PIPE_DEPTH-1];
    // in_1 = pipe1[SB_ARBITER_PIPE_DEPTH-1];
    sb_apkt_t in_0 = pkt_in_q0.peek(valid_0);
    sb_apkt_t in_1 = pkt_in_q1.peek(valid_1);
    auto pkt_0 = in_0.tag;
    auto pkt_1 = in_1.tag;
    bool fwd_0_0 = valid_0 && (pkt_0 & (1 << b)) == 0;
    bool fwd_0_1 = valid_0 && (pkt_0 & (1 << b)) != 0;
    bool fwd_1_0 = valid_1 && (pkt_1 & (1 << b)) == 0;
    bool fwd_1_1 = valid_1 && (pkt_1 & (1 << b)) != 0;

    bool conflict =
        valid_0 && valid_1 && fwd_0_0 == fwd_1_0 && fwd_0_1 == fwd_1_1;
    bool prioritize_1 = priority & 1;

    bool read_0 = !((!fwd_0_0 && !fwd_0_1) || (prioritize_1 && conflict));
    bool read_1 = !((!fwd_1_0 && !fwd_1_1) || (!prioritize_1 && conflict));
    bool write_0 = fwd_0_0 || fwd_1_0;
    bool write_1 = fwd_1_1 || fwd_0_1;
    bool write_0_0 = fwd_0_0 && (!fwd_1_0 || !prioritize_1);
    bool write_1_1 = fwd_1_1 && (!fwd_0_1 || prioritize_1);

    // if can forward through (0->0 or 1->1), do it
    // otherwise, check for conflict
    const bool written_0 =
        write_0 && pkt_out_q[0].try_write(write_0_0 ? in_0 : in_1);
    const bool written_1 =
        write_1 && pkt_out_q[1].try_write(write_1_1 ? in_1 : in_0);

    // if can forward through (0->0 or 1->1), do it
    // otherwise, round robin priority of both ins
    if (read_0 && (write_0_0 ? written_0 : written_1)) {
      pipe0[0] = pkt_in_q0.read(nullptr);
    }
    if (read_1 && (write_1_1 ? written_1 : written_0)) {
      pipe1[0] = pkt_in_q1.read(nullptr);
    }

    if (conflict) ++priority;
  }
}

void InnerStage(int b,
                tapa::istreams<sb_apkt_t, kN / 2>& in_q0,
                tapa::istreams<sb_apkt_t, kN / 2>& in_q1,
                tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
}

void rai(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}

void rao(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}

void wai(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}

void wao(int b,
        tapa::istreams<sb_apkt_t, kN>& in_q,
        tapa::ostreams<sb_apkt_t, kN> out_q) {
  tapa::task().invoke<tapa::detach>(InnerStage, b, in_q, in_q, out_q);
}
