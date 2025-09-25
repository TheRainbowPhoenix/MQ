//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/defs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

void *memdup(void const *ptr, size_t size)
{
    if(!ptr || !size)
        return NULL;
    void *optr = malloc(size);
    return optr ? memcpy(optr, ptr, size) : NULL;
}

void mq_log_default_handler(enum mq_log_priority priority, char *str)
{
    if(priority == MQ_LOG_DEBUG)
        fprintf(stderr, "\e[36mdebug:\e[m %s\n", str);
    else if(priority == MQ_LOG_WARNING)
        fprintf(stderr, "\e[33mwarning:\e[m %s\n", str);
    else if(priority == MQ_LOG_ERROR)
        fprintf(stderr, "\e[31merror:\e[m %s\n", str);
    else
        fprintf(stderr, "%s\n", str);
}

static void (*loghandler)(enum mq_log_priority, char *) =
    mq_log_default_handler;

void mq_log(enum mq_log_priority priority, char const *fmt, ...)
{
    char *str = NULL;
    va_list args;
    va_start(args, fmt);
    vasprintf(&str, fmt, args);
    va_end(args);

    if(loghandler && str)
        loghandler(priority, str);

    free(str);
}

void mq_log_handler(void (*handler)(enum mq_log_priority, char *))
{
    loghandler = handler ? handler : mq_log_default_handler;
}

//=== Named messages =========================================================//

struct named_message {
    /* Tag string */
    char const *tag;
    /* Rate limiter */
    i16 rateLimit;
    /* Log priority */
    enum mq_log_priority priority;
};

static struct named_message *NM_messages = NULL;
static int NM_count = 0;
static int NM_capacity = 0;

int mq_log_register(
    char const *tag, int limit, enum mq_log_priority priority, int count)
{
    /* Reallocate by chunks of 16 */
    if(NM_count + count >= NM_capacity) {
        int new_capacity = (NM_count + count + 15) & -16;
        struct named_message *new_messages =
            realloc(NM_messages, new_capacity * sizeof *NM_messages);
        if(!new_messages) {
            mq_log(MQ_LOG_ERROR, "mq_log_register: Failed to increase "
                "capacity to %d", new_capacity);
            return INT_MIN;
        }
        NM_messages = new_messages;
        NM_capacity = new_capacity;
    }

    int start = NM_count;
    for(int i = 0; i < count; i++) {
        struct named_message *msg = &NM_messages[start + i];
        msg->tag = tag;
        msg->rateLimit = limit;
        msg->priority = priority;
    }

    NM_count += count;
    return start;
}

void mq_logn(int name, char const *fmt, ...)
{
    if(name < 0 || name >= NM_count) {
        mq_log(MQ_LOG_WARNING, "mq_logn: invalid log name %d", name);
        return;
    }

    struct named_message *msg = &NM_messages[name];
    /* Rate limit of 0 disables the message entirely. */
    if(msg->rateLimit == 0)
        return;

    char *str = NULL;
    va_list args;
    va_start(args, fmt);
    vasprintf(&str, fmt, args);
    va_end(args);

    if(loghandler && str)
        loghandler(msg->priority, str);

    free(str);

    /* If rate limit isn't infinite, decrease it. Upon zero, make it silent. */
    if(msg->rateLimit >= 0) {
        msg->rateLimit--;
        if(msg->rateLimit == 0)
            loghandler(msg->priority, "(^ this message will now be silenced)");
    }
}

void mq_logn_setRateLimit(int name, i16 rateLimit)
{
    if(name >= 0 && name < NM_count)
        NM_messages[name].rateLimit = rateLimit;
}
