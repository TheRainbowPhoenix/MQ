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

#include <glob.h>

struct mqBfile {
    /* file tracking table */
    mqFilesystemFile **fdtable;
    int fdtable_nb_slot;
    // struct {
    //     int pos;
    //     glob_t glob;
    // } search_table[MQ_FILESYSTEM_MAX_FD];
};
typedef struct mqBfile mqBfile;


//=== BFile types ============================================================//

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

struct mqBfileFileinfo {
    int tmp;
};
typedef struct mqBfileFileinfo mqBfileFileinfo;


//=== system interface ======================================================//

mqBfile *mq_bfile_create(void);
bool mq_bfile_initialize(mqBfile *bfile, int fdtable_nb_slot);
bool mq_bfile_destroy(mqBfile **bfile);

//=== BFile interface =======================================================//

/* 1d9f */ int mq_bfile_IdentifyDevice(mqMachine *mach, u32 pathAddr);

/* 1da3 */ int mq_bfile_OpenFile(mqMachine *mach, u32 pathAddr, int mode);
/* 1da4 */ int mq_bfile_CloseFile(mqMachine *mach, int fd);

/* 1da6 */ int mq_bfile_GetFileSize(mqMachine *mach, int fd);
/* 1da7 */ int mq_bfile_GetFileInfo(mqMachine *mach,
        u32 pathAddr, u32 fileinfoAddr);

/* 1da9 */ int mq_bfile_SeekFile(mqMachine *mach, int fd, int pos);
/* 1dab */ int mq_bfile_Filepos(mqMachine *mach, int fd);

/* 1dac */ int mq_bfile_ReadFile(mqMachine *mach,
        int fd, u32 bufAddr, int size, int readpos);

/* 1dae */ int mq_bfile_CreateEntry(mqMachine *mach,
        u32 filenameAddr, int mode, u32 sizeAddr);
/* 1daf */ int mq_bfile_WriteFile(mqMachine *mach,
        int fd, u32 bufAddr, int size);

/* 1db3 */ int mq_bfile_RenameEntry(mqMachine *mach,
        u32 oldnameAddr, u32 newnameAddr);
/* 1db4 */ int mq_bfile_DeleteEntry(mqMachine *mach,
        u32 entrynameAddr);

/* 1db7 */ int mq_bfile_FindFirst(mqMachine *mach,
        u32 pathAddr, u32 ffdAddr, u32 foundfileAddr, u32 fileinfoAddr);
/* 1db9 */ int mq_bfile_FindNext(mqMachine *mach,
        int ffd, u32 foundfileAddr, u32 fileinfoAddr);
/* 1dba */ int mq_bfile_FindClose(mqMachine *mach, int ffd);

MQ_END_DEFS
#endif /* MQ_SYSTEM_BFILE_H */
