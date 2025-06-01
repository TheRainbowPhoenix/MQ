//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq-headless: Simple headless CLI interface for performance profiling

#include <mq/mq.h>
#include <mq/machine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>

void fatal(int rc, char const *fmt, ...);

void *openAndReadFile(char const *path, long *size_ptr)
{
    FILE *fp;
    void *data = NULL;
    long size;

    *size_ptr = 0;

    fp = fopen(path, "r");
    if(!fp) goto err;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Allocate a non-NULL pointer even if file is empty */
    data = malloc(size + (size == 0));
    if(!data) goto err;

    if(fread(data, size, 1, fp) != 1) goto err;
    fclose(fp);

    *size_ptr = size;
    return data;

err:
    fprintf(stderr, "openAndReadFile: cannot open '%s': %s\n",
        path, strerror(errno));
    if(fp)
        fclose(fp);
    if(data)
        free(data);
    return NULL;
}

int main(int argc, char **argv)
{
    if(argc != 2 || !strcmp(argv[1], "--help"))
        fatal(argc != 2, "usage: %s <add-in file>\n", argv[0]);

    char const *addinFile = argv[1];
    struct timespec tpStart, tpEnd;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tpStart);

    mq_init();
    mqMachine *mach = mq_machine_create();
    if(!mach)
        fatal(1, "could not allocate machine\n");

    mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_CG);
    long size;
    void *data = openAndReadFile(addinFile, &size);
    if(!data || !mq_machine_load_g3a(mach, data, size))
        fatal(1, "could not load %s\n", addinFile);
    free(data);

    printf("Waiting 1 billion cycles...\n");
    mq_machine_cycle(mach, 1000*1000*1000);

    mq_machine_destroy(mach);
    mq_quit();

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tpEnd);
    int64_t sec = tpEnd.tv_sec - tpStart.tv_sec;
    int64_t msec = (tpEnd.tv_nsec - tpStart.tv_nsec) / 1000000 + 1000 * sec;
    printf("Total time: %ld ms\n", msec);
    return 0;
}

//============================================================================//

void fatal(int rc, char const *fmt, ...)
{
    char str[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(str, sizeof str, fmt, args);
    va_end(args);

    fputs(str, stderr);
    exit(rc);
}
