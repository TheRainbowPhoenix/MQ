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

static bool _filesystem_gen_real_pathname(mqFilesystem *fs,
        char *output, char const *pathname, size_t n)
{
    if(!fs) {
        mq_log(MQ_LOG_ERROR, "filesystem_real_patname: internal error");
        return false;
    }
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "filesystem_real_patname: no root URI");
        return false;
    }
    if(n < fs->root_uri_len) {
        mq_log(MQ_LOG_ERROR, "filesystem_real_patname: too short buffer");
        return false;
    }
    strncpy(&output[0], fs->root_uri, n);
    strncat(&output[fs->root_uri_len], pathname, n - fs->root_uri_len);
    return true;
}

static bool _filesystem_gen_virt_pathname(mqFilesystem *fs,
        char *output, char const *pathname, size_t n)
{
    if(!fs) {
        mq_log(MQ_LOG_ERROR, "filesystem_virt_patname: internal error");
        return false;
    }
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "filesystem_virt_patname: no root URI");
        return false;
    }
    char const*source = &(pathname[fs->root_uri_len]);
    if(strlen(source) >= n) {
        mq_log(MQ_LOG_ERROR, "filesystem_virt_patname: too short buffer");
        return false;
    }
    mq_log(MQ_LOG_DEBUG, "gen_virt_path: %s", source);
    strcpy(output, source);
    return true;
}

//=== system interface ======================================================//

mqFilesystem *mq_filesystem_create(void)
{
    mqFilesystem *fs = (mqFilesystem *)calloc(1, sizeof(mqFilesystem));
    if (!fs) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_create: calloc() fail");
        return NULL;
    }
    return fs;
}

bool mq_filesystem_initialize(mqFilesystem *fs, int fs_type)
{
    if(!fs) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_destroy: broken arguments");
        return false;
    }
    //todo: support specific fs-type initialization
    (void)fs_type;
    return true;
}

bool mq_filesystem_destroy(mqFilesystem **fs)
{
    if(fs == NULL || *fs == NULL) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_destroy: broken arguments");
        return false;
    }
    free(*fs);
    *fs = NULL;
    return true;
}

bool mq_filesystem_set_root_uri(mqFilesystem *fs, char const *pathname)
{
    if(!fs || !pathname) {
        mq_log(MQ_LOG_ERROR, "fs_set_uri: broken arguments");
        return false;
    }
    if(fs->root_uri)
        free(fs->root_uri);
    fs->root_uri_len = strlen(pathname) + 1;
    fs->root_uri = (char*)calloc(1, fs->root_uri_len + 1);
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "fs_set_uri: unable to strdup()");
        fs->root_uri_len = 0;
        return false;
    }
    strcpy(fs->root_uri, pathname);
    strcat(fs->root_uri, "/");
    mq_log(MQ_LOG_DEBUG, "fs_set_root_uri: switch root for %s", fs->root_uri);
    return true;
}

//=== POSIX interface =======================================================//

bool mq_filesystem_create_file(mqFilesystem *fs,
 char const *pathname, bool is_dir)
{
    char real_pathname[1024];
    struct stat st;
    int ret;

    if(!pathname) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_open: broken arguments");
        return false;
    }
    if(!_filesystem_gen_real_pathname(fs, real_pathname, pathname, 1024))
        return false;
    if(stat(real_pathname, &st) != -1) {
        mq_log(MQ_LOG_ERROR,
                "filesystem_create: file already exists %s", real_pathname);
        return false;
    }
    if(is_dir) {
        ret = mkdir(real_pathname, 0700);
    } else {
        ret = creat(real_pathname, 0644);
    }
    if(ret < 0) {
        mq_log(MQ_LOG_ERROR,
                "filesystem_create: unable to create the file %s",
                real_pathname);
        return false;
    }
    return true;
}

bool mq_filesystem_delete_file(mqFilesystem *fs,
        char const *pathname)
{
    char real_pathname[1024];

    if(!fs || !pathname) {
        mq_log(MQ_LOG_ERROR, "fs_delete: broken arguments");
        return false;
    }
    if(!_filesystem_gen_real_pathname(fs, real_pathname, pathname, 1024))
        return false;
    mq_log(MQ_LOG_DEBUG, "fs_delete(): path == %s", real_pathname);
    if(remove(real_pathname) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_delete: unable to remove()");
        return false;
    }
    return true;
}

bool mq_filesystem_stat(mqFilesystem *fs,
        char const *pathname, struct stat *statbuf)
{
    char real_pathname[1024];

    if(!pathname) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_open: broken arguments");
        return false;
    }
    if(!_filesystem_gen_real_pathname(fs, real_pathname, pathname, 1024))
        return false;
    if(stat(real_pathname, statbuf) < 0) {
        mq_log(MQ_LOG_ERROR,
                "filesystem_stat: unable to stat() %s", real_pathname);
        return false;
    }
    return true;
}

mqFilesystemFile *mq_filesystem_open(mqFilesystem *fs,
        char const *pathname, char const *mode)
{
    char real_pathname[1024];

    if(!pathname || !mode) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_open: broken arguments");
        return NULL;
    }
    if(!_filesystem_gen_real_pathname(fs, real_pathname, pathname, 1024))
        return NULL;
    FILE *fp = fopen(real_pathname, mode);
    if(!fp) {
        mq_log(MQ_LOG_ERROR,
                "mq_filesystem_open: unable to open file `%s` with `%s`",
                real_pathname, mode);
        return NULL;
    }
    return fp;
}

u32 mq_filesystem_read(mqFilesystem *fs,
        mqFilesystemFile *file, void *buff, u32 count)
{
    if(!fs || !file || !buff) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_open: broken arguments");
        return 0;
    }
    mq_log(MQ_LOG_DEBUG, "fs_read(): count == %d", count);
    return fread(buff, sizeof(u8), count, file);
}

u32 mq_filesystem_write(mqFilesystem *fs,
        mqFilesystemFile *file, void *buff, u32 count)
{
    if(!fs || !file || !buff) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_open: broken arguments");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "fs_write(): count == %d", count);
    return fwrite(buff, sizeof(u8), count, file);
}

bool mq_filesystem_lseek(mqFilesystem *fs,
        mqFilesystemFile *file, int offset, int whence)
{
    if(!fs || !file) {
        mq_log(MQ_LOG_ERROR, "fs_lseek: broken arguments");
        return false;
    }
    mq_log(MQ_LOG_DEBUG,
            "fs_lseek: offset==%d && whence==%d", offset, whence);
    if(fseek(file, offset, whence) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_lseek: unable to fseek()");
        return false;
    }
    return true;
}

bool mq_filesystem_fstat(mqFilesystem *fs,
        mqFilesystemFile *file, struct stat *statbuf)
{
    if(!fs || !file || !statbuf) {
        mq_log(MQ_LOG_ERROR, "fs_fstat(): broken arguments");
        return false;
    }
    int fd = fileno(file);
    if(fd < 0) {
        mq_log(MQ_LOG_ERROR, "fs_fstat(): unable to fileno()");
        return false;
    }
    if(fstat(fd, statbuf) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_fstat(): unable to fstat()");
        return false;
    }
    return true;
}

bool mq_filesystem_close(mqFilesystem *fs,
        mqFilesystemFile **file)
{
    if(!fs || !file || (*file == NULL)) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_open: broken arguments");
        return false;
    }
    fclose(*file);
    *file = NULL;
    return true;
}

//=== search functions ======================================================//

bool mq_filesystem_search_stat(mqFilesystem *fs,
        mqFilesystemSearch *search, struct stat *statinfo)
{
    if(!fs || !search || !statinfo) {
        mq_log(MQ_LOG_ERROR, "fs_search_stat: broken arguments");
        return false;
    }
    if(search->pos < 0 || search->pos >= (int)search->glob.gl_pathc) {
        mq_log(MQ_LOG_ERROR, "fs_search_stat: invalid pos");
        return false;
    }
    char *pathname = search->glob.gl_pathv[search->pos];
    mq_log(MQ_LOG_DEBUG, "fs_search_stat: filename == %s", pathname);
    if(stat(pathname, statinfo) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_search_stat: unable to stat()");
        return false;
    }
    return true;
}

mqFilesystemSearch *mq_filesystem_search_open(mqFilesystem *fs,
        char const *pattern)
{
    char real_pattern[1024];
    mqFilesystemSearch *search;
    int rc;

    if(!pattern) {
        mq_log(MQ_LOG_ERROR, "fs_search_open(): broken arguments");
        return NULL;
    }
    if(!_filesystem_gen_real_pathname(fs, real_pattern, pattern, 1024))
        return NULL;
    search = (mqFilesystemSearch*)calloc(1, sizeof(mqFilesystemFile));
    if(!search) {
        mq_log(MQ_LOG_ERROR, "fs_search_open(): unable to callo()");
        return NULL;
    }
    rc = glob(real_pattern, 0, NULL, &(search->glob));
    printf("fs_search_open(): Searching %s: %zu results\n", real_pattern,
        (rc == GLOB_NOMATCH) ? 0 : search->glob.gl_pathc);
    if(rc == GLOB_NOMATCH) {
        free(search);
        return NULL;
    }
    search->pos = -1;
    return search;
}

bool mq_filesystem_search_next(mqFilesystem *fs,
        mqFilesystemSearch *search, char *buffer, size_t n)
{
    if(!fs || !search) {
        mq_log(MQ_LOG_ERROR, "fs_search_next: broken arguments");
        return false;
    }
    if(search->pos >= (int)search->glob.gl_pathc) {
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
    mq_log(MQ_LOG_DEBUG, "fs_search_next: found == %s", buffer);
    return true;
}

bool mq_filesystem_search_close(mqFilesystem *fs,
        mqFilesystemSearch **search)
{
    if(!fs || !search || (*search == NULL)) {
        mq_log(MQ_LOG_ERROR, "fs_search_open: broken arguments");
        return false;
    }
    globfree(&((*search)->glob));
    *search = NULL;
    return true;
}
