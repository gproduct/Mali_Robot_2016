#ifndef FAT_H_
#define FAT_H_


#define OVERWRITE 	0
#define ADD_TO_END 1

signed char SD_Init(void (*SPI_Initf)(void), unsigned char (*SPI_ReadWritef)(unsigned char), char CSpin, volatile unsigned char *CSport);
signed char SD_WriteSector(unsigned char *buffer, unsigned long sector);
signed char SD_ReadSector(unsigned char *buffer, unsigned long sector);
char getCardType(void);

char FAT_Init(void);
unsigned char FAT_OpenFile(char *filename, unsigned char parentDirectoryIndex);
unsigned char FAT_OpenDirectory(char *directoryName, unsigned char parentDirectoryIndex);
unsigned char FAT_CloseDirectory(unsigned char directoryIndex);
unsigned char FAT_CreateDirectory(char *name, unsigned char parentDirectoryIndex);
unsigned char FAT_CreateFile(char *name, unsigned char parentDirectoryIndex);
char FAT_RemoveFile(unsigned char fileIndex);
char FAT_CloseFile(unsigned char fileIndex);
char FAT_WriteFile(unsigned char fileIndex, char mode, char *content);
char FAT_ReadFile(unsigned char fileIndex, unsigned char *buffer, unsigned long startOffset, unsigned int numOfBytesToRead);

#endif
