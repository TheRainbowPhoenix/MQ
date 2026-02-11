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
    if(!fs) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_phys_patname: internal error");
        return false;
    }
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_phys_patname: no root URI");
        return false;
    }
    if(n < fs->root_uri_len) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_phys_patname: too short buffer");
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
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_patname: internal error");
        return false;
    }
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_patname: no root URI");
        return false;
    }
    char const*source = &(pathname[fs->root_uri_len]);
    if(strlen(source) >= n) {
        mq_log(MQ_LOG_ERROR, "_fs_gen_virt_patname: too short buffer");
        return false;
    }
    mq_log(MQ_LOG_DEBUG, "_fs_gen_virt_patname: generated -> %s", source);
    strcpy(output, source);
    return true;
}

//=== system interface ======================================================//

mqFilesystem *mq_filesystem_interface_create(void)
{
    mqFilesystem *fs = (mqFilesystem *)calloc(1, sizeof(mqFilesystem));
    if (!fs) {
        mq_log(MQ_LOG_ERROR, "fs_interface_create: calloc() fail");
        return NULL;
    }
    return fs;
}

bool mq_filesystem_interface_initialize(mqFilesystem *fs, int fs_type)
{
    if(!fs) {
        mq_log(MQ_LOG_ERROR, "fs_interface_destroy: broken arguments");
        return false;
    }
    //todo: support specific fs-type initialization
    (void)fs_type;
    return true;
}

bool mq_filesystem_interface_destroy(mqFilesystem **fs)
{
    if(!fs || *fs == NULL) {
        mq_log(MQ_LOG_ERROR, "fs_interface_destroy: broken arguments");
        return false;
    }
    free(*fs);
    *fs = NULL;
    return true;
}

bool mq_filesystem_interface_set_root_uri(mqFilesystem *fs,
        char const *pathname)
{
    if(!fs || !pathname) {
        mq_log(MQ_LOG_ERROR, "fs_interface_set_uri: broken arguments");
        return false;
    }
    if(fs->root_uri)
        free(fs->root_uri);
    fs->root_uri_len = strlen(pathname) + 1;
    fs->root_uri = (char*)calloc(1, fs->root_uri_len + 1);
    if(!fs->root_uri) {
        mq_log(MQ_LOG_ERROR, "fs_interface_set_uri: unable to strdup()");
        fs->root_uri_len = 0;
        return false;
    }
    strcpy(fs->root_uri, pathname);
    strcat(fs->root_uri, "/");
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

    if(!fs || !virt_pathname) {
        mq_log(MQ_LOG_ERROR, "fs_file_create: broken arguments");
        return false;
    }
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
    if(ret < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_create: unable mkdir() or creat()");
        return false;
    }
    return true;
}

bool mq_filesystem_file_delete(mqFilesystem *fs,
        char const *virt_pathname)
{
    char phys_pathname[1024];

    if(!fs || !virt_pathname) {
        mq_log(MQ_LOG_ERROR, "fs_file_delete: broken arguments");
        return false;
    }
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

    if(!fs || !virt_pathname || !statbuf) {
        mq_log(MQ_LOG_ERROR, "fs_file_stat: broken arguments");
        return false;
    }
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

    if(!fs || !virt_pathname || !mode) {
        mq_log(MQ_LOG_ERROR, "fs_file_open: broken arguments");
        return NULL;
    }
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
    if(!fs || !file || !buff) {
        mq_log(MQ_LOG_ERROR, "fs_file_read: broken arguments");
        return 0;
    }
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
    if(!fs || !file || !buff) {
        mq_log(MQ_LOG_ERROR, "fs_file_write: broken arguments");
        return -1;
    }
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
    if(!fs || !file) {
        mq_log(MQ_LOG_ERROR, "fs_file_lseek: broken arguments");
        return false;
    }
    if(fseek(file, offset, whence) < 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_lseek: unable to fseek()");
        return false;
    }
    return true;
}

bool mq_filesystem_file_fstat(mqFilesystem *fs,
        mqFilesystemFile *file, struct stat *statbuf)
{
    if(!fs || !file || !statbuf) {
        mq_log(MQ_LOG_ERROR, "fs_file_fstat: broken arguments");
        return false;
    }
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

bool mq_filesystem_file_close(mqFilesystem *fs,
        mqFilesystemFile **file)
{
    if(!fs || !file || (*file == NULL)) {
        mq_log(MQ_LOG_ERROR, "fs_file_close: broken arguments");
        return false;
    }
    if (fclose(*file) != 0) {
        mq_log(MQ_LOG_ERROR, "fs_file_close: unable to fclose()");
        return false;
    }
    *file = NULL;
    return true;
}

//=== search interface ======================================================//

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
    if(glob(phys_pattern, 0, NULL, &(search->glob)) == GLOB_NOMATCH) {
        mq_log(MQ_LOG_ERROR, "fs_search_open: unable to glob()");
        free(search);
        return NULL;
    }
    search->pos = -1;
    return search;
}

bool mq_filesystem_search_next(mqFilesystem *fs,
        mqFilesystemSearch *search, char *buffer, size_t n)
{
    if(!fs || !search || !buffer) {
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
    return true;
}

bool mq_filesystem_search_close(mqFilesystem *fs,
        mqFilesystemSearch **search)
{
    if(!fs || !search || (*search == NULL)) {
        mq_log(MQ_LOG_ERROR, "fs_search_close: broken arguments");
        return false;
    }
    globfree(&((*search)->glob));
    *search = NULL;
    return true;
}
