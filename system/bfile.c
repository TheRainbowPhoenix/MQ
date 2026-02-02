//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/system/bfile.h>
#include <mq/memory.h>
#include <string.h>
#include <stdlib.h>

//=== path manipulation =====================================================//

/* convert the virtual u16 string
 * - convert to `char` (todo: support SHIFT-JIS)
 * - skip device information (only fls0)
 * - replace all "\" into "/" */
static int _bfile_uri_conv8(mqMachine *mach,
        char *buffer, u32 sourceAddr, size_t n)
{
    void *src = mq_memory_access(mach->memory, sourceAddr);
    size_t i = 0;
    size_t j = 0;
    u16 curr;

    if (!src)
        return -1;
    while (i < n) {
        curr = mq_buffer_read16(src, j);
        buffer[i++] = (curr >> 0) & 0xff;
        if(curr > 0x7f)
            buffer[i++] = (curr >> 8) & 0xff;
        if(curr == 0x0000 || curr == 0xffff)
            break;
        j += 2;
    }
    //todo: error when crd0
    //todo: replace \ with /
    if(!memcmp(buffer, "\\\\fls0\\", 7)) {
        memcpy(&buffer[0], &buffer[7], i - 7);
        mq_log(MQ_LOG_DEBUG, "conv8: %s - %d", buffer, i - 7);
        return i - 7;
    }
    mq_log(MQ_LOG_DEBUG,
            "_fs_uri_conv8: unable to verify the path `%s`<%d>", buffer, i);
    return -1;
}

//=== file descriptor table =================================================//

static int _bfile_fdtable_reserve(mqBfile *bfile)
{
    if(!bfile) {
        mq_log(MQ_LOG_ERROR, "fdtable_reserve: internal error");
        return false;
    }
    for(int i = 0 ; i < bfile->fdtable_nb_slot ; i++) {
        if(bfile->fdtable[i] != NULL)
            continue;
        bfile->fdtable[i] = (mqFilesystemFile*)-1;
        mq_log(MQ_LOG_DEBUG, "bfile_fdtable: reserved slot %d", i);
        return i;
    }
    return -1;
}

static bool _bfile_fdtable_release(mqBfile *bfile, int slot)
{
    if(!bfile) {
        mq_log(MQ_LOG_ERROR, "fdtable_reserve: internal error");
        return false;
    }
    if(slot < 0 || slot >= bfile->fdtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "fdtable_release: invalid slot");
        return false;
    }
    if(bfile->fdtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "fdtable_release: slot not used %d", slot);
        return false;
    }
    bfile->fdtable[slot] = NULL;
    mq_log(MQ_LOG_DEBUG, "bfile_fdtable: released slot %d", slot);
    return true;
}

static bool _bfile_fdtable_set(mqBfile *bfile,
        int slot, mqFilesystemFile *fp)
{
    if(!bfile) {
        mq_log(MQ_LOG_ERROR, "fdtable_reserve: internal error");
        return false;
    }
    if(slot < 0 || slot >= bfile->fdtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "fdtable_set: invalid slot");
        return false;
    }
    if(bfile->fdtable[slot] != (mqFilesystemFile*)-1) {
        mq_log(MQ_LOG_ERROR, "fdtable_release: slot not prepared %d", slot);
        return false;
    }
    bfile->fdtable[slot] = fp;
    mq_log(MQ_LOG_DEBUG, "bfile_fdtable: setup slot %d", slot);
    return true;
}

static mqFilesystemFile *_bfile_fdtable_find(mqBfile *bfile, int slot)
{
    if(!bfile) {
        mq_log(MQ_LOG_ERROR, "fdtable_reserve: internal error");
        return NULL;
    }
    if(slot < 0 || slot >= bfile->fdtable_nb_slot) {
        mq_log(MQ_LOG_ERROR, "fdtable_set: invalid slot");
        return NULL;
    }
    if(bfile->fdtable[slot] == NULL) {
        mq_log(MQ_LOG_ERROR, "fdtable_release: slot in used %d", slot);
        return NULL;
    }
    if(bfile->fdtable[slot] == (mqFilesystemFile*)-1) {
        mq_log(MQ_LOG_ERROR, "fdtable_release: slot not prepared %d", slot);
        return NULL;
    }
    return bfile->fdtable[slot];
}

//=== system interface ======================================================//

mqBfile *mq_bfile_create(void)
{
    mqBfile *bfile = (mqBfile*)calloc(1, sizeof(mqBfile));
    if(!bfile) {
        mq_log(MQ_LOG_ERROR, "bfile_create: calloc() fails");
        return NULL;
    }
    return bfile;
}

bool mq_bfile_initialize(mqBfile *bfile, int fdtable_nb_slot)
{
    if(!bfile) {
        mq_log(MQ_LOG_ERROR, "bfile_initialize: broken arguments");
        return false;
    }
    bfile->fdtable_nb_slot = 0;
    bfile->fdtable = (mqFilesystemFile **)calloc(
            fdtable_nb_slot, sizeof(mqFilesystemFile *));
    if(!bfile->fdtable) {
        mq_log(MQ_LOG_ERROR, "bfile_initialize: unable to alloc()");
        return false;
    }
    bfile->fdtable_nb_slot = fdtable_nb_slot;
    return true;
}

bool mq_bfile_destroy(mqBfile **bfile)
{
    if(!bfile || (*bfile == NULL)) {
        mq_log(MQ_LOG_ERROR, "bfile_destroy: broken arguments");
        return false;
    }
    if(!(*bfile)->fdtable)
        free((*bfile)->fdtable);
    free(*bfile);
    *bfile = NULL;
    return true;
}

//=== bfile interface =======================================================//

int mq_bfile_DeleteEntry(mqMachine *mach, u32 filenameAddr)
{
    (void)mach;
    (void)filenameAddr;
    return -1;
#if 0
    u16 const *filename_u16;
    char filename_u8[1024];
    mq_bfile_NameToStr_ncpy(filename_u8, filename_u16, 1024);
    printf("Deleting %s (virtually)\n", filename_u8);
    return 0;
#endif
}

int mq_bfile_CreateEntry(mqMachine *mach,
        u32 filenameAddr, int mode, u32 sizeAddr)
{
    char filename[1024];
    size_t size;

    if (_bfile_uri_conv8(mach, filename, filenameAddr, 1024) < 0) {
        mq_log(MQ_LOG_DEBUG, "unable to verify the path");
        return -1;
    }
    size = mq_buffer_read32(mq_memory_access(mach->memory, sizeAddr), 0);
    if(mode == BFILE_CREATEMODE_FILE) {
        mq_log(MQ_LOG_DEBUG,
                "bfile_create: Creating file %s with size %zu (virtually)",
                filename, size);
        if (!mq_filesystem_create_file(mach->fs, filename, false))
            return -1;
        return 0;
    }
    else if(mode == BFILE_CREATEMODE_FOLDER) {
        mq_log(MQ_LOG_DEBUG,
                "bfile_create: Creating dir %s with size %zu (virtually)",
                filename, size);
        if (!mq_filesystem_create_file(mach->fs, filename, true))
            return -1;
        return 0;
    }
    else {
        mq_log(MQ_LOG_ERROR, "bfile_CreateEntry(): Invalid mode %d", mode);
        return -1;
    }
}

int mq_bfile_OpenFile(mqMachine *mach,
        u32 filenameAddr, int mode)
{
    char filename[1024];
    char const *bits;

    if (_bfile_uri_conv8(mach, filename, filenameAddr, 1024) < 0) {
        mq_log(MQ_LOG_DEBUG, "unable to verify the path");
        return -1;
    }
    if(mode == BFILE_MODE_READ || mode == BFILE_MODE_READ_SHARE)
        bits = "rb";
    else if(mode == BFILE_MODE_WRITE)
        bits = "wb";
    else if(mode == BFILE_MODE_READWRITE || mode == BFILE_MODE_READWRITE_SHARE)
        bits = "w+b";
    else {
        mq_log(MQ_LOG_DEBUG,
                "mq_bfile_OpenFile(): invalid mode %d for %s\n",
                mode, filename);
        return -1;
    }
    int slot = _bfile_fdtable_reserve(mach->bfile);
    if(slot < 0) {
        mq_log(MQ_LOG_ERROR,
                "mq_bfile_OpenFile(): cannot open %s, table is full",
                filename);
        return -1;
    }
    mqFilesystemFile *fp = mq_filesystem_open(mach->fs, filename, bits);
    if(!fp) {
        mq_log(MQ_LOG_ERROR,
                "OpenFile: cannot open `%s` - %s", filename, bits);
        _bfile_fdtable_release(mach->bfile, slot);
        return -1;
    }
    if(!_bfile_fdtable_set(mach->bfile, slot, fp)) {
        mq_log(MQ_LOG_ERROR,
                "OpenFile: unable to set file descriptor %d", slot);
        _bfile_fdtable_release(mach->bfile, slot);
        return -1;
    }
    return slot;
}

int mq_bfile_GetFileSize(mqMachine *mach, int fd)
{
    (void)mach;
    (void)fd;
    return -1;
#if 0
    FILE *fp = file_table[fd];
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, pos, SEEK_SET);
    return size;
#endif
}

int mq_bfile_GetFileInfo(mqMachine *mach,
        u32 pathAddr, u32 fileinfoAddr)
{
    (void)mach;
    (void)pathAddr;
    (void)fileinfoAddr;
    return -1;
}

int mq_bfile_SeekFile(mqMachine *mach, int fd, int pos)
{
    (void)mach;
    (void)fd;
    (void)pos;
    return -1;
#if 0
    fseek(file_table[fd], pos, SEEK_SET);
    return 0;
#endif
}

int mq_bfile_Filepos(mqMachine *mach, int fd)
{
    (void)mach;
    (void)fd;
    return -1;
}

int mq_bfile_ReadFile(mqMachine *mach,
        int fd, u32 buffAddr, int size, int readpos)
{
    u8 buffer[1024];
    u32 read_size;
    u32 need_size;
    u32 try_size;

    mq_log(MQ_LOG_DEBUG,
            "Bfile_ReadFile(): size=%d && seek=%d", size, readpos);
    mqFilesystemFile *fp = _bfile_fdtable_find(mach->bfile, fd);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "Bfile_CloseFile(): unable to find the fd");
        return 0;
    }
    void *buffVirt = mq_memory_access(mach->memory, buffAddr);
    if(!buffVirt) {
        mq_log(MQ_LOG_ERROR, "Bfile_ReadFile(): requested addr error");
        return 0;
    }
    if(readpos < 0) {
        if(!mq_filesystem_lseek(mach->fs, fp, readpos, SEEK_SET)) {
            mq_log(MQ_LOG_ERROR, "Bfile_ReadFile(): seek error");
            return 0;
        }
    }
    read_size = 0;
    while(size > 0) {
        need_size = (size > 1024) ? 1024 : size;
        try_size = mq_filesystem_read(mach->fs, fp, buffer, need_size);
        for(u32 j = 0 ; j < need_size ; j++)
            mq_buffer_write8(buffVirt, j, buffer[j]);
        if(try_size != need_size) {
            mq_log(MQ_LOG_ERROR, "Bfile_ReadFile(): need != try");
            return read_size + try_size;
        }
        read_size += try_size;
        size -= try_size;
    }
    return read_size;
}

int mq_bfile_RenameEntry(mqMachine *mach,
        u32 oldnameAddr, u32 newnameAddr)
{
    (void)mach;
    (void)oldnameAddr;
    (void)newnameAddr;
    return -1;
}

int mq_bfile_WriteFile(mqMachine *mach,
        int fd, u32 buffAddr, int size)
{
    (void)mach;
    (void)fd;
    (void)buffAddr;
    (void)size;
    return -1;
#if 0
    return fwrite(buf, 1, size, file_table[fd]);
#endif
}

int mq_bfile_CloseFile(mqMachine *mach, int fd)
{
    mq_log(MQ_LOG_DEBUG, "Bfile_CloseFile(): fd == %d", fd);
    mqFilesystemFile *fp = _bfile_fdtable_find(mach->bfile, fd);
    if(!fp) {
        mq_log(MQ_LOG_ERROR, "Bfile_CloseFile(): unable to find the fd");
        return -1;
    }
    if(!mq_filesystem_close(mach->fs, &fp))
        mq_log(MQ_LOG_ERROR, "Bfile_CloseFile(): filesystem error");
    if(!_bfile_fdtable_release(mach->bfile, fd))
        mq_log(MQ_LOG_ERROR, "Bfile_CloseFile(): fd release error");
    return 0;
}

int mq_bfile_FindFirst(mqMachine *mach,
    u32 patternAddr, u32 fdAddr, u32 foundAddr, u32 fileinfoAddr)
{
    (void)mach;
    (void)patternAddr;
    (void)fdAddr;
    (void)foundAddr;
    (void)fileinfoAddr;
    return -1;
#if 0
    char pattern_u8[1024];
    mq_bfile_NameToStr_ncpy(pattern_u8, pattern_u16, 1024);

    int slot = 0;
    while(slot < MQ_FILESYSTEM_MAX_FD && search_table[slot].pos != 0)
        slot++;
    if(slot >= MQ_FILESYSTEM_MAX_FD) {
        printf("mq_bfile_FindFirst(): cannot search %s, table is full\n", pattern_u8);
        *fd = -1;
        return -16;
    }

    int rc = glob(pattern_u8, 0, NULL, &search_table[slot].glob);

    printf("mq_bfile_FindFirst(): Searching %s: %zu results\n", pattern_u8,
        (rc == GLOB_NOMATCH) ? 0 : search_table[slot].glob.gl_pathc);

    if(rc == GLOB_NOMATCH)
        return -16;

    *fd = slot;
    return mq_bfile_FindNext(slot, found, fileinfo);
#endif
}

int mq_bfile_FindNext(mqMachine *mach,
        int fd, u32 foundAddr, u32 fileinfoAddr)
{
    (void)mach;
    (void)fd;
    (void)foundAddr;
    (void)fileinfoAddr;
    return -1;
#if 0
    int *pos = &search_table[fd].pos;
    glob_t *glob = &search_table[fd].glob;

    if(*pos >= glob->gl_pathc)
        return -16;

    const char *name = glob->gl_pathv[*pos];
    mq_bfile_StrToName_ncpy(found, name, strlen(name)+1);
    (*pos)++;

    mq_bfile_FileInfo *fileinfo = fileinfo0;
    // TODO: More resonsable mq_bfile_FileInfo entries?
    memset(fileinfo, 0, sizeof *fileinfo);

    FILE *fp = fopen(name, "rb");
    if(fp) {
        fseek(fp, 0, SEEK_END);
        fileinfo->fsize = ftell(fp);
        fclose(fp);
    }

    return 0;
#endif
}

int mq_bfile_FindClose(mqMachine *mach, int fd)
{
    (void)mach;
    (void)fd;
    return -1;
#if 0
    if(fd < 0 || fd >= MQ_FILESYSTEM_MAX_FD)
        return -1;

    search_table[fd].pos = 0;
    globfree(&search_table[fd].glob);
    return 0;
#endif
}
