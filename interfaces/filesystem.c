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

//=== path handling =========================================================//

static bool _filesystem_gen_phys_pathname(mqFilesystem *fs,
        char *output, char const *pathname, size_t n)
{
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_phys_patname: no root URI");
        return false;
    }
    int rc = snprintf(output, n, "%s%s", fs->root_uri, pathname);

    if(rc >= (int)n) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_phys_patname: buffer too short");
        return false;
    }

    return true;
}

static bool _filesystem_gen_virt_pathname(mqFilesystem *fs,
        char *output, char const *pathname, size_t n)
{
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_pathname: no root URI");
        return false;
    }
    if(memcmp(pathname, fs->root_uri, fs->root_uri_len)) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_pathname: '%s' doesn't have "
            "expected prefix '%s'", pathname, fs->root_uri);
        return false;
    }
    char const*source = &(pathname[fs->root_uri_len]);
    if(strlen(source) >= n) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_pathname: too short buffer");
        return false;
    }
    mq_log(MQ_LOG_DEBUG, "_fs_gen_virt_pathname: generated -> %s", source);
    strcpy(output, source);
    return true;
}

//=== system interface ======================================================//

mqFilesystem *mq_filesystem_create(void)
{
    mqFilesystem *fs = calloc(1, sizeof(mqFilesystem));
    return fs;
}

bool mq_filesystem_initialize(mqFilesystem *fs, int fs_type)
{
    if(!fs) {
        mq_log(MQ_LOG_ERROR, "fs_interface_destroy: broken arguments");
        return false;
    }
    //todo: support specific fs-type initialization
    (void)fs_type;
    return true;
}

void mq_filesystem_destroy(mqFilesystem *fs)
{
    free(fs);
}

bool mq_filesystem_set_root_uri(mqFilesystem *fs,
        char const *pathname)
{
    if(!pathname)
        return false;
    if(fs->root_uri)
        free(fs->root_uri);

    int rc = asprintf(&fs->root_uri, "%s/", pathname);
    if(rc < 0) {
        mq_log(MQ_LOG_ERROR, "fs_interface_set_uri: unable to strdup()");
        fs->root_uri = NULL;
        return false;
    }

    fs->root_uri_len = rc;
    mq_log(MQ_LOG_DEBUG,
            "fs_interface_set_root_uri: switch root for %s", fs->root_uri);
    return true;
}

//=== file interface ========================================================//

bool mq_filesystem_file_create(mqFilesystem *fs,
        char const *virt_pathname, bool is_dir)
{
    char phys_pathname[1024];
    struct stat st;
    int ret;

    if(!virt_pathname)
        return false;

    if(!_filesystem_gen_phys_pathname(fs, phys_pathname, virt_pathname, 1024))
        return false;
    if(stat(phys_pathname, &st) != -1) {
        mq_log(MQ_LOG_ERROR,
                "fs_file_create: file already exists %s", phys_pathname);
        return false;
    }
    if(is_dir) {
        ret = mkdir(phys_pathname, 0755);
    } else {
        ret = creat(phys_pathname, 0644);
    }
    // TODO: Check EEXIST after the call instead of stat() before
    if(ret < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_create: unable to mkdir() or creat()");
        return false;
    }
    return true;
}

bool mq_filesystem_file_delete(mqFilesystem *fs,
        char const *virt_pathname)
{
    char phys_pathname[1024];

    if(!virt_pathname)
        return false;
    if(!_filesystem_gen_phys_pathname(fs, phys_pathname, virt_pathname, 1024))
        return false;
    if(remove(phys_pathname) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_delete: unable to remove()");
        return false;
    }
    return true;
}

bool mq_filesystem_file_stat(mqFilesystem *fs,
        char const *virt_pathname, struct stat *statbuf)
{
    char phys_pathname[1024];

    if(!virt_pathname || !statbuf)
        return false;
    if(!_filesystem_gen_phys_pathname(fs, phys_pathname, virt_pathname, 1024))
        return false;
    if(stat(phys_pathname, statbuf) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_stat: unable to stat()");
        return false;
    }
    return true;
}

mqFilesystemFile *mq_filesystem_file_open(mqFilesystem *fs,
        char const *virt_pathname, char const *mode)
{
    char phys_pathname[1024];

    if(!virt_pathname || !mode)
        return NULL;
    if(!_filesystem_gen_phys_pathname(fs, phys_pathname, virt_pathname, 1024))
        return NULL;
    FILE *fp = fopen(phys_pathname, mode);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "fs_file_open: %s - %s", phys_pathname, mode);
        return NULL;
    }
    return fp;
}

u32 mq_filesystem_file_read(mqFilesystem *fs,
        mqFilesystemFile *file, void *buff, u32 count)
{
    (void)fs;
    if(!file || !buff)
        return 0;
    int rc = fread(buff, sizeof(u8), count, file);
    if(rc <= 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_read: unable to fread()");
        return 0;
    }
    return rc;
}

u32 mq_filesystem_file_write(mqFilesystem *fs,
        mqFilesystemFile *file, void *buff, u32 count)
{
    (void)fs;
    if(!file || !buff)
        return -1;
    int rc = fwrite(buff, sizeof(u8), count, file);
    if(rc <= 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_write: unable to fwrite()");
        return rc;
    }
    // todo: remove flush()
    if(fflush(file) != 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_write: unable to fflush()");
        return 0;
    }
    return rc;
}

bool mq_filesystem_file_lseek(mqFilesystem *fs,
        mqFilesystemFile *file, int offset, int whence)
{
    (void)fs;
    if(!file)
        return false;
    if(fseek(file, offset, whence) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_lseek: unable to fseek()");
        return false;
    }
    return true;
}

bool mq_filesystem_file_fstat(mqFilesystem *fs,
        mqFilesystemFile *file, struct stat *statbuf)
{
    (void)fs;
    if(!file || !statbuf)
        return false;
    int fd = fileno(file);
    if(fd < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_fstat: unable to fileno()");
        return false;
    }
    if(fstat(fd, statbuf) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_fstat: unable to fstat()");
        return false;
    }
    return true;
}

void mq_filesystem_file_close(mqFilesystem *fs, mqFilesystemFile *file)
{
    (void)fs;
    fclose(file);
}

//=== search interface ======================================================//

bool mq_filesystem_search_stat(mqFilesystem *fs,
        mqFilesystemSearch *search, struct stat *statinfo)
{
    if(!statinfo)
        return false;

    if(search->pos < 0 || search->pos >= (int)search->glob.gl_pathc) {
        mq_log(MQ_LOG_ERROR, "fs_search_stat: invalid pos");
        return false;
    }
    char *phys_pathname = search->glob.gl_pathv[search->pos];
    if(stat(phys_pathname, statinfo) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_search_stat: unable to stat()");
        return false;
    }
    return true;
}

mqFilesystemSearch *mq_filesystem_search_open(mqFilesystem *fs,
        char const *pattern)
{
    char phys_pattern[1024];
    mqFilesystemSearch *search;

    if(!pattern) {
        mq_log(MQ_LOG_ERROR, "fs_search_open: broken arguments");
        return NULL;
    }
    if(!_filesystem_gen_phys_pathname(fs, phys_pattern, pattern, 1024))
        return NULL;
    search = (mqFilesystemSearch*)calloc(1, sizeof(mqFilesystemFile));
    if(!search) {
        mq_log(MQ_LOG_ERROR, "fs_search_open: unable to calloc()");
        return NULL;
    }
    // TODO: glob probably doesn't return . and .. when iterating over a folder
    if(glob(phys_pattern, 0, NULL, &(search->glob)) == GLOB_NOMATCH) {
        mq_log(MQ_LOG_ERROR, "fs_search_open: unable to glob()");
        free(search);
        return NULL;
    }
    search->pos = -1;
    // TODO: Make it start at 0 like any self-respecting array index
    return search;
}

bool mq_filesystem_search_next(mqFilesystem *fs,
        mqFilesystemSearch *search, char *buffer, size_t n)
{
    if(!buffer)
        return false;
    if(search->pos + 1 >= (int)search->glob.gl_pathc) {
        mq_log(MQ_LOG_ERROR, "fs_search_next: invalid pos");
        return false;
    }
    search->pos += 1;
    bool ok = _filesystem_gen_virt_pathname(fs,
            buffer, search->glob.gl_pathv[search->pos], n);
    if(!ok) {
        mq_log(MQ_LOG_ERROR, "fs_search_next: gen virt path error");
        return false;
    }
    return true;
}

void mq_filesystem_search_close(mqFilesystem *fs, mqFilesystemSearch *search)
{
    (void)fs;
    globfree(&search->glob);
}
