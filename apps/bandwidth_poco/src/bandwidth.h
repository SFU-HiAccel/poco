#define TILE          (32)
#define N             (16)
#define DEPTH         (2)
#define PACK_LENGTH   (2)
#define NDBLKS        (TILE/PACK_LENGTH)
#define NUM_PAR_READS (4)

#include "tapa.h"
#include <ap_int.h>
#include "sb_config.h"

#define SB_PAGE_SIZE        (1024)   // bytes
#define SB_MSGS_PER_PAGE    ((SB_PAGE_SIZE)/(SB_MSG_SIZE))
// #ifndef __SYNTHESIS__
// #define SB_WORDS_PER_PAGE   (2*SB_MSGS_PER_PAGE + 4)
// #else
#define SB_WORDS_PER_PAGE   (2*SB_MSGS_PER_PAGE)
// #endif

template <typename T>
using bits = ap_uint<tapa::widthof<T>()>;
using data_type_mmap = uint64_t;

void bandwidth(
          tapa::mmap<const uint64_t> ivector_values0,
          tapa::mmap<uint64_t> ovector_timer,
          int dummy);

