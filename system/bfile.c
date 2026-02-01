//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/system/bfile.h>
#include <string.h>


void mq_bfile_NameToStr_ncpy(mqMachine *mach,
        u32 destAddr, u32 sourceAddr, size_t n)
{
    (void)mach;
    (void)destAddr;
    (void)sourceAddr;
    (void)n;
    // return -1;
#if 0
    u16 const *source;
    size_t i = 0;

    if(!memcmp(source, u"\\\\fls0\\", 14))
        source += 7;

    while(i < n && source[i] != 0x0000 && source[i] != 0xffff) {
        dest[i] = source[i];
        i++;
    }
    for(size_t j = i; j < n; j++)
        dest[j] = source[i];
#endif
}

void mq_bfile_StrToName_ncpy(mqMachine *mach,
        u32 destAddr, u32 sourceAddr, size_t n)
{
    (void)mach;
    (void)destAddr;
    (void)sourceAddr;
    (void)n;
    // return -1;
#if 0
    u16 *dest;
    char const *source;
    if(!strncmp(source, "\\\\fls0\\", 7)) {
        memcpy(dest, u"\\\\fls0\\", 14);
        dest += 7;
    }

    size_t i = 0;
    while(i < n && source[i]) {
        dest[i] = source[i];
        i++;
    }
    while(i < n)
        dest[i++] = 0;
#endif
}

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
    (void)mach;
    (void)filenameAddr;
    (void)mode;
    (void)sizeAddr;
    return -1;
#if 0
    u16 const *filename_u16;
    char filename_u8[1024];
    mq_bfile_NameToStr_ncpy(filename_u8, filename_u16, 1024);

    if(mode == BFILE_CREATEMODE_FILE) {
        printf("Creating %s with size %zu (virtually)\n", filename_u8, *size);
    }
    else if(mode == BFILE_CREATEMODE_FOLDER) {
        printf("Cannot create folder %s: Not Implemented\n", filename_u8);
        return -1;
    }
    else {
        printf("mq_bfile_CreateEntry(): Invalid mode %d\n", mode);
        return -1;
    }
    return 0;
#endif
}

int mq_bfile_OpenFile(mqMachine *mach,
        u32 filenameAddr, int mode)
{
    (void)mach;
    (void)filenameAddr;
    (void)mode;
    return -1;
#if 0
    char filename_u8[1024];
    mq_bfile_NameToStr_ncpy(filename_u8, filename_u16, 1024);

    char const *bits;
    if(mode == BFILE_READ || mode == BFILE_READ_SHARE)
        bits = "rb";
    else if(mode == BFILE_WRITE)
        bits = "wb";
    else if(mode == BFILE_READWRITE || mode == BFILE_READWRITE_SHARE)
        bits = "w+b";
    else {
        printf("mq_bfile_OpenFile(): invalid mode %d for %s\n", mode, filename_u8);
        return -1;
    }

    FILE *fp = fopen(filename_u8, bits);
    if(!fp) {
        printf("mq_bfile_OpenFile(): cannot open %s: %m\n", filename_u8);
        return -1;
    }

    /* Find open slot in file table */
    int slot = 0;
    while(slot < MQ_FILESYSTEM_MAX_FD && file_table[slot] != NULL)
        slot++;
    if(slot >= MQ_FILESYSTEM_MAX_FD) {
        printf("mq_bfile_OpenFile(): cannot open %s, table is full\n",filename_u8);
        fclose(fp);
        return -1;
    }

    printf("mq_bfile_OpenFile(): opened %s\n", filename_u8);
    file_table[slot] = fp;
    return slot;
#endif
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

int mq_bfile_IdentifyDevice(mqMachine *mach, u32 pathAddr)
{
    (void)mach;
    (void)pathAddr;
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
    (void)mach;
    (void)fd;
    (void)buffAddr;
    (void)size;
    (void)readpos;
    return -1;
#if 0
  if(readpos != -1)
    mq_bfile_SeekFile(fd, readpos);
  return fread(buf, 1, size, file_table[fd]);
#endif
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
    (void)mach;
    (void)fd;
    return -1;
#if 0
    fclose(file_table[fd]);
    file_table[fd] = NULL;
    return 0;
#endif
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
