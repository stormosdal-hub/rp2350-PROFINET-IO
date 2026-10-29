/*
 * osal_cc.h - compiler support for the OSAL on the RP2350 (GCC, Cortex-M33: little endian).
 */
#ifndef CC_H
#define CC_H

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CC_PACKED_BEGIN
#define CC_PACKED_END
#define CC_PACKED __attribute__((packed))

#define CC_FORMAT(str, arg) __attribute__((format(printf, str, arg)))

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define CC_TO_LE16(x)   (x)
#define CC_TO_LE32(x)   (x)
#define CC_TO_LE64(x)   (x)
#define CC_FROM_LE16(x) (x)
#define CC_FROM_LE32(x) (x)
#define CC_FROM_LE64(x) (x)
#define CC_TO_BE16(x)   __builtin_bswap16(x)
#define CC_TO_BE32(x)   __builtin_bswap32(x)
#define CC_TO_BE64(x)   __builtin_bswap64(x)
#define CC_FROM_BE16(x) __builtin_bswap16(x)
#define CC_FROM_BE32(x) __builtin_bswap32(x)
#define CC_FROM_BE64(x) __builtin_bswap64(x)
#else
#error "osal_cc.h: big-endian target"
#endif

// 8, 16 and 32 bits are single loads and stores on the Cortex-M33
#define CC_ATOMIC_GET8(p)  (*(p))
#define CC_ATOMIC_GET16(p) (*(p))
#define CC_ATOMIC_GET32(p) (*(p))
#define CC_ATOMIC_GET64(p) __atomic_load_n((p), __ATOMIC_SEQ_CST)

#define CC_ATOMIC_SET8(p, v)  ((*(p)) = (v))
#define CC_ATOMIC_SET16(p, v) ((*(p)) = (v))
#define CC_ATOMIC_SET32(p, v) ((*(p)) = (v))
#define CC_ATOMIC_SET64(p, v) __atomic_store_n((p), (v), __ATOMIC_SEQ_CST)

#define CC_ASSERT(exp)        assert(exp)
#define CC_STATIC_ASSERT(exp) _Static_assert(exp, "")

#ifdef __cplusplus
}
#endif

#endif /* CC_H */
