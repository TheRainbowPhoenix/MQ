//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/defs.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void mq_log_default_handler(enum mq_log_priority priority, char *str)
{
    if(priority == MQ_LOG_DEBUG)
        fprintf(stderr, "\e[36mdebug:\e[m %s", str);
    else if(priority == MQ_LOG_WARNING)
        fprintf(stderr, "\e[33mwarning:\e[m %s", str);
    else if(priority == MQ_LOG_ERROR)
        fprintf(stderr, "\e[31merror:\e[m %s", str);
    else
        fputs(str, stderr);
}

static void (*loghandler)(enum mq_log_priority, char *) =
    mq_log_default_handler;

void mq_log(enum mq_log_priority priority, char *fmt, ...)
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
