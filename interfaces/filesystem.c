//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/interfaces/filesystem.h>
#include <string.h>
#include <stdlib.h>

//=== Helpers

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

//=== info

mqFilesystem *mq_filesystem_create(void)
{
    mqFilesystem *fs = (mqFilesystem *)calloc(1, sizeof(mqFilesystem));
    if (!fs) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_create: calloc() fail");
        return NULL;
    }
    return fs;
}

void mq_filesystem_destroy(mqFilesystem **fs)
{
    if(fs == NULL || *fs == NULL) {
        mq_log(MQ_LOG_ERROR, "mq_filesystem_destroy: broken arguments");
        return;
    }
    free(*fs);
    *fs = NULL;
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

//=== POSIX interface

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
