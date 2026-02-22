//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/interfaces/filesystem.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Cleanup function to free pointers dynamically allocated by path utils. */
static void freestr(char **string) { free(*string); }

/* Get the physical path associated with virtual path VIRT in filesystem FS.
   If conversion fails, return ERR_RC. Otherwise, produce char *PHYS which will
   be automatically freed when the function returns. */
#define GET_PHYSICAL_PATH(FS, VIRT, ERR_RC, PHYS)               \
    [[gnu::cleanup(freestr)]] char *PHYS = NULL;                \
    char const *__ROOT = FS->posix.root ? FS->posix.root : "."; \
    if(asprintf(&PHYS, "%s%s", __ROOT, VIRT) < 0) {             \
        PHYS = NULL; return ERR_RC;                             \
    }

static bool _filesystem_gen_virt_pathname(mqFilesystem *fs,
        char *output, char const *pathname, size_t n)
{
    if(!fs->posix.root) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_pathname: no root URI");
        return false;
    }
    int root_len = strlen(fs->posix.root);
    if(memcmp(pathname, fs->posix.root, root_len)) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_pathname: '%s' doesn't have "
            "expected prefix '%s'", pathname, fs->posix.root);
        return false;
    }
    char const *source = pathname + root_len;
    if(strlen(source) >= n) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_pathname: too short buffer");
        return false;
    }
    strcpy(output, source);
    return true;
}

//=== system interface ======================================================//

mqFilesystem *mq_filesystem_posix_create(char const *root)
{
    mqFilesystem *fs = calloc(1, sizeof *fs);
    if(!fs)
        return NULL;
    fs->type = MQ_FILESYSTEM_TYPE_POSIX;
    if(!mq_filesystem_posix_setRoot(fs, root)) {
        free(fs);
        return NULL;
    }
    return fs;
}

bool mq_filesystem_posix_setRoot(mqFilesystem *fs, char const *root)
{
    if(fs->type != MQ_FILESYSTEM_TYPE_POSIX)
        return false;
    free(fs->posix.root);

    // TODO: Better normalization of host paths?
    int rc = asprintf(&fs->posix.root, "%s/", root);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_posix_setRoot: %m");
        fs->posix.root = NULL;
        return false;
    }

    mq_log(MQ_LOG_DEBUG, "mq_filesystem_posix_setRoot: root set to %s",
        fs->posix.root);
    return true;
}

void mq_filesystem_reset(mqFilesystem *fs)
{
    // TODO: mq_filesystem_reset: Purge if in-memory filesystem
    (void)fs;
}

void mq_filesystem_destroy(mqFilesystem *fs)
{
    if(fs->type == MQ_FILESYSTEM_TYPE_POSIX)
        free(fs->posix.root);
    free(fs);
}

//=== File interface =========================================================//

int mq_filesystem_creat(mqFilesystem *fs, char const *virt, mode_t mode)
{
    GET_PHYSICAL_PATH(fs, virt, -1, phys);
    int rc = creat(phys, mode);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_creat(%s, %#o) [%s] = %d (%m)",
            virt, mode, phys, rc);
    }
    return rc;
}

int mq_filesystem_mkdir(mqFilesystem *fs, char const *virt, mode_t mode)
{
    GET_PHYSICAL_PATH(fs, virt, -1, phys);
    int rc = mkdir(phys, mode);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_mkdir(%s, %#o) [%s] = %d (%m)",
            virt, mode, phys, rc);
    }
    return rc;
}

int mq_filesystem_unlink(mqFilesystem *fs, char const *virt)
{
    GET_PHYSICAL_PATH(fs, virt, -1, phys);
    int rc = unlink(phys);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_unlink(%s) [%s] = %d (%m)",
            virt, phys, rc);
    }
    return rc;
}

int mq_filesystem_rmdir(mqFilesystem *fs, char const *virt)
{
    GET_PHYSICAL_PATH(fs, virt, -1, phys);
    int rc = rmdir(phys);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_rmdir(%s) [%s] = %d (%m)",
            virt, phys, rc);
    }
    return rc;
}

int mq_filesystem_stat(mqFilesystem *fs, char const *virt, struct stat *st)
{
    GET_PHYSICAL_PATH(fs, virt, -1, phys);
    int rc = stat(phys, st);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_stat(%s) [%s] = %d (%m)",
            virt, phys, rc);
    }
    return rc;
}

int mq_filesystem_fstat(mqFilesystem *fs, int fd, struct stat *st)
{
    (void)fs;
    int rc = fstat(fd, st);
    if(rc < 0)
        mq_log(MQ_LOG_ERROR, "mq_filesystem_fstat(%d) = %d (%m)", fd, rc);
    return rc;
}

int mq_filesystem_open(mqFilesystem *fs,
    char const *virt, int flags, mode_t mode)
{
    GET_PHYSICAL_PATH(fs, virt, -1, phys);
    int fd = open(phys, flags, mode);
    if(fd < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_open(%s, %#x, %#o) [%s] = %d (%m)",
            virt, flags, mode, phys, fd);
    }
    return fd;
}

int mq_filesystem_close(mqFilesystem *fs, int fd)
{
    (void)fs;
    int rc = close(fd);
    if(rc < 0)
        mq_log(MQ_LOG_ERROR, "mq_filesystem_close(%d) = %d (%m)", fd, rc);
    return rc;
}

ssize_t mq_filesystem_read(mqFilesystem *fs, int fd, void *buf, size_t size)
{
    (void)fs;
    ssize_t rc = read(fd, buf, size);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_read(%d, %p, %zu) = %d (%m)",
            fd, buf, size, rc);
    }
    return rc;
}

ssize_t mq_filesystem_write(mqFilesystem *fs,
    int fd, void const *buf, size_t size)
{
    (void)fs;
    ssize_t rc = write(fd, buf, size);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_write(%d, %p, %zu) = %d (%m)",
            fd, buf, size, rc);
    }
    return rc;
}

int mq_filesystem_lseek(mqFilesystem *fs, int fd, off_t offset, int whence)
{
    (void)fs;
    int rc = lseek(fd, offset, whence);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_lseek(%d, %zd, %d) = %d (%m)",
            fd, offset, whence, rc);
    }
    return rc;
}

//=== search interface ======================================================//

mqFilesystemSearch *mq_filesystem_search_open(mqFilesystem *fs,
        char const *pattern)
{
    GET_PHYSICAL_PATH(fs, pattern, NULL, phys_pattern);
    mqFilesystemSearch *search = calloc(1, sizeof *search);
    if(!search)
        return NULL;

    int rc = glob(phys_pattern, GLOB_PERIOD, NULL, &search->glob);
    if(rc == GLOB_NOMATCH) {
        search->glob.gl_pathc = 0;
        return search;
    }
    else if(rc != 0) {
        mq_log(MQ_LOG_ERROR, "fs_search_open: glob error");
        free(search);
        return NULL;
    }

    return search;
}

bool mq_filesystem_search_next(mqFilesystem *fs,
    mqFilesystemSearch *search, char *path, size_t path_size,
    struct stat *statbuf)
{
    if(!path || search->pos >= search->glob.gl_pathc)
        return false;

    /* Incremnt in case of error, so we don't loop */
    char const *phys_pathname = search->glob.gl_pathv[search->pos];
    search->pos++;

    if(!_filesystem_gen_virt_pathname(fs, path, phys_pathname, path_size)) {
        mq_log(MQ_LOG_ERROR, "fs_search_next: gen virt path error");
        return false;
    }
    if(statbuf && stat(phys_pathname, statbuf) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_search_stat: unable to stat()");
        return false;
    }

    return true;
}

void mq_filesystem_search_close(mqFilesystem *fs, mqFilesystemSearch *search)
{
    (void)fs;
    globfree(&search->glob);
}
