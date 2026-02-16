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

static int _bfile_dtable_reserve(mqBfile *bfile, void **dtable)
{
    if(!dtable)
        return -1;

    for(uint i = 0 ; i < bfile->dtable_nb_slot ; i++) {
        if(dtable[i] == NULL)
            return i;
    }

    mq_log(MQ_LOG_WARNING, "bfile: (some) descriptor table out of free slots");
    return -1;
}

static void *_bfile_dtable_get(mqBfile *bfile, void **dtable, int slot)
{
    if((uint)slot >= bfile->dtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get: invalid slot %d", slot);
        return NULL;
    }
    if(dtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get: slot %d unused", slot);
        return NULL;
    }
    return dtable[slot];
}

static mqFilesystemFile *_bfile_dtable_get_file(mqBfile *bfile, int slot)
{
    return _bfile_dtable_get(bfile, (void **)bfile->file_dtable, slot);
}

static mqFilesystemSearch *_bfile_dtable_get_search(mqBfile *bfile, int slot)
{
    return _bfile_dtable_get(bfile, (void **)bfile->search_dtable, slot);
}

//=== system interface ======================================================//

mqBfile *mq_bfile_create(int fdtable_nb_slot)
{
    mqBfile *bfile;
    mqFilesystemFile **file_dtable;
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
    return mq_filesystem_file_delete(mach->fs, pathname) ? 0 : -1;
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
        return mq_filesystem_file_create(mach->fs, pathname, false) ? 0 : -1;
    }
    else if(mode == BFILE_CREATEMODE_FOLDER) {
        mq_log(MQ_LOG_DEBUG, "Bfile_CreateEntry: Creating dir %s", pathname);
        return mq_filesystem_file_create(mach->fs, pathname, true) ? 0 : -1;
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
    char const *bits;

    if(!_bfile_uri_conv8(mach, pathname, pathnameAddr, 1024)) {
        mq_log(MQ_LOG_ERROR, "Bfile_OpenFile: unable to verify the path");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_OpenFile: %s - %d", pathname, mode);
    if(mode == BFILE_MODE_READ || mode == BFILE_MODE_READ_SHARE)
        bits = "rb";
    else if(mode == BFILE_MODE_WRITE)
        bits = "wb";
    else if(mode == BFILE_MODE_READWRITE || mode == BFILE_MODE_READWRITE_SHARE)
        bits = "r+b";
    else {
        mq_log(MQ_LOG_ERROR, "Bfile_OpenFile: invalid mode %d", mode);
        return -1;
    }
    void **dtable = (void**)mach->bfile->file_dtable;
    int slot = _bfile_dtable_reserve(mach->bfile, dtable);
    if(slot < 0)
        return -1;

    dtable[slot] = mq_filesystem_file_open(mach->fs, pathname, bits);
    return dtable[slot] ? slot : -1;
}

int mq_bfile_SeekFile(mqMachine *mach, int fd, int pos)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_SeekFile: %d - %d", fd, pos);
    mqFilesystemFile *fp = _bfile_dtable_get_file(mach->bfile, fd);
    if(!fp || !mq_filesystem_file_lseek(mach->fs, fp, pos, SEEK_SET)) {
        mq_log(MQ_LOG_ERROR, "Bfile_SeekFile: seek error");
        return -1;
    }
    return 0;
}

int mq_bfile_ReadFile(mqMachine *mach,
        int fd, u32 buffAddr, int size, int readpos)
{
    u8 buffer[1024];
    u32 read_size;
    u32 need_size;
    u32 try_size;

    mq_log(MQ_LOG_DEBUG,
            "Bfile_ReadFile: fd=%d && size=%d && seek=%d",
            fd, size, readpos);

    mqFilesystemFile *fp = _bfile_dtable_get_file(mach->bfile, fd);
    if(!fp)
        return -1;

    void *buffVirt = mq_memory_access(mach->memory, buffAddr);
    if(!buffVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: invalid buffer address");
        return -1;
    }
    if(readpos >= 0
       && !mq_filesystem_file_lseek(mach->fs, fp, readpos, SEEK_SET)) {
        mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: unable to fs_lseek()");
        return -1;
    }
    read_size = 0;
    while(size > 0) {
        need_size = (size > 1024) ? 1024 : size;
        try_size = mq_filesystem_file_read(mach->fs, fp, buffer, need_size);
        for(u32 j = 0 ; j < try_size ; j++)
            mq_buffer_write8(buffVirt, read_size + j, buffer[j]);
        if(try_size == 0) {
            mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: expected %d bytes, got 0",
                need_size);
            return read_size;
        }
        read_size += try_size;
        size -= try_size;
    }
    return read_size;
}

int mq_bfile_WriteFile(mqMachine *mach,
        int fd, u32 buffAddr, int size)
{
    u8 buffer[1024];
    u32 write_size;
    u32 need_size;
    u32 try_size;

    if(!mach->bfile)
        return -1;

    mq_log(MQ_LOG_DEBUG, "Bfile_WriteFile: fd=%d && size=%d", fd, size);
    mqFilesystemFile *fp = _bfile_dtable_get_file(mach->bfile, fd);
    if(!fp)
        return -1;

    void *buffVirt = mq_memory_access(mach->memory, buffAddr);
    if(!buffVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_WriteFile: invalid buffer address");
        return 0;
    }
    write_size = 0;
    while(size > 0) {
        need_size = (size > 1024) ? 1024 : size;
        for(u32 j = 0 ; j < need_size ; j++)
            buffer[j] = mq_buffer_read8(buffVirt, write_size + j);
        try_size = mq_filesystem_file_write(mach->fs, fp, buffer, need_size);
        if(try_size == 0) {
            mq_log(MQ_LOG_ERROR, "Bfile_WriteFile: sent %d bytes, wrote 0",
                need_size);
            return write_size;
        }
        write_size += try_size;
        size -= try_size;
    }
    return write_size;
}

int mq_bfile_GetFileSize(mqMachine *mach, int fd)
{
    struct stat statbuf;

    mq_log(MQ_LOG_DEBUG, "Bfile_GetFileSize: fd == %d", fd);
    mqFilesystemFile *fp = _bfile_dtable_get_file(mach->bfile, fd);
    if(!fp)
        return -1;

    if(!mq_filesystem_file_fstat(mach->fs, fp, &statbuf)) {
        mq_log(MQ_LOG_ERROR, "Bfile_GetFileSize: unnable to fs_fstat()");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_GetFileSize: found == %d", statbuf.st_size);
    return statbuf.st_size;
}

int mq_bfile_CloseFile(mqMachine *mach, int fd)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_CloseFile: fd == %d", fd);
    mqFilesystemFile *fp = _bfile_dtable_get_file(mach->bfile, fd);
    if(!fp)
        return -1;

    mq_filesystem_file_close(mach->fs, fp);
    mach->bfile->file_dtable[fd] = NULL;
    return 0;
}

//=== search interface ======================================================//

int mq_bfile_FindFirst(mqMachine *mach,
    u32 patternAddr, u32 fdAddr, u32 foundAddr, u32 fileinfoAddr)
{
    char pattern[1024];
    int rc;

    void *fdVirt = mq_memory_access(mach->memory, fdAddr);
    if(!fdVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: invalid fdAddr");
        return -1;
    }
    mq_buffer_write32(fdVirt, 0, -1);
    if(!_bfile_uri_conv8(mach, pattern, patternAddr, 1024)) {
        mq_log(MQ_LOG_DEBUG, "Bfile_FindFirst: unable to verify the path");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindFirst: pattern=%s", pattern);
    void **dtable = (void**)mach->bfile->search_dtable;
    int slot = _bfile_dtable_reserve(mach->bfile, dtable);
    if(slot < 0)
        return -1;

    dtable[slot] = mq_filesystem_search_open(mach->fs, pattern);
    if(dtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: unable to fs_search_open()");
        return -1;
    }
    rc = mq_bfile_FindNext(mach, slot, foundAddr, fileinfoAddr);
    if (rc < 0) {
        // TODO: Difference between no result and internal error?
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: unable to FindNext()");
        dtable[slot] = NULL;
        return -1;
    }
    mq_buffer_write32(fdVirt, 0, slot);
    return rc;
}

int mq_bfile_FindNext(mqMachine *mach,
        int fd, u32 foundAddr, u32 fileinfoAddr)
{
    char pathname[1024];
    struct stat statbuf;

    if(!mach->bfile) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: internal error");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindNext: fd == %d", fd);
    mqFilesystemSearch *search = _bfile_dtable_get_search(mach->bfile, fd);
    if(!search)
        return -1;

    if(!mq_filesystem_search_next(mach->fs, search, pathname, 1024)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to fs_search_next()");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindNext: fs_next() == %s", pathname);
    if(!mq_filesystem_search_stat(mach->fs, search, &statbuf)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to stat()");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindNext: fs_stat() == %d", statbuf.st_size);

    char const *basename = strrchr(pathname, '/');
    basename = basename ? basename + 1 : pathname;

    if(!_bfile_uri_conv_set16(mach, foundAddr, basename)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to uri_conv_set16()");
        return -1;
    }
    return _bfile_stat_set(mach, fileinfoAddr, pathname, &statbuf);
}

int mq_bfile_FindClose(mqMachine *mach, int fd)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_FindClose: fd == %d", fd);
    mqFilesystemSearch *search = _bfile_dtable_get_search(mach->bfile, fd);
    if(!search)
        return -1;

    mq_filesystem_search_close(mach->fs, search);
    mach->bfile->search_dtable[fd] = NULL;
    return 0;
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
    if(!mq_filesystem_file_stat(mach->fs, pathname, &statbuf)) {
        mq_log(MQ_LOG_ERROR, "Bfile_GetFileInfo: unable to fs_file_stat()");
        return -1;
    }
    return _bfile_stat_set(mach, fileinfoAddr, pathname, &statbuf);
}
