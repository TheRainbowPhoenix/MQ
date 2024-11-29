//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.defs: General definitions available in every file
// This header is included explicitly or transitively in every mq header file.
//---

#ifndef MQ_DEFS_H
#define MQ_DEFS_H

#ifdef __cplusplus
# define MQ_START_DEFS extern "C" {
# define MQ_END_DEFS }
# define MQ_STATIC_ASSERT static_assert
#else
# define MQ_START_DEFS
# define MQ_END_DEFS
# define MQ_STATIC_ASSERT _Static_assert
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Aliases for common integer types */

typedef unsigned int uint;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

/* Attributes */

#define MQ_INLINE static inline __attribute__((always_inline))
#define MQ_LIKELY(EXPR) __builtin_expect((EXPR), 1)
#define MQ_UNLIKELY(EXPR) __builtin_expect((EXPR), 0)

#endif /* MQ_DEFS_H */
