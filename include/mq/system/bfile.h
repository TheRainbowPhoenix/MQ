//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.system.bfile: Bfile API emulation on top of the filesystem interface

#ifndef MQ_SYSTEM_BFILE_H
#define MQ_SYSTEM_BFILE_H

#include <mq/defs.h>
#include <mq/machine.h>
#include <mq/interfaces/filesystem.h>
MQ_START_DEFS

//=== system interface ======================================================//

struct mqBfile {
    mqFilesystemFile **file_dtable;
    mqFilesystemSearch **search_dtable;
    uint dtable_nb_slot;
};
typedef struct mqBfile mqBfile;

mqBfile *mq_bfile_create(int fdtable_nb_slot);
void mq_bfile_destroy(mqBfile *bfile);

//=== BFile types ===========================================================//

enum {
    BFILE_MODE_READ             = 0x01,
    BFILE_MODE_WRITE            = 0x02,
    BFILE_MODE_READWRITE        = (BFILE_MODE_READ | BFILE_MODE_WRITE),
    BFILE_MODE_READWRITE_SHARE  = (0x80 | BFILE_MODE_READWRITE),
    BFILE_MODE_READ_SHARE       = (0x80 | BFILE_MODE_READ),
};
enum {
    BFILE_CREATEMODE_FILE       = 1,
    BFILE_CREATEMODE_FOLDER     = 5,
};
enum {
    BFILE_TYPE_DIRECTORY  = 0x0000,
    BFILE_TYPE_FILE       = 0x0001,
    BFILE_TYPE_ADDIN      = 0x0002,
    BFILE_TYPE_EACT       = 0x0003,
    BFILE_TYPE_LANGUAGE   = 0x0004,
    BFILE_TYPE_BITMAP     = 0x0005,
    BFILE_TYPE_MAINMEM    = 0x0006,
    BFILE_TYPE_TEMP       = 0x0007,
    BFILE_TYPE_DOT        = 0x0008,
    BFILE_TYPE_DOTDOT     = 0x0009,
    BFILE_TYPE_VOLUME     = 0x000a,
    BFILE_TYPE_ARCHIVED   = 0x0041,
};

//=== storage interface =====================================================//

int mq_bfile_CreateEntry(mqMachine *mach,
        u32 filenameAddr, int mode, u32 sizeAddr);
int mq_bfile_DeleteEntry(mqMachine *mach,
        u32 entrynameAddr);

//=== file interface ========================================================//

int mq_bfile_GetFileInfo(mqMachine *mach,
        u32 pathAddr, u32 fileinfoAddr);
int mq_bfile_OpenFile(mqMachine *mach,
        u32 pathAddr, int mode);
int mq_bfile_SeekFile(mqMachine *mach,
        int fd, int pos);
int mq_bfile_ReadFile(mqMachine *mach,
        int fd, u32 bufAddr, int size, int readpos);
int mq_bfile_WriteFile(mqMachine *mach,
        int fd, u32 bufAddr, int size);
int mq_bfile_CloseFile(mqMachine *mach,
        int fd);
int mq_bfile_GetFileSize(mqMachine *mach,
        int fd);

//=== search interface ======================================================//

int mq_bfile_FindFirst(mqMachine *mach,
        u32 pathAddr, u32 ffdAddr, u32 foundfileAddr, u32 fileinfoAddr);
int mq_bfile_FindNext(mqMachine *mach,
        int ffd, u32 foundfileAddr, u32 fileinfoAddr);
int mq_bfile_FindClose(mqMachine *mach,
        int ffd);

MQ_END_DEFS
#endif /* MQ_SYSTEM_BFILE_H */
