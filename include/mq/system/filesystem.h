//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.system.filesystem: Filesystem syscall emulation
//
// This header defines the filesystem API, which provides POSIX-style access to
// emulated files. The filesystem is currently bound to the "_mqfs" folder
// under the working directory.
//
// TODO: In-memory filesystem? Choose folder?
// TODO: This might be useful for hardware filesystem emulation later on.
//---

#ifndef MQ_SYSTEM_FILESYSTEM_H
#define MQ_SYSTEM_FILESYSTEM_H

#include <mq/defs.h>
MQ_START_DEFS

/* Maximum number of file descriptors and search descriptors. */
#define MQ_FILESYSTEM_MAX_FD 16

struct mqFilesystem {
    /* Open file descriptor table (-1 when unused). */
    int fdtable[MQ_FILESYSTEM_MAX_FD];
};

typedef struct mqFilesystem mqFilesystem;

/* Wrapped POSIX functions. */
int mq_filesystem_open(mqFilesystem *fs, char const *path, int flags, ...);
int mq_filesystem_creat(mqFilesystem *fs, char const *path, u32 mode);
i32 mq_filesystem_read(mqFilesystem *fs, int fd, void *buf, i32 count);
i32 mq_filesystem_write(mqFilesystem *fs, int fd, void const *buf, i32 count);
i32 mq_filesystem_lseek(mqFilesystem *fs, int fd, i32 offset, int whence);

MQ_END_DEFS
#endif /* MQ_SYSTEM_FILESYSTEM_H */
