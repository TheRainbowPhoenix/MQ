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
    int *file_dtable;
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

enum {
    BFILE_ERROR_ENTRYNOTFOUND     = -1,
    BFILE_ERROR_ILLEGALPARAM      = -2,
    BFILE_ERROR_ILLEGALPATH       = -3,
    BFILE_ERROR_DEVICEFULL        = -4,
    BFILE_ERROR_ILLEGALDEVICE     = -5,
    BFILE_ERROR_ILLEGALFILESYSTEM = -6,
    BFILE_ERROR_ILLEGALSYSTEM     = -7,
    BFILE_ERROR_ACCESSDENIED      = -8,
    BFILE_ERROR_ALREADYLOCKED     = -9,
    BFILE_ERROR_ILLEGALTASKID     = -10,
    BFILE_ERROR_PERMISSIONERROR   = -11,
    BFILE_ERROR_ENTRYFULL         = -12,
    BFILE_ERROR_ALREADYEXISTS     = -13,
    BFILE_ERROR_READONLYFILE      = -14,
    BFILE_ERROR_ILLEGALFILTER     = -15,
    BFILE_ERROR_ENUMERATEEND      = -16,
    BFILE_ERROR_DEVICECHANGED     = -17,
    BFILE_ERROR_NOTRECORDFILE     = -18, // NOT USED
    BFILE_ERROR_ILLEGALSEEKPOS    = -19,
    BFILE_ERROR_ILLEGALBLOCKFILE  = -20,
    BFILE_ERROR_NOSUCHDEVICE      = -21, // NOT USED
    BFILE_ERROR_ENDOFFILE         = -22, // NOT USED
    BFILE_ERROR_NOTMOUNTDEVICE    = -23,
    BFILE_ERROR_NOTUNMOUNTDEVICE  = -24,
    BFILE_ERROR_CANNOTLOCKSYSTEM  = -25,
    BFILE_ERROR_RECORDNOTFOUND    = -26,
    BFILE_ERROR_NOTDUALRECORDFILE = -27, // NOT USED
    BFILE_ERROR_NOALARMSUPPORT    = -28,
    BFILE_ERROR_CANNOTADDALARM    = -29,
    BFILE_ERROR_FILEFINDUSED      = -30,
    BFILE_ERROR_DEVICEERROR       = -31,
    BFILE_ERROR_SYSTEMNOTLOCKED   = -32,
    BFILE_ERROR_DEVICENOTFOUND    = -33,
    BFILE_ERROR_FILETYPEMISMATCH  = -34,
    BFILE_ERROR_NOTEMPTY          = -35,
    BFILE_ERROR_BROKENSYSTEMDATA  = -36,
    BFILE_ERROR_MEDIANOTREADY     = -37,
    BFILE_ERROR_TOOMANYALARMS     = -38,
    BFILE_ERROR_SAMEALARMEXISTS   = -39,
    BFILE_ERROR_ACCESSSWAPAREA    = -40,
    BFILE_ERROR_MULTIMEDIACARD    = -41,
    BFILE_ERROR_COPYPROTECTION    = -42,
    BFILE_ERROR_ILLEGALFILEDATA   = -43,
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
        int handle, int pos);
int mq_bfile_ReadFile(mqMachine *mach,
        int handle, u32 bufAddr, int size, int readpos);
int mq_bfile_WriteFile(mqMachine *mach,
        int handle, u32 bufAddr, int size);
int mq_bfile_CloseFile(mqMachine *mach,
        int handle);
int mq_bfile_GetFileSize(mqMachine *mach,
        int handle);
int mq_bfile_GetFilePos(mqMachine *mach,
        int handle);

//=== search interface ======================================================//

int mq_bfile_FindFirst(mqMachine *mach,
        u32 pathAddr, u32 ffdAddr, u32 foundfileAddr, u32 fileinfoAddr);
int mq_bfile_FindNext(mqMachine *mach,
        int ffd, u32 foundfileAddr, u32 fileinfoAddr);
int mq_bfile_FindClose(mqMachine *mach,
        int ffd);

MQ_END_DEFS
#endif /* MQ_SYSTEM_BFILE_H */
