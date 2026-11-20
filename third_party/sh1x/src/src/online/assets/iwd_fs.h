#ifndef IWD_FS_H
#define IWD_FS_H
#include "zlib/unzip.h"
#include <stdint.h>
/* C1 archive checksum input: CRC plus full filename hash bucket. */
uint32_t IwdChecksumWord(const char *name, uint32_t crc, uint32_t hash_size);
/* Initialize before indexing archives. Failure clears previous key state;
 * existing ZIP handles own their keys and remain independently usable. */
int InitIwdFilesystem(const char *client_exe);
void ClearIwdFilesystem(void);
int IsEncryptedIwd(const char *path);
unzFile OpenIwdArchive(const char *path);
#endif
