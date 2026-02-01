//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.interfaces.filesystem: Filesystem bridge
//
// This header defines the filesystem API, which provides POSIX-style access to
// emulated files. The filesystem is currently bound to the "_mqfs" folder
// under the working directory.
//
// TODO: In-memory filesystem? Choose folder?
// TODO: This might be useful for hardware filesystem emulation later on.
//---

#ifndef MQ_INTERFACES_FILESYSTEM_H
#define MQ_INTERFACES_FILESYSTEM_H

#include <mq/defs.h>

MQ_START_DEFS

#include <stdio.h>

struct mqFilesystem {
    /* current */
    char  *root_uri;
    size_t root_uri_len;
};
typedef struct mqFilesystem mqFilesystem;
typedef FILE mqFilesystemFile;

//=== info

mqFilesystem *mq_filesystem_create(void);
void mq_filesystem_destroy(mqFilesystem **fs);
bool mq_filesystem_set_root_uri(mqFilesystem *fs, char const *pathname);

//=== Wrapped POSIX functions ===

mqFilesystemFile *mq_filesystem_open(mqFilesystem *fs,
        char const *path, char const *mode);

// int mq_filesystem_creat(mqFilesystem *fs, char const *path, u32 mode);
// i32 mq_filesystem_read(mqFilesystem *fs, int fd, void *buf, i32 count);
// i32 mq_filesystem_write(mqFilesystem *fs, int fd, void const *buf, i32 count);
// i32 mq_filesystem_lseek(mqFilesystem *fs, int fd, i32 offset, int whence);

MQ_END_DEFS
#endif /* MQ_INTERFACES_FILESYSTEM_H */
