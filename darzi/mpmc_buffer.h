#ifndef TAPA_BASE_MPMCBUFFER_H
#define TAPA_BASE_MPMCBUFFER_H

namespace tapa {

struct normal {};
struct complete {};

template <int ft>
struct cyclic {
  const int factor = ft;
};

template <int ft>
struct block {
  const int factor = ft;
};

template <typename... partitions>
struct array_partition {};

struct bram {};
struct uram {};

template <typename core_type>
struct memcore {};

template <int ft>
struct blocks {
  const int factor = ft;
};

template <int ft>
struct pages {
  const int factor = ft;
};

// struct sb_req_t {};
struct sb_rsp_t {};
// struct sb_pageid_t {};

template <typename T, typename... dims>
class mpmcbuffer;

// template <typename mbuf, int nports>
// class mpmc;

template <typename mbuf>
struct port_proxy {
  template<typename... Ts> sb_rsp_t do_alloc (Ts...);
  template<typename... Ts> sb_rsp_t do_free  (Ts...);
  template<typename... Ts> sb_rsp_t do_write (Ts...);
  template<typename... Ts> sb_rsp_t do_read  (Ts...);
};

template<typename mpmcbuf, int N>
struct mpmc {
  port_proxy<mpmcbuf> operator[](int) const;
  port_proxy<mpmcbuf> operator[](int);
};

// template <typename T, int n>
// struct istreams {
//   T* data[n];
// };

// template <typename T, int n>
// struct ostreams {
//   T* data[n];
// };

}  // namespace tapa

#endif  // TAPA_BASE_MPMCBUFFER_H