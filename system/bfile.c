//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/system/bfile.h>
#include <mq/memory.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

//=== path manipulation =====================================================//

/* convert the virtual u16 string
 * - convert to `char` (todo: support SHIFT-JIS)
 * - skip device information (only fls0)
 * - replace all "\" into "/" */
static bool _bfile_uri_conv8(mqMachine *mach,
        char *buffer, u32 sourceAddr, size_t n)
{
    size_t i = 0;
    size_t j = 0;
    u16 curr;

    void *src = mq_memory_access(mach->memory, sourceAddr);
    if (!src) {
        mq_log(MQ_LOG_ERROR, "_uri_conv8: invalid sourceAddr");
        return false;
    }
    while (i < n) {
        curr = mq_buffer_read16(src, j);
        buffer[i] = (curr >> 0) & 0xff;
        buffer[i] = (buffer[i] == '\\') ? '/' : buffer[i];
        if(curr == 0x0000 || curr == 0xffff)
            break;
        if(curr > 0x7f)
            buffer[++i] = (curr >> 8) & 0xff;
        i += 1;
        j += 2;
    }
    buffer[i] = 0;
    if(!memcmp(buffer, "//crd0/", 7)) {
        mq_log(MQ_LOG_ERROR, "_bfile_uri_conv8: unsupported crd0");
        return false;
    }
    if(!memcmp(buffer, "//fls0/", 7)) {
        memmove(&buffer[0], &buffer[7], i - 7 + 1 /* NUL */);
        return true;
    }
    mq_log(MQ_LOG_DEBUG, "_bfile_uri_conv8: unsupported path `%s`", buffer);
    return false;
}

/* convert the "internal" pathname into u16
 * - convert `u16` (todo: support SHIFT-JS)
 * - replace all "/" into "\"
 * - add device information if requested */
static bool _bfile_uri_conv_set16(mqMachine *mach,
        u32 outputAddr, char const *pathname)
{
    void *outputVirt = mq_memory_access(mach->memory, outputAddr);
    if(!outputVirt) {
        mq_log(MQ_LOG_ERROR, "_bfile_uri_set16: invalid memory");
        return false;
    }

    for(int i = 0; pathname[i] != '\0'; i++) {
        mq_buffer_write16(outputVirt, 0,
                (pathname[i] != '/') ? pathname[i] : '\\');
        outputVirt += 2;
    }

    mq_buffer_write16(outputVirt, 0, 0x0000);
    return true;
}

//=== shared utilities ======================================================//

static bool _bfile_stat_set(mqMachine *mach,
        u32 fileinfoAddr, char const *pathname, struct stat *statbuf)
{
    void *fileinfo = mq_memory_access(mach->memory, fileinfoAddr);
    if(!fileinfo) {
        mq_log(MQ_LOG_ERROR, "_bfile_stat_set: invalid fileinfoAddr");
        return false;
    }
    char const *filename = strrchr(pathname, '/');
    if(!filename)
        filename = pathname;
    filename = &filename[(filename[0] == '/')];
    if(filename[0] == '\0') {
        mq_log(MQ_LOG_ERROR, "_bfile_stat_set: invalid filename");
        return false;
    }
    int type = BFILE_TYPE_FILE;
    int data_size = statbuf->st_size;
    if(!strcmp(filename, "."))
        type = BFILE_TYPE_DOT;
    if(!strcmp(filename, ".."))
        type = BFILE_TYPE_DOTDOT;
    char const *ext = strrchr(filename, '.');
    if(ext) {
        if(!strcmp(ext, ".g1a") || !strcmp(ext, ".G1A")) {
            type = BFILE_TYPE_ADDIN;
            data_size -= 0x200;
        }
        if(!strcmp(ext, ".g3a") || !strcmp(ext, ".G3A")) {
            type = BFILE_TYPE_ADDIN;
            data_size -= 0x7000;
        }
    }
    if(data_size < 0) {
        type = BFILE_CREATEMODE_FILE;
        data_size = statbuf->st_size;
    }
    if((statbuf->st_mode & S_IFMT) == S_IFDIR)
        type = BFILE_TYPE_DIRECTORY;
    mq_buffer_write16(fileinfo,  0, 0x00);
    mq_buffer_write16(fileinfo,  2, type);
    mq_buffer_write32(fileinfo,  4, statbuf->st_size);
    mq_buffer_write32(fileinfo,  8, data_size);
    mq_buffer_write32(fileinfo, 12, 0);
    mq_buffer_write32(fileinfo, 16, 0x00000000);
    return true;
}

//=== file/find descriptor table ============================================//

static int _bfile_dtable_reserve_file(mqBfile *bfile)
{
    for(uint i = 0 ; i < bfile->dtable_nb_slot ; i++) {
        if(bfile->file_dtable[i] == -1)
            return i;
    }
    mq_log(MQ_LOG_WARNING, "bfile: file descriptor table out of free slots");
    return -1;
}

static int _bfile_dtable_reserve_search(mqBfile *bfile)
{
    for(uint i = 0 ; i < bfile->dtable_nb_slot ; i++) {
        if(bfile->search_dtable[i] == NULL)
            return i;
    }
    mq_log(MQ_LOG_WARNING, "bfile: search descriptor table out of free slots");
    return -1;
}

static int _bfile_dtable_get_file(mqBfile *bfile, int handle)
{
    if ((handle & 0x0f000000) != 0x01000000) {
        mq_log(MQ_LOG_ERROR,
            "_bfile_datable_get_file: invalid fls0 handle %08x", handle);
        return -1;
    }
    int slot = handle & 0xf0ffffff;

    if((uint)slot >= bfile->dtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get_file: invalid slot %d", slot);
        return -1;
    }
    if(bfile->file_dtable[slot] == -1) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get_file: slot %d unused", slot);
        return -1;
    }
    return bfile->file_dtable[slot];
}

static mqFilesystemSearch *_bfile_dtable_get_search(mqBfile *bfile, int slot)
{
    if((uint)slot >= bfile->dtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get_search: invalid slot %d", slot);
        return NULL;
    }
    if(bfile->search_dtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get_search: slot %d unused", slot);
        return NULL;
    }
    return bfile->search_dtable[slot];
}

//=== system interface ======================================================//

mqBfile *mq_bfile_create(int fdtable_nb_slot)
{
    mqBfile *bfile;
    int *file_dtable;
    mqFilesystemSearch **search_dtable;

    bfile = calloc(1, sizeof *bfile);
    file_dtable = calloc(fdtable_nb_slot, sizeof *file_dtable);
    search_dtable = calloc(fdtable_nb_slot, sizeof *search_dtable);

    if(!bfile || !file_dtable || !search_dtable) {
        free(bfile);
        free(file_dtable);
        free(search_dtable);
        return NULL;
    }

    bfile->file_dtable = file_dtable;
    bfile->search_dtable = search_dtable;
    bfile->dtable_nb_slot = fdtable_nb_slot;
    for(uint i = 0; i < bfile->dtable_nb_slot; i++)
        bfile->file_dtable[i] = -1;
    return bfile;
}

void mq_bfile_destroy(mqBfile *bfile)
{
    if(bfile) {
        free(bfile->file_dtable);
        free(bfile->search_dtable);
        free(bfile);
    }
}

//=== storage interface =====================================================//

int mq_bfile_DeleteEntry(mqMachine *mach, u32 pathnameAddr)
{
    char pathname[1024];

    if(!_bfile_uri_conv8(mach, pathname, pathnameAddr, 1024)) {
        mq_log(MQ_LOG_ERROR, "Bfile_DeleteEntry: unable to verify the path");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_DeleteEntry: %s", pathname);
    int rc = mq_filesystem_unlink(mach->fs, pathname);
    if(rc < 0 && errno == EISDIR)
        rc = mq_filesystem_rmdir(mach->fs, pathname);
    return rc < 0 ? -1 : 0;
}

int mq_bfile_CreateEntry(mqMachine *mach,
        u32 pathnameAddr, int mode, u32 sizeAddr)
{
    char pathname[1024];

    (void)sizeAddr;
    if(!_bfile_uri_conv8(mach, pathname, pathnameAddr, 1024)) {
        mq_log(MQ_LOG_ERROR, "Bfile_CreateEntry: unable to verify the path");
        return -1;
    }
    if(mode == BFILE_CREATEMODE_FILE) {
        mq_log(MQ_LOG_DEBUG, "Bfile_CreateEntry: Creating file %s", pathname);
        return mq_filesystem_creat(mach->fs, pathname, 0644) < 0 ? -1 : 0;
    }
    else if(mode == BFILE_CREATEMODE_FOLDER) {
        mq_log(MQ_LOG_DEBUG, "Bfile_CreateEntry: Creating dir %s", pathname);
        int rc = mq_filesystem_mkdir(mach->fs, pathname, 0755);
        if(rc >= 0)
            return 0;
        else if(errno == EEXIST)
            return -13; // AlreadyExists
        else
            return -2; // IllegalParam
    }
    else {
        mq_log(MQ_LOG_ERROR, "Bfile_CreateEntry: Invalid mode %d", mode);
        return -1;
    }
}

//=== file interface ========================================================//

int mq_bfile_OpenFile(mqMachine *mach,
        u32 pathnameAddr, int mode)
{
    char pathname[1024];
    int flags = 0;

    if(!_bfile_uri_conv8(mach, pathname, pathnameAddr, 1024)) {
        mq_log(MQ_LOG_ERROR, "Bfile_OpenFile: unable to verify the path");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_OpenFile: %s - %d", pathname, mode);

    if(mode == BFILE_MODE_READ || mode == BFILE_MODE_READ_SHARE)
        flags = O_RDONLY;
    else if(mode == BFILE_MODE_WRITE)
        flags = O_WRONLY;
    else if(mode == BFILE_MODE_READWRITE || mode == BFILE_MODE_READWRITE_SHARE)
        flags = O_RDWR;
    else {
        mq_log(MQ_LOG_ERROR, "Bfile_OpenFile: invalid mode %d", mode);
        return -1;
    }
    int slot = _bfile_dtable_reserve_file(mach->bfile);
    if(slot < 0)
        return -1;

    int fd = mq_filesystem_open(mach->fs, pathname, flags, 0);
    mach->bfile->file_dtable[slot] = (fd >= 0) ? fd : -1;
    return (fd >= 0) ? (slot | 0x01000000) : -1;
}

int mq_bfile_SeekFile(mqMachine *mach, int handle, int pos)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_SeekFile: %08x - %d", handle, pos);
    int fd = _bfile_dtable_get_file(mach->bfile, handle);
    if(fd < 0 || mq_filesystem_lseek(mach->fs, fd, pos, SEEK_SET) < 0) {
        mq_log(MQ_LOG_ERROR, "Bfile_SeekFile: seek error");
        return -1;
    }
    return 0;
}

int mq_bfile_ReadFile(mqMachine *mach,
    int handle, u32 buffAddr, int size, int readpos)
{
    u8 buffer[1024];
    size_t read_size = 0;

    mq_log(MQ_LOG_DEBUG, "Bfile_ReadFile(%08x, size: %d, pos: %d)",
        handle, size, readpos);

    int fd = _bfile_dtable_get_file(mach->bfile, handle);
    if(fd < 0)
        return -1;

    void *buffVirt = mq_memory_access(mach->memory, buffAddr);
    if(!buffVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: invalid buffer address");
        return -1;
    }
    if(readpos >= 0 && !mq_filesystem_lseek(mach->fs, fd, readpos, SEEK_SET)) {
        mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: unable to fs_lseek()");
        return -1;
    }

    while(size > 0) {
        size_t need_size = (size > 1024) ? 1024 : size;
        ssize_t rc = mq_filesystem_read(mach->fs, fd, buffer, need_size);
        if(rc <= 0) {
            mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: read of %d bytes -> %zd",
                need_size, rc);
            return read_size;
        }
        for(u32 j = 0 ; j < rc ; j++)
            mq_buffer_write8(buffVirt, read_size + j, buffer[j]);
        read_size += rc;
        size -= rc;
    }
    return read_size;
}

int mq_bfile_WriteFile(mqMachine *mach, int handle, u32 buffAddr, int size)
{
    u8 buffer[1024];
    size_t write_size = 0;

    mq_log(MQ_LOG_DEBUG, "Bfile_WriteFile(%08x, size %d)", handle, size);
    int fd = _bfile_dtable_get_file(mach->bfile, handle);
    if(!fd)
        return -1;

    void *buffVirt = mq_memory_access(mach->memory, buffAddr);
    if(!buffVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_WriteFile: invalid buffer address");
        return 0;
    }

    while(size > 0) {
        size_t need_size = (size > 1024) ? 1024 : size;
        for(u32 j = 0 ; j < need_size ; j++)
            buffer[j] = mq_buffer_read8(buffVirt, write_size + j);
        ssize_t rc = mq_filesystem_write(mach->fs, fd, buffer, need_size);
        if(rc <= 0) {
            mq_log(MQ_LOG_ERROR, "Bfile_WriteFile: write of %d bytes -> %zd",
                need_size, rc);
            return write_size;
        }
        write_size += rc;
        size -= rc;
    }
    return write_size;
}

int mq_bfile_GetFileSize(mqMachine *mach, int handle)
{
    int fd = _bfile_dtable_get_file(mach->bfile, handle);
    if(fd < 0)
        return -1;

    struct stat statbuf;
    if(mq_filesystem_fstat(mach->fs, fd, &statbuf) < 0) {
        mq_log(MQ_LOG_DEBUG, "Bfile_GetFileSize(%0_x) -> error", handle);
        return -1;
    }

    mq_log(MQ_LOG_DEBUG, "Bfile_GetFileSize(%0_x) -> %d",
        handle, statbuf.st_size);
    return statbuf.st_size;
}

int mq_bfile_GetFilePos(mqMachine *mach, int handle)
{
    int fd = _bfile_dtable_get_file(mach->bfile, handle);
    if(fd < 0)
        return -1;

    int rc = mq_filesystem_lseek(mach->fs, fd, 0, SEEK_CUR);
    if(rc < 0) {
        mq_log(MQ_LOG_DEBUG, "Bfile_GetFilePos(%08x) -> error", handle);
        return -1;
    }

    mq_log(MQ_LOG_DEBUG, "Bfile_GetFilePos(%08x) -> %d", handle, rc);
    return rc;
}

int mq_bfile_GetFileInfo(mqMachine *mach,
        u32 pathnameAddr, u32 fileinfoAddr)
{
    char pathname[1024];
    struct stat statbuf;

    if(!_bfile_uri_conv8(mach, pathname, pathnameAddr, 1024)) {
        mq_log(MQ_LOG_DEBUG, "Bfile_GetFileInfo: unable to verify the path");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_GetFileInfo: pathname == %s", pathname);
    if(mq_filesystem_stat(mach->fs, pathname, &statbuf) < 0) {
        mq_log(MQ_LOG_ERROR, "Bfile_GetFileInfo: unable to fs_file_stat()");
        return -1;
    }
    if(!_bfile_stat_set(mach, fileinfoAddr, pathname, &statbuf)) {
        mq_log(MQ_LOG_ERROR, "Bfile_GetFileInfo: unable to write back stat");
        return BFILE_ERROR_BROKENSYSTEMDATA;
    }
    return 0;
}

int mq_bfile_CloseFile(mqMachine *mach, int handle)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_CloseFile(%08x)", handle);
    int fd = _bfile_dtable_get_file(mach->bfile, handle);
    if(fd < 0)
        return -1;

    mq_filesystem_close(mach->fs, fd);
    mach->bfile->file_dtable[handle & 0xf0ffffff] = -1;
    return 0;
}

//=== search interface ======================================================//

int mq_bfile_FindFirst(mqMachine *mach,
    u32 patternAddr, u32 fdAddr, u32 foundAddr, u32 fileinfoAddr)
{
    mqBfile *bfile = mach->bfile;
    char pattern[1024];

    void *fdVirt = mq_memory_access(mach->memory, fdAddr);
    if(!fdVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: invalid fdAddr");
        return BFILE_ERROR_ILLEGALPARAM;
    }
    mq_buffer_write32(fdVirt, 0, -1);
    if(!_bfile_uri_conv8(mach, pattern, patternAddr, 1024)) {
        mq_log(MQ_LOG_DEBUG, "Bfile_FindFirst: unable to verify the path");
        return BFILE_ERROR_ILLEGALPATH;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindFirst: pattern=%s", pattern);
    int slot = _bfile_dtable_reserve_search(bfile);
    if(slot < 0)
        return BFILE_ERROR_ENTRYFULL;

    bfile->search_dtable[slot] = mq_filesystem_search_open(mach->fs, pattern);
    if(bfile->search_dtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: unable to fs_search_open()");
        return BFILE_ERROR_ENTRYNOTFOUND;
    }
    if(mq_bfile_FindNext(mach, slot, foundAddr, fileinfoAddr) < 0) {
        bfile->search_dtable[slot] = NULL;
        return BFILE_ERROR_ENTRYNOTFOUND;
    }
    mq_buffer_write32(fdVirt, 0, slot);
    return 0;
}

int mq_bfile_FindNext(mqMachine *mach,
        int fd, u32 foundAddr, u32 fileinfoAddr)
{
    char pathname[1024];
    struct stat statbuf;

    mqFilesystemSearch *search = _bfile_dtable_get_search(mach->bfile, fd);
    if(!search)
        return BFILE_ERROR_ENTRYNOTFOUND;

    if(!mq_filesystem_search_next(mach->fs, search, pathname, 1024, &statbuf)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: done");
        return BFILE_ERROR_ENUMERATEEND;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindNext: %s (size %d)",
        pathname, statbuf.st_size);

    char const *basename = strrchr(pathname, '/');
    basename = basename ? basename + 1 : pathname;

    if(!_bfile_uri_conv_set16(mach, foundAddr, basename)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to uri_conv_set16()");
        return BFILE_ERROR_BROKENSYSTEMDATA;
    }
    if(!_bfile_stat_set(mach, fileinfoAddr, pathname, &statbuf)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to write back stat");
        return BFILE_ERROR_BROKENSYSTEMDATA;
     }
     return 0;
}

int mq_bfile_FindClose(mqMachine *mach, int fd)
{
    /* Silence error messages for this common case */
    if(fd < 0)
        return 0;

    mq_log(MQ_LOG_DEBUG, "Bfile_FindClose(%d)", fd);
    mqFilesystemSearch *search = _bfile_dtable_get_search(mach->bfile, fd);
    if(!search)
        return BFILE_ERROR_ENTRYNOTFOUND;

    mq_filesystem_search_close(mach->fs, search);
    mach->bfile->search_dtable[fd] = NULL;
    return 0;
}
