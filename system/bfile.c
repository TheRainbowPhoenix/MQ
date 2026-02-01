//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/system/bfile.h>

static FILE *file_table[MQ_FILESYSTEM_MAX_FD] = { NULL };
static struct {
    int pos;
    glob_t glob;
} search_table[MQ_FILESYSTEM_MAX_FD];

void Bfile_NameToStr_ncpy(char *dest, const uint16_t *source, size_t n)
{
    size_t i = 0;

    if(!memcmp(source, u"\\\\fls0\\", 14))
        source += 7;

    while(i < n && source[i] != 0x0000 && source[i] != 0xffff) {
        dest[i] = source[i];
        i++;
    }
    for(size_t j = i; j < n; j++)
        dest[j] = source[i];
}

void Bfile_StrToName_ncpy(uint16_t *dest, const char *source, size_t n)
{
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
}

int Bfile_DeleteEntry(const uint16_t *filename_u16)
{
    char filename_u8[1024];
    Bfile_NameToStr_ncpy(filename_u8, filename_u16, 1024);
    printf("Deleting %s (virtually)\n", filename_u8);
    return 0;
}

int Bfile_CreateEntry_OS(const uint16_t *filename_u16, int mode, size_t *size)
{
    char filename_u8[1024];
    Bfile_NameToStr_ncpy(filename_u8, filename_u16, 1024);

    if(mode == BFILE_CREATEMODE_FILE) {
        printf("Creating %s with size %zu (virtually)\n", filename_u8, *size);
    }
    else if(mode == BFILE_CREATEMODE_FOLDER) {
        printf("Cannot create folder %s: Not Implemented\n", filename_u8);
        return -1;
    }
    else {
        printf("Bfile_CreateEntry_OS(): Invalid mode %d\n", mode);
        return -1;
    }

    return 0;
}

int Bfile_OpenFile_OS(const uint16_t *filename_u16, int mode, int zero)
{
    (void)zero;
    char filename_u8[1024];
    Bfile_NameToStr_ncpy(filename_u8, filename_u16, 1024);

    char const *bits;
    if(mode == BFILE_READ || mode == BFILE_READ_SHARE)
        bits = "rb";
    else if(mode == BFILE_WRITE)
        bits = "wb";
    else if(mode == BFILE_READWRITE || mode == BFILE_READWRITE_SHARE)
        bits = "w+b";
    else {
        printf("Bfile_OpenFile_OS(): invalid mode %d for %s\n", mode, filename_u8);
        return -1;
    }

    FILE *fp = fopen(filename_u8, bits);
    if(!fp) {
        printf("Bfile_OpenFile_OS(): cannot open %s: %m\n", filename_u8);
        return -1;
    }

    /* Find open slot in file table */
    int slot = 0;
    while(slot < MQ_FILESYSTEM_MAX_FD && file_table[slot] != NULL)
        slot++;
    if(slot >= MQ_FILESYSTEM_MAX_FD) {
        printf("Bfile_OpenFile_OS(): cannot open %s, table is full\n",filename_u8);
        fclose(fp);
        return -1;
    }

    printf("Bfile_OpenFile_OS(): opened %s\n", filename_u8);
    file_table[slot] = fp;
    return slot;
}

int Bfile_GetFileSize_OS(int fd)
{
    FILE *fp = file_table[fd];
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, pos, SEEK_SET);
    return size;
}

int Bfile_SeekFile_OS(int fd, int pos)
{
    fseek(file_table[fd], pos, SEEK_SET);
    return 0;
}

int Bfile_ReadFile_OS(int fd, void *buf, int size, int readpos)
{
  if(readpos != -1)
    Bfile_SeekFile_OS(fd, readpos);

  return fread(buf, 1, size, file_table[fd]);
}

int Bfile_WriteFile_OS(int fd, const void *buf, int size)
{
    return fwrite(buf, 1, size, file_table[fd]);
}

int Bfile_CloseFile_OS(int fd)
{
    fclose(file_table[fd]);
    file_table[fd] = NULL;
    return 0;
}

int Bfile_FindFirst(const uint16_t *pattern_u16, int *fd, uint16_t *found,
  void *fileinfo)
{
    char pattern_u8[1024];
    Bfile_NameToStr_ncpy(pattern_u8, pattern_u16, 1024);

    int slot = 0;
    while(slot < MQ_FILESYSTEM_MAX_FD && search_table[slot].pos != 0)
        slot++;
    if(slot >= MQ_FILESYSTEM_MAX_FD) {
        printf("Bfile_FindFirst(): cannot search %s, table is full\n", pattern_u8);
        *fd = -1;
        return -16;
    }

    int rc = glob(pattern_u8, 0, NULL, &search_table[slot].glob);

    printf("Bfile_FindFirst(): Searching %s: %zu results\n", pattern_u8,
        (rc == GLOB_NOMATCH) ? 0 : search_table[slot].glob.gl_pathc);

    if(rc == GLOB_NOMATCH)
        return -16;

    *fd = slot;
    return Bfile_FindNext(slot, found, fileinfo);
}

int Bfile_FindNext(int fd, uint16_t *found, void *fileinfo0)
{
    int *pos = &search_table[fd].pos;
    glob_t *glob = &search_table[fd].glob;

    if(*pos >= glob->gl_pathc)
        return -16;

    const char *name = glob->gl_pathv[*pos];
    Bfile_StrToName_ncpy(found, name, strlen(name)+1);
    (*pos)++;

    Bfile_FileInfo *fileinfo = fileinfo0;
    // TODO: More resonsable Bfile_FileInfo entries?
    memset(fileinfo, 0, sizeof *fileinfo);

    FILE *fp = fopen(name, "rb");
    if(fp) {
        fseek(fp, 0, SEEK_END);
        fileinfo->fsize = ftell(fp);
        fclose(fp);
    }

    return 0;
}

int Bfile_FindClose(int fd)
{
    if(fd < 0 || fd >= MQ_FILESYSTEM_MAX_FD)
        return -1;

    search_table[fd].pos = 0;
    globfree(&search_table[fd].glob);
    return 0;
}
