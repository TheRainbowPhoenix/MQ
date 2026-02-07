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

#include <glob.h>
#include <stdio.h>
#include <sys/stat.h>

//=== system interface ======================================================//

struct mqFilesystem {
    /* current root path */
    char  *root_uri;
    size_t root_uri_len;
};
typedef struct mqFilesystem mqFilesystem;
struct mqFilesystemSearch {
    int pos;
    glob_t glob;
};
typedef struct mqFilesystemSearch mqFilesystemSearch;
typedef FILE mqFilesystemFile;

enum {
    MQ_FILESYSTEM_TYPE_FUGUE_FAT12,
    MQ_FILESYSTEM_TYPE_FUGUE_FAT16,
    //MQ_FILESYSTEM_TYPE_CASIOWIN,
};

mqFilesystem *mq_filesystem_interface_create(void);
bool mq_filesystem_interface_initialize(mqFilesystem *fs, int fs_type);
bool mq_filesystem_interface_set_root_uri(mqFilesystem *fs, char const *uri);
bool mq_filesystem_interface_destroy(mqFilesystem **fs);

//=== file functions ========================================================//

bool mq_filesystem_file_create(mqFilesystem *fs,
        char const *virt_pathname, bool is_dir);

bool mq_filesystem_file_delete(mqFilesystem *fs,
        char const *virt_pathname);

bool mq_filesystem_file_stat(mqFilesystem *fs,
        char const *virt_pathname, struct stat *statbuf);

mqFilesystemFile *mq_filesystem_file_open(mqFilesystem *fs,
        char const *virt_pathname, char const *mode);

u32 mq_filesystem_file_read(mqFilesystem *fs,
        mqFilesystemFile *file, void *buf, u32 count);

u32 mq_filesystem_file_write(mqFilesystem *fs,
        mqFilesystemFile *file, void *buf, u32 count);

bool mq_filesystem_file_lseek(mqFilesystem *fs,
        mqFilesystemFile *file, int offset, int whence);

bool mq_filesystem_file_fstat(mqFilesystem *fs,
        mqFilesystemFile *file, struct stat *statbuf);

bool mq_filesystem_file_close(mqFilesystem *fs,
        mqFilesystemFile **file);

//=== search functions ======================================================//

mqFilesystemSearch *mq_filesystem_search_open(mqFilesystem *fs,
        char const *pattern);

bool mq_filesystem_search_next(mqFilesystem *fs,
        mqFilesystemSearch *search, char *buffer, size_t n);

bool mq_filesystem_search_stat(mqFilesystem *fs,
        mqFilesystemSearch *search, struct stat *statbuf);

bool mq_filesystem_search_close(mqFilesystem *fs,
        mqFilesystemSearch **search);

MQ_END_DEFS
#endif /* MQ_INTERFACES_FILESYSTEM_H */
