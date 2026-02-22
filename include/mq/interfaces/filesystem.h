//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.interfaces.filesystem: Filesystem bridge
//
// This header defines the filesystem API, which provides POSIX-style access to
// emulated files. The intent is that the filesystem could be based on varied
// data sources, including at least a folder from the host system or a virtual
// in-memory filesystem for browser builds.
//
// In addition to this basic interface, this interface should also deal with
// importing and exporting filesystem dumps, for e.g. browser localStorage or
// FAT12/FAT16 representations. The FAT exports in particular may be performed
// live to serve low-level filesystem drivers (like Fygue in gint).
//---

#ifndef MQ_INTERFACES_FILESYSTEM_H
#define MQ_INTERFACES_FILESYSTEM_H

#include <mq/defs.h>
#include <glob.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
MQ_START_DEFS

enum {
    /* The filesystem is stored on disk and accessed via POSIX interface. */
    MQ_FILESYSTEM_TYPE_POSIX,
    /* The filesystem is stored in-memory. */
    // MQ_FILESYSTEM_TYPE_MEMORY,
};

struct mqFilesystem;
typedef struct mqFilesystem mqFilesystem;

struct mqFilesystem {
    /* MQ_FILESYSTEM_TYPE_* and the associated implementation data */
    int type;

    struct {
        char *root;
    } posix;

    // struct {
    // } memory;
};

struct mqFilesystemSearch {
    size_t pos;
    glob_t glob;
};

typedef struct mqFilesystem mqFilesystem;
typedef struct mqFilesystemSearch mqFilesystemSearch;

/* Create a POSIX-backed filesystem. */
mqFilesystem *mq_filesystem_posix_create(char const *root);
/* Change the root folder. */
bool mq_filesystem_posix_setRoot(mqFilesystem *fs, char const *root);

// TODO: In-memory filesystem implementation

/* CRD functions. */
void mq_filesystem_reset(mqFilesystem *fs);
void mq_filesystem_destroy(mqFilesystem *fs);

/* Standard POSIX file functions. */
// is_dir: mkdir 0755; otherwise creat 0644

int mq_filesystem_creat(mqFilesystem *fs, char const *path, mode_t mode);
int mq_filesystem_mkdir(mqFilesystem *fs, char const *path, mode_t mode);
int mq_filesystem_unlink(mqFilesystem *fs, char const *path);
int mq_filesystem_rmdir(mqFilesystem *fs, char const *path);

int mq_filesystem_stat(mqFilesystem *fs, char const *path, struct stat *st);
int mq_filesystem_fstat(mqFilesystem *fs, int fd, struct stat *st);

int mq_filesystem_open(mqFilesystem *fs,
    char const *path, int flags, mode_t mode);
int mq_filesystem_close(mqFilesystem *fs, int fd);

ssize_t mq_filesystem_read(mqFilesystem *fs, int fd, void *buf, size_t size);
ssize_t mq_filesystem_write(mqFilesystem *fs,
    int fd, void const *buf, size_t size);
int mq_filesystem_lseek(mqFilesystem *fs, int fd, off_t offset, int whence);

//=== search functions ======================================================//

mqFilesystemSearch *mq_filesystem_search_open(mqFilesystem *fs,
    char const *pattern);

/* Get the path and stat of one of the found files. statbuf is optional. */
bool mq_filesystem_search_next(mqFilesystem *fs,
    mqFilesystemSearch *search, char *path, size_t path_size,
    struct stat *statbuf);

void mq_filesystem_search_close(mqFilesystem *fs,
    mqFilesystemSearch *search);

MQ_END_DEFS
#endif /* MQ_INTERFACES_FILESYSTEM_H */
