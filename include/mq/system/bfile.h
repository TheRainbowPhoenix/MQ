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
MQ_START_DEFS

#include <glob.h>
#include <stdio.h>

/* Maximum number of file descriptors and search descriptors. */
#define MQ_FILESYSTEM_MAX_FD 16

struct mqBfile {
    /* Open file descriptor table (-1 when unused). */
    int fdtable[MQ_FILESYSTEM_MAX_FD];
    /* file tracking table */
    FILE *file_table[MQ_FILESYSTEM_MAX_FD];
    struct {
        int pos;
        glob_t glob;
    } search_table[MQ_FILESYSTEM_MAX_FD];
};
typedef struct mqBfile mqBfile;

struct mqBfileFileinfo {
    int tmp;
};
typedef struct mqBfileFileinfo mqBfileFileinfo;

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
