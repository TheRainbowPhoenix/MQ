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

/* Macros and tools for profiling */

#ifdef __cplusplus
# include "tracy/Tracy.hpp"
#else
# include "tracy/TracyC.h"
#endif

MQ_START_DEFS

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
#define MQ_UNUSED __attribute__((unused))
#define MQ_LIKELY(EXPR) __builtin_expect((EXPR), 1)
#define MQ_UNLIKELY(EXPR) __builtin_expect((EXPR), 0)

/* Simple logging facilities for the GUI console */

enum mq_log_priority {
    MQ_LOG_MESSAGE,     // Message with no semantic information
    MQ_LOG_DEBUG,       // Debug message intended for developers
    MQ_LOG_WARNING,     // Attention-worthy information for user
    MQ_LOG_ERROR,       // Error report from the emulator; may be fatal or not
};

/* Log a message, printf()-style. */
void mq_log(enum mq_log_priority priority, char const *fmt, ...);

/* Set the message handler. NULL resets to the default. */
void mq_log_handler(void (*handler)(enum mq_log_priority, char *));

/* Default log handler; prints to stderr with colored "debug", "warning",
   "error" headers. */
void mq_log_default_handler(enum mq_log_priority priority, char *str);

/* Log a named message. Named messages have to be registered and are identified
   by an integer. Using named messages allows MQ to automatically silence them
   after a while (to clean up the output) while keeping track of statistics.
   Named messages have an associated log level so it's not repeated. */
void mq_logn(int name, char const *fmt, ...);

/* Set the rate limit of a named message. This limits how the message logs:
   - rateLimit > 0: the message shows this many times before going silent.
   - rateLimit = 0: the message doesn't show at all.
   - rateLimit < 0: the message shows all the time, with no limit. */
void mq_logn_setRateLimit(int name, i16 rateLimit);

/* Register one or more named message. This top-level macro works like this:
     MQ_LOG_REGISTER(lvalue NAME, char const *TAG, int LIMIT,
                     enum mq_log_priority PRIORITY, int COUNT)
   It sets NAME to the first if COUNT consecutive identifiers to be used with
   mq_logn, all set at the given PRIORITY. NAME is declared as a global. The
   TAG will be kept as a pointer and must remain alive (e.g. a literal). */
#define MQ_LOG_REGISTER(NAME, ...) MQ_LGR2(NAME, __COUNTER__, __VA_ARGS__)
#define MQ_LGR2(NAME, COUNTER, ...) MQ_LGR3(NAME, COUNTER, __VA_ARGS__)
#define MQ_LGR3(NAME, COUNTER, TAG, LIMIT, PRIORITY, COUNT) \
   int NAME; \
    __attribute__((constructor)) static void _log_ctor##COUNTER(void) { \
        NAME = mq_log_register(TAG, LIMIT, PRIORITY, COUNT); \
    }
int mq_log_register(
   char const *tag, int limit, enum mq_log_priority priority, int count);

MQ_END_DEFS
#endif /* MQ_DEFS_H */
