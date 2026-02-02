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

enum {
    MQ_FILESYSTEM_TYPE_FUGUE_FAT12,
    MQ_FILESYSTEM_TYPE_FUGUE_FAT16,
    //MQ_FILESYSTEM_TYPE_CASIOWIN,
};

mqFilesystem *mq_filesystem_create(void);
bool mq_filesystem_initialize(mqFilesystem *fs, int fs_type);
bool mq_filesystem_destroy(mqFilesystem **fs);
bool mq_filesystem_set_root_uri(mqFilesystem *fs, char const *pathname);

//=== Wrapped POSIX functions ===

mqFilesystemFile *mq_filesystem_open(mqFilesystem *fs,
        char const *path, char const *mode);

bool mq_filesystem_create_file(mqFilesystem *fs,
        char const *path, bool is_dir);

bool mq_filesystem_close(mqFilesystem *fs,
        mqFilesystemFile **file);

u32 mq_filesystem_read(mqFilesystem *fs,
        mqFilesystemFile *file, void *buf, u32 count);

u32 mq_filesystem_write(mqFilesystem *fs,
        mqFilesystemFile *file, void *buf, u32 count);

int mq_filesystem_lseek(mqFilesystem *fs,
        mqFilesystemFile *file, int offset, int whence);

MQ_END_DEFS
#endif /* MQ_INTERFACES_FILESYSTEM_H */
