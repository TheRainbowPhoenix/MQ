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
        if(curr > 0x7f)
            buffer[++i] = (curr >> 8) & 0xff;
        if(curr == 0x0000 || curr == 0xffff)
            break;
        i += 1;
        j += 2;
    }
    if(!memcmp(buffer, "//crd0/", 7)) {
        mq_log(MQ_LOG_ERROR, "_bfile_uri_conv8: unsupported crd0");
        return false;
    }
    if(!memcmp(buffer, "//fls0/", 7)) {
        memcpy(&buffer[0], &buffer[7], i - 7);
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
        u32 outputAddr, char const *pathname, bool storage_prefix)
{
    void *outputVirt = mq_memory_access(mach->memory, outputAddr);
    if(!outputVirt) {
        mq_log(MQ_LOG_ERROR, "_bfile_uri_set16: invalid memory");
        return false;
    }
    u32 idx = 0;
    if(storage_prefix) {
        mq_buffer_write16(outputVirt, 0*2, '\\');
        mq_buffer_write16(outputVirt, 1*2, '\\');
        mq_buffer_write16(outputVirt, 2*2, 'f');
        mq_buffer_write16(outputVirt, 3*2, 'l');
        mq_buffer_write16(outputVirt, 4*2, 's');
        mq_buffer_write16(outputVirt, 5*2, '0');
        mq_buffer_write16(outputVirt, 6*2, '\\');
        idx = 7;
    }
    int i = -1;
    while(pathname[++i] != '\0') {
        mq_buffer_write16(outputVirt, (idx + i) * 2,
                (pathname[i] != '/') ? pathname[i] : '\\');
    }
    mq_buffer_write16(outputVirt, (idx + i) * 2, 0x0000);
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

static void **_bfile_dtable_reserve(mqBfile *bfile,
        void **dtable, int *slot)
{
    if(!dtable || !slot) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_reserve: internal error");
        return NULL;
    }
    for(int i = 0 ; i < bfile->dtable_nb_slot ; i++) {
        if(dtable[i] != NULL)
            continue;
        dtable[i] = (mqFilesystemFile*)-1;
        *slot = i;
        return &(dtable[i]);
    }
    return NULL;
}

static bool _bfile_dtable_release(mqBfile *bfile,
        void **dtable, int slot)
{
    if(!bfile || !dtable) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_release: internal error");
        return false;
    }
    if(slot < 0 || slot >= bfile->dtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_release: invalid slot");
        return false;
    }
    if(dtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_release: slot not used %d", slot);
        return false;
    }
    dtable[slot] = NULL;
    return true;
}

static void *_bfile_dtable_get(mqBfile *bfile,
        void **dtable, int slot)
{
    if(!bfile || !dtable) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get: internal error");
        return NULL;
    }
    if(slot < 0 || slot >= bfile->dtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get: invalid slot");
        return NULL;
    }
    if(dtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get: slot in used %d", slot);
        return NULL;
    }
    if(dtable[slot] == (void*)-1) {
        mq_log(MQ_LOG_ERROR, "_bfile_dtable_get: slot not prepared %d", slot);
        return NULL;
    }
    return dtable[slot];
}

//=== system interface ======================================================//

mqBfile *mq_bfile_interface_create(void)
{
    mqBfile *bfile = (mqBfile*)calloc(1, sizeof(mqBfile));
    if(!bfile) {
        mq_log(MQ_LOG_ERROR, "bfile_interface_create: calloc() fails");
        return NULL;
    }
    return bfile;
}

bool mq_bfile_interface_initialize(mqBfile *bfile, int dtable_nb_slot)
{
    if(!bfile || dtable_nb_slot <= 0) {
        mq_log(MQ_LOG_ERROR, "bfile_interface_init: broken arguments");
        return false;
    }
    bfile->dtable_nb_slot = 0;
    bfile->file_dtable = (mqFilesystemFile **)calloc(
            dtable_nb_slot, sizeof(mqFilesystemFile *));
    if(!bfile->file_dtable) {
        mq_log(MQ_LOG_ERROR, "bfile_interface_init: unable to alloc() file");
        return false;
    }
    bfile->search_dtable = (mqFilesystemSearch **)calloc(
            dtable_nb_slot, sizeof(mqFilesystemSearch *));
    if(!bfile->search_dtable) {
        mq_log(MQ_LOG_ERROR, "bfile_interface_init: unable to alloc() search");
        free(bfile->file_dtable);
        bfile->file_dtable = NULL;
        return false;
    }
    bfile->dtable_nb_slot = dtable_nb_slot;
    return true;
}

bool mq_bfile_interface_destroy(mqBfile **bfile)
{
    if(!bfile || (*bfile == NULL)) {
        mq_log(MQ_LOG_ERROR, "bfile_destroy: broken arguments");
        return false;
    }
    if(!(*bfile)->file_dtable)
        free((*bfile)->file_dtable);
    if(!(*bfile)->search_dtable)
        free((*bfile)->search_dtable);
    free(*bfile);
    *bfile = NULL;
    return true;
}

//=== storage interface =====================================================//

int mq_bfile_DeleteEntry(mqMachine *mach, u32 pathnameAddr)
{
    char pathname[1024];

    if(!_bfile_uri_conv8(mach, pathname, pathnameAddr, 1024)) {
        mq_log(MQ_LOG_ERROR, "Bfile_DeleteEntry: unable to verify the path");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_DeleteEntry: pathname == %s", pathname);
    if(!mq_filesystem_file_delete(mach->fs, pathname)) {
        mq_log(MQ_LOG_ERROR, "Bfile_DeleteEntry: unable to fs_delete()");
        return -1;
    }
    return 0;
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
        if (!mq_filesystem_file_create(mach->fs, pathname, false))
            return -1;
        return 0;
    }
    else if(mode == BFILE_CREATEMODE_FOLDER) {
        mq_log(MQ_LOG_DEBUG, "Bfile_CreateEntry: Creating dir %s", pathname);
        if (!mq_filesystem_file_create(mach->fs, pathname, true))
            return -1;
        return 0;
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
    int slot;

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
        mq_log(MQ_LOG_ERROR, "Bfile_OpenFile: invalid mode");
        return -1;
    }
    void **dtable = (void**)mach->bfile->file_dtable;
    void **fp = _bfile_dtable_reserve(mach->bfile, dtable, &slot);
    if(!fp || slot < 0) {
        mq_log(MQ_LOG_ERROR, "Bfile_OpenFile: descritor table is full");
        return -1;
    }
    *fp = mq_filesystem_file_open(mach->fs, pathname, bits);
    if((*fp) == NULL) {
        mq_log(MQ_LOG_ERROR, "Bfile_OpenFile: unable to fs_open()");
        _bfile_dtable_release(mach->bfile, dtable, slot);
        return -1;
    }
    return slot;
}

int mq_bfile_SeekFile(mqMachine *mach, int fd, int pos)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_SeekFile: %d - %d", fd, pos);
    void **dtable = (void**)mach->bfile->file_dtable;
    mqFilesystemFile *fp = (mqFilesystemFile*)_bfile_dtable_get(
            mach->bfile, dtable, fd);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "Bfile_SeekFile: unable to find the fd");
        return -1;
    }
    if(!mq_filesystem_file_lseek(mach->fs, fp, pos, SEEK_SET)) {
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
    void **dtable = (void**)mach->bfile->file_dtable;
    mqFilesystemFile *fp = (mqFilesystemFile*)_bfile_dtable_get(
            mach->bfile, dtable, fd);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: unable to find fd");
        return 0;
    }
    void *buffVirt = mq_memory_access(mach->memory, buffAddr);
    if(!buffVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: invalid buffer address");
        return 0;
    }
    if(readpos >= 0) {
        if(!mq_filesystem_file_lseek(mach->fs, fp, readpos, SEEK_SET)) {
            mq_log(MQ_LOG_ERROR, "Bfile_ReadFile: unable to fs_lseek()");
            return 0;
        }
    }
    read_size = 0;
    while(size > 0) {
        need_size = (size > 1024) ? 1024 : size;
        try_size = mq_filesystem_file_read(mach->fs, fp, buffer, need_size);
        for(u32 j = 0 ; j < try_size ; j++)
            mq_buffer_write8(buffVirt, read_size + j, buffer[j]);
        if(try_size != need_size) {
            mq_log(MQ_LOG_ERROR,
                    "Bfile_ReadFile: need(%d) != try(%d)",
                    need_size, try_size);
            return read_size + try_size;
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

    if(!mach->bfile) {
        mq_log(MQ_LOG_ERROR, "Bfile_WriteFile: internal error");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG,
            "Bfile_WriteFile: fd=%d && size=%d", fd, size);
    void **dtable = (void**)mach->bfile->file_dtable;
    mqFilesystemFile *fp = (mqFilesystemFile*)_bfile_dtable_get(
            mach->bfile, dtable, fd);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "Bfile_WriteFile: unable to find fd");
        return 0;
    }
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
        if(try_size != need_size) {
            mq_log(MQ_LOG_ERROR,
                    "Bfile_WriteFile: need(%d) != try(%d)",
                    need_size, try_size);
            return write_size + try_size;
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
    void **dtable = (void**)mach->bfile->file_dtable;
    mqFilesystemFile *fp = (mqFilesystemFile*)_bfile_dtable_get(
            mach->bfile, dtable, fd);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "Bfile_GetFileSize: unable to find the fd");
        return -1;
    }
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
    void **dtable = (void**)mach->bfile->file_dtable;
    mqFilesystemFile *fp = (mqFilesystemFile*)_bfile_dtable_get(
            mach->bfile, dtable, fd);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "Bfile_CloseFile: unable to find the fd");
        return -1;
    }
    if(!mq_filesystem_file_close(mach->fs, &fp))
        mq_log(MQ_LOG_ERROR, "Bfile_CloseFile: filesystem error");
    if(!_bfile_dtable_release(mach->bfile, dtable, fd))
        mq_log(MQ_LOG_ERROR, "Bfile_CloseFile: fd release error");
    return 0;
}

//=== search interface ======================================================//

int mq_bfile_FindFirst(mqMachine *mach,
    u32 patternAddr, u32 fdAddr, u32 foundAddr, u32 fileinfoAddr)
{
    char pattern[1024];
    int slot;
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
    void **find = _bfile_dtable_reserve(mach->bfile, dtable, &slot);
    if(!find || slot < 0) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: unable to reserve slot");
        return -1;
    }
    *find = mq_filesystem_search_open(mach->fs, pattern);
    if((*find) == NULL) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: unable to fs_search_open()");
        if(!_bfile_dtable_release(mach->bfile, dtable, slot))
            mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: slot release error");
        return -1;
    }
    mq_buffer_write32(fdVirt, 0, slot);
    rc = mq_bfile_FindNext(mach, slot, foundAddr, fileinfoAddr);
    if (rc < 0) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: unable to FindNext()");
        if(!_bfile_dtable_release(mach->bfile, dtable, slot))
            mq_log(MQ_LOG_ERROR, "Bfile_FindFirst: slot release error");
        mq_buffer_write32(fdVirt, 0, -1);
        return -1;
    }
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
    void **dtable = (void**)mach->bfile->search_dtable;
    mqFilesystemSearch *search = (mqFilesystemSearch*)_bfile_dtable_get(
            mach->bfile, dtable, fd);
    if(!search) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to find fd");
        return -1;
    }
    if(!mq_filesystem_search_next(mach->fs, search, pathname, 1024)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to fs_serach_next()");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindNext: fs_next() == %s", pathname);
    if(!mq_filesystem_search_stat(mach->fs, search, &statbuf)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to stat()");
        return -1;
    }
    mq_log(MQ_LOG_DEBUG, "Bfile_FindNext: fs_stat() == %d", statbuf.st_size);
    if(!_bfile_uri_conv_set16(mach, foundAddr, pathname, false)) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindNext: unable to uri_conv_set16()");
        return -1;
    }
    return _bfile_stat_set(mach, fileinfoAddr, pathname, &statbuf);
}

int mq_bfile_FindClose(mqMachine *mach, int fd)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_FindClose: fd == %d", fd);
    void **dtable = (void**)mach->bfile->search_dtable;
    mqFilesystemSearch *search = (mqFilesystemSearch*)_bfile_dtable_get(
            mach->bfile, dtable, fd);
    if(!search) {
        mq_log(MQ_LOG_ERROR, "Bfile_FindClose: unable to find fd");
        return -1;
    }
    if(!mq_filesystem_search_close(mach->fs, &search))
        mq_log(MQ_LOG_ERROR, "Bfile_FindClose: unable to fs_search_close()");
    if(!_bfile_dtable_release(mach->bfile, dtable, fd))
        mq_log(MQ_LOG_ERROR, "Bfile_FindClose: fd release error");
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
