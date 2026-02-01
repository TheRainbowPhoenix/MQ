//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.system.bfile: Bfile API emulation on top of the filesystem interface

#ifndef MQ_SYSTEM_BFILE_H
#define MQ_SYSTEM_BFILE_H

#include <mq/defs.h>
MQ_START_DEFS

/* 1d9f */ int mq_bfile_IdentifyDevice(u16 const *path);

/* 1da3 */ int mq_bfile_OpenFile(u16 const *path, int mode, int P3);
/* 1da4 */ int mq_bfile_CloseFile(int fd);

/* 1da6 */ int mq_bfile_GetFileSize(int fd);
/* 1da7 */ int mq_bfile_GetFileInfo(u16 const *path, FILE_INFO *fileinfo);

/* 1da9 */ int mq_bfile_SeekFile(int fd, int pos);
/* 1dab */ int mq_bfile_Filepos(int fd);

/* 1dac */ int mq_bfile_ReadFile(int fd, void *buf, int size, int readpos);

/* 1dae */ int mq_bfile_CreateEntry(u16 const *filename, int mode, int *size);
/* 1daf */ int mq_bfile_WriteFile(int fd, const void *buf, int size);

/* 1db3 */ int mq_bfile_RenameEntry(u16 const *oldname, u16 const *newname);
/* 1db4 */ int mq_bfile_DeleteEntry(u16 const *entryname);

/* 1db7 */ int mq_bfile_FindFirst(u16 const *path, int *ffd, u16 *foundfile, FILE_INFO *fileinfo);
/* 1db9 */ int mq_bfile_FindNext(int ffd, u16 *foundfile, FILE_INFO *fileinfo);
/* 1dba */ int mq_bfile_FindClose(int ffd);

MQ_END_DEFS
#endif /* MQ_SYSTEM_BFILE_H */
