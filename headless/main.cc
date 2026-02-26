#include <mq/mq.h>
#include <mq/controller.h>
#include <mq/machine.h>
#include <mq/interfaces/display.h>
#include <mq/interfaces/keyboard.h>
#include <mq/system/casiowin.h>
#include <mq/system/heap.h>
#include <mq/modules/mmu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>

// If we want recording
#include "../gui/record.h"

// Global controller
mqController *emu0 = NULL;

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] <g1a/g3a file>\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --frames <n>      Run for n frames (default: 600)\n");
    fprintf(stderr, "  --record <file>   Record video to file (requires ffmpeg support)\n");
    fprintf(stderr, "  --headless        (Implicit)\n");
    fprintf(stderr, "  --help            Show this help\n");
}

int main(int argc, char **argv) {
    int frames_to_run = 600;
    const char *program_path = NULL;
    const char *record_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames_to_run = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            record_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else {
            program_path = argv[i];
        }
    }

    if (!program_path) {
        fprintf(stderr, "Error: No program file specified.\n");
        usage(argv[0]);
        return 1;
    }

    mq_init();
    mqMachine *mach = mq_machine_create();
    emu0 = mq_controller_create();
    if (!emu0) {
        fprintf(stderr, "Failed to create controller\n");
        return 1;
    }

    // Load program
    FILE *f = fopen(program_path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *data = malloc(size);
    if (!data) {
        perror("malloc");
        fclose(f);
        return 1;
    }
    if (fread(data, 1, size, f) != (size_t)size) {
        perror("fread");
        free(data);
        fclose(f);
        return 1;
    }
    fclose(f);

    // Initialize machine based on file extension
    // Simple check
    const char *ext = strrchr(program_path, '.');
    if (ext && (strcasecmp(ext, ".g1a") == 0)) {
         mq_machine_setupHardware(mach, MQ_MACHINE_HARDWARE_VIRT_ADDIN_FX);
         mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN);
         mq_machine_load_g1a(mach, data, size);
    } else if (ext && (strcasecmp(ext, ".g3a") == 0)) {
         mq_machine_setupHardware(mach, MQ_MACHINE_HARDWARE_VIRT_ADDIN_CG);
         mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN);
         mq_machine_load_g3a(mach, data, size);
    } else {
        fprintf(stderr, "Unknown file extension: %s\n", ext ? ext : "(none)");
        free(data);
        return 1;
    }
    free(data);

    // Setup recording if requested
#if MQ_VIDEO_FFMPEG
    mqRecord recorder;
    if (record_path) {
        // usually display is set up by mq_machine_initialize
        if (!recorder.start(mach->display, record_path, 60, false)) {
            fprintf(stderr, "Failed to start recording: %s\n", recorder.lasterror().c_str());
            return 1;
        }
    }
#else
    if (record_path) {
        fprintf(stderr, "Warning: FFMPEG support disabled, recording ignored.\n");
    }
#endif

    // Run loop
    emu0->mach = mach;

    printf("Running for %d frames...\n", frames_to_run);

    // Approx cycles per frame (SH4 ~29.5MHz)
    // 29491200 / 60 = 491520 cycles per frame
    int cycles_per_frame = 491520;

    for (int f = 0; f < frames_to_run; f++) {
        int cycles_done = 0;
        bool frame_ready = false;

        // Run chunks until we produce a frame or exceed budget
        while (!frame_ready && cycles_done < cycles_per_frame * 2) {
             int chunk = 20000;

             // Depending on implementation, mq_machine_cycle might use setjmp
             // But usually it's handled inside or by caller.
             // If we don't have MQ_CONTROLLER_SETJMP defined here, we assume core handles it or we need to mirror controller.c logic.
             // Since we link against LIB_SOURCES, mq_machine_cycle is available.
             // We need to check if we need to wrap it in setjmp if MQ_CONTROLLER_SETJMP is true.
             // For now, let's assume direct call works as in controller.c ifdef block.

             #if MQ_CONTROLLER_SETJMP
             int rc = mq_machine_setBreakJumpBuffer(mach);
             if (rc == 0) {
                 mq_machine_cycle(mach, chunk);
             }
             mq_machine_clearBreakJumpBuffer(mach);
             #else
             mq_machine_cycle(mach, chunk);
             #endif

             cycles_done += chunk;

             if (mach->display && mach->display->frameChanged) {
                 frame_ready = true;
                 mq_display_setFrameChanged(mach->display, false);

#if MQ_VIDEO_FFMPEG
                 if (record_path) {
                     recorder.frame_add(mach->display);
                 }
#endif
             }

             if (mach->stuck) {
                 fprintf(stderr, "Machine stuck at frame %d\n", f);
                 // Should we exit or continue?
                 // If stuck, it won't progress.
                 goto end_loop;
             }
        }
    }

end_loop:

#if MQ_VIDEO_FFMPEG
    if (record_path) {
        recorder.stop(mach->display);
    }
#endif

    mq_controller_destroy(emu0);
    // mq_machine_destroy(mach); // controller destroys it if attached?
    // mq_controller_destroy calls mq_controller_reset which calls mq_machine_destroy(controller->mach).
    mq_quit();

    return 0;
}
