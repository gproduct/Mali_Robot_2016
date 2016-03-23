#include "fat.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned char (*SPI_ReadWrite)(unsigned char);
static volatile unsigned char *CS_port, CS_pin;
static char cardType = 0;
static char byteAddressing = 1;

static void UART_Print(char *data, ...)
{
	
}

static unsigned char sectorBuffer[512];

static struct sFAT
{
	unsigned int bootAddress;
	unsigned int dataStartAddress;
	unsigned int rootStartAddress;
	unsigned int fatStartAddress;
	unsigned int sizeOfFAT;
	unsigned int sizeOfRoot;
	unsigned int nextFreeCluster; // cluster is same thing as allocation unit, but marked differently
	unsigned int lastFreeFatSector;
	unsigned int currentSector;
	unsigned char clusterSize;
}FAT;

struct sFile
{
	char filename[8];
	unsigned char parentDirectoryIndex;
	unsigned char index;
	unsigned int dataStartSector;
	unsigned int dataEndCluster;
	unsigned int descriptorSector;
	unsigned int descriptorOffset;

	struct sFile *nextNode;
	struct sFile *previousNode;
};

struct sDirectory
{
	char directoryName[8];
	unsigned char parentDirectoryIndex;
	unsigned char index;
	unsigned int numOfFiles;
	unsigned int dataStartSector;
	unsigned int dataEndCluster;
	unsigned int descriptorSector;
	unsigned int descriptorOffset;

	struct sDirectory *nextNode;
	struct sDirectory *previousNode;
};

static unsigned char nextFreeFileIndex;
static unsigned char nextFreeDirectoryIndex;
static unsigned char numOfOpenedFiles = 0;
static unsigned char numOfOpenedDirectories = 0;

static struct sFile *fileHead = NULL;
static struct sDirectory *directoryHead = NULL;

static inline void CS_activate(void)
{
	*CS_port &= ~(1 << CS_pin); // CS Low
}

static inline void CS_deactivate(void)
{
	*CS_port |= (1 << CS_pin); // CS high
}

static char SD_Write(unsigned char command, unsigned long args)
{
	char response;
	unsigned char numOfTries = 0;

	CS_activate();
	SPI_ReadWrite(command | 0x40);

	if((command == 24 || command == 17) && byteAddressing == 1)
	{
		SPI_ReadWrite(args >> 15);
		SPI_ReadWrite(args >> 7);
		SPI_ReadWrite(args << 1);
		SPI_ReadWrite(0);
	}
	else
	{
		SPI_ReadWrite(args >> 24);
		SPI_ReadWrite(args >> 16);
		SPI_ReadWrite(args >> 8);
		SPI_ReadWrite(args);
	}

	if(command == 0)
		SPI_ReadWrite(0x95);
	else if(command == 0x11)
		SPI_ReadWrite(0xFF);
//	else
		//SPI_ReadWrite(0x87);

	while((response = SPI_ReadWrite(0xFF)) & 0b10000000)
		if (++numOfTries > 200)
		{
			UART_Print("SPI command failed.\n\r");
			return -1;
		}

	unsigned char i;
	for(i = 0; i < 8; ++i) // ?? -> testirati bez ovoga
		SPI_ReadWrite(0xFF);

	return response;
}

static char SD_WriteR1(unsigned char command, unsigned long args)
{
	char response = SD_Write(command, args);
	//SPI_ReadWrite(0xFF); // stupid older Canon V1 card requiers an extra byte to be sent for it to do what expected
	CS_deactivate();

	return response;
}

static unsigned long SD_WriteR37(unsigned char command, unsigned long args)
{
	char response = SD_Write(command, args);

	if(response != -1)
		args = ((unsigned long) SPI_ReadWrite(0xFF) << 24)
				| ((unsigned long) SPI_ReadWrite(0xFF) << 16)
				| (SPI_ReadWrite(0xFF) << 8) | (SPI_ReadWrite(0xFF));

	SPI_ReadWrite(0xFF);
	CS_deactivate();

	if(response == -1)
		return 0xFFFFFFFF;

	return args;
}

signed char SD_Init(void (*SPI_Initf)(void), unsigned char (*SPI_ReadWritef)(unsigned char), char CSpin, volatile unsigned char *CSport)
{
	int i;
	char cResponse;
	unsigned long ulResponse;

	CS_port = CSport;
	CS_pin = CSpin;
	SPI_ReadWrite = SPI_ReadWritef;
	SPI_Initf();

	CS_deactivate(); // more than 74 pulses of ones required after power-up, 100 * 8 is more than enough but okay
	for(i = 0; i < 60; ++i)
		SPI_ReadWrite(0xFF);

	if(SD_WriteR1(0, 0) != 0x01)   // software reset, 0x01 expected
		return -1;

	ulResponse = SD_WriteR37(8, 0x1AA); // checking SD voltage range
	i = 0;
	do // sending ACMD41 command until accepted
	{
		SD_WriteR1(55, 0);
		cResponse = SD_WriteR1(41, 0x40000000);

		if(++i >= 30) // command failed
		{
			if((ulResponse & 0xFFFF) == 0x1AA)
				return -1;
			else
				cardType = 'M'; //MMC

			break;
		}
	}while(cResponse != 0x00);

	if((ulResponse & 0xFFFF) == 0x1AA) // SD V2
	{
		cardType = '2'; //SD V2

		ulResponse = SD_WriteR37(58, 0); // getting 32- bit OCR register

		if ((ulResponse >> 30) & 0x01) // checking CCS bit in position 30, check init flowchart
		{
			// block addressing
			byteAddressing = 1;
			return 1;
		}
		else // byte addressing
		{
			//byteAddressing = 1;
			//UART_Print("Byte addressing.\n\r");
		}
	}
	else if (cardType == 'M') // MMC init
	{
		i = 0;
		while (SD_WriteR1(1, 0) != 0) // CMD1 for MMC  init
			if(++i > 200) // MMC init failed
			{
				UART_Print("MMC init failed.\n\r");
				return -1;
			}
	}
	else
	{
		cardType = '1'; // SD V1
	}

	if(SD_WriteR1(16, 0x200) != 0) // forcing 512 bytes block addressing for FAT
		return -1;

	return 1; // init success
}

signed char SD_WriteSector(unsigned char *sectorBuffer, unsigned long sector)
{
	int i;
	unsigned char status;

	//if(byteAddressing == 1)
	//sector <<= 9;

	while(SPI_ReadWrite(0xFF) == 0x00);
	SPI_ReadWrite(0xFF);
	unsigned char test = SD_Write(24, sector);
	while(test)
	{
		test = SD_Write(24, sector);
		UART_Print("Writting sector failed. %d\n\r", test);
		//return -1;
	}

	SPI_ReadWrite(0xFF);
	SPI_ReadWrite(0xFF);
	CS_deactivate();
	SPI_ReadWrite(0xFE);		// data token

	for(i = 0; i < 512; ++i)
		SPI_ReadWrite(sectorBuffer[i]);

	SPI_ReadWrite(0xFF);	//CRC se zanemaruje
	SPI_ReadWrite(0xFF);
	CS_activate();

	while(((test = SPI_ReadWrite(0xFF)) & 0x01) == 0);
	status = (test >> 1) & 0b111;

	for(i = 0; i < 8; ++i)
		SPI_ReadWrite(0xFF);
	if(status == 0x02)
		return 1;

	CS_deactivate();

	UART_Print("Writting sector failed - 1 %d\n\r", test);
	return -1;
}

signed char SD_ReadSector(unsigned char *sectorBuffer, unsigned long sector)
{
	int i = 0;

//	if(byteAddressing == 1)
	//	sector <<= 9; // <--> sector *= 512

	while(SPI_ReadWrite(0xFF) == 0); // while card busy

	unsigned char test;
	test = SD_Write(17, sector);
	if(test)
	{
		UART_Print("Reading sector fail. %d\n\r", test);
		return -1;
	}

	while(SPI_ReadWrite(0xFF) != 0xFE) // waiting for card to send memory token
		if(++i > 800) // timeout
		{
			UART_Print("Reading timeout.\n\r");
			return -1;
		}

	for(i = 0; i < 512; ++i) // reading sector bytes
		*(sectorBuffer + i) = SPI_ReadWrite(0xFF);

	SPI_ReadWrite(0xFF); // ignoring cheksum
	SPI_ReadWrite(0xFF); // ignoring cheksum
	CS_deactivate();

	for(i = 0; i < 8; ++i)
			SPI_ReadWrite(0xFF);

	FAT.currentSector = sector;
	return 1;
}

static signed char FAT_FindFreeCluster(void)
{
	unsigned int sector = FAT.lastFreeFatSector, offset = 0;

	while(sector < (FAT.sizeOfFAT + FAT.fatStartAddress))
	{
		SD_ReadSector(sectorBuffer, sector);
		UART_Print("Reading sector %d\n\r", sector);

		//if((sectorBuffer[510] == 0) && (sectorBuffer[511] == 0))
		//{
		offset = 0;
		while(offset < 512)
		{
			if((sectorBuffer[offset] == 0) && (sectorBuffer[offset + 1] == 0))
			{
				FAT.nextFreeCluster = (offset / 2) + ((sector - FAT.fatStartAddress) * 256);
				//FAT.lastFreeFatSector = sector;
				UART_Print("Next free cluster: %d\n\r", FAT.nextFreeCluster);
				FAT.lastFreeFatSector = sector;
				return 1;
			}

			offset += 2;
			//	}
		}
		sector++;
	}

	return -1;
}

char FAT_Init(void)
{
	SD_ReadSector(sectorBuffer, 0);
	//SD_PrintSector(sectorBuffer);

	//if( (sectorBuffer[0] != 0xEB) || (sectorBuffer[2] != 0x90)) // getting info about dynamic rellocation factor of sectors on SD card
	//{
	//FAT.bootAddress = //(((unsigned long)sectorBuffer[0x1C9]) << 24) | ( ((unsigned long)sectorBuffer[0x1C8])<< 16) |
		//	(sectorBuffer[0x1C7] << 8) | sectorBuffer[0x1C6];

	//SD_ReadSector(sectorBuffer, FAT.bootAddress);
	//}

	FAT.clusterSize = *(sectorBuffer + 0x0D); // num of sectors per allocation unit, aka cluster- NOT NUMBER OF BYTES!!!
	FAT.fatStartAddress = (((*(sectorBuffer + 0x0F)) << 8) | (*(sectorBuffer + 0x0E))) + FAT.bootAddress;
	FAT.sizeOfFAT = ((*(sectorBuffer + 0x17)) << 8) | (*(sectorBuffer + 0x16));
	FAT.rootStartAddress = FAT.fatStartAddress + (*(sectorBuffer + 0x10)) * FAT.sizeOfFAT;
	FAT.sizeOfRoot = ((sectorBuffer[0x11] | (((unsigned int)sectorBuffer[0x12]) << 8)) * 32) / 512;
	FAT.dataStartAddress = FAT.rootStartAddress + FAT.sizeOfRoot;
	FAT.lastFreeFatSector = FAT.fatStartAddress;

	FAT_FindFreeCluster();

	UART_Print("LBA- BOOT start address: %d\n\r", FAT.bootAddress);
	UART_Print("Allocation unit- cluster size: %d\n\r", FAT.clusterSize);
	UART_Print("FAT start address: %d\n\r", FAT.fatStartAddress);
	UART_Print("Size of FAT: %d\n\r", FAT.sizeOfFAT);
	UART_Print("Root start address: %d\n\r", FAT.rootStartAddress);
	UART_Print("Size of root: %d\n\r", FAT.sizeOfRoot);
	UART_Print("Data start address: %d\n\r", FAT.dataStartAddress);

	SD_ReadSector(sectorBuffer, FAT.rootStartAddress);
	// deskriptor root direktorijuma ne postoji, postoji samo data deo, sto je sam root, gde se cuvaju deskriptori filesystema
	// direktorijum sa indeksom nula je root direktorijum
	directoryHead = (struct sDirectory *) malloc(sizeof(struct sDirectory));
	directoryHead->parentDirectoryIndex = 0;
	directoryHead->dataStartSector = FAT.rootStartAddress;
	directoryHead->descriptorOffset = directoryHead->descriptorSector = 0;
	directoryHead->index = 0;
	directoryHead->nextNode = directoryHead->previousNode = NULL;
	directoryHead->numOfFiles = 0;
	nextFreeDirectoryIndex++;
	numOfOpenedDirectories++;

	/*fileHead = (struct sFile *) malloc(sizeof(sFile));
	fileHead->previousNode = fileHead->nextNode = NULL;
	fileHead->dataEndCluster = fileHead->dataStartSector = 0;
	fileHead->index = 1;*/
	numOfOpenedFiles = 0;
	nextFreeFileIndex = 1;

	return 1;
}

char getCardType(void)
{
	return cardType;
}

unsigned char FAT_OpenFile(char *filename, unsigned char parentDirectoryIndex)
{
	unsigned char i = 0;
	struct sFile *File = fileHead;
	struct sDirectory *Directory = directoryHead;

	while(File != NULL) // searching trough the list if file already opened
	{
		if(File->parentDirectoryIndex == parentDirectoryIndex) // file has same parent directory as needed
		{
			i = 0;
			while(i < 8)
			{
				if(File->filename[i] != *(filename + i))
					break;
				i++;
			}

			if(i == 8)
				return File->index;// file already opened and exists in list
		}
		File = File->nextNode;
	}

	while(Directory != NULL) // searching for parent directory
	{
		if(Directory->index == parentDirectoryIndex)
			break;

		Directory = Directory->nextNode;
	}

	if(Directory == NULL) // parent not opened, can't open file
		return 0;

	// swithing sector only if necessary
	// need to have access to descriptors of files in parent directory-> reading first data block of parent directory
	unsigned int tempSector = Directory->dataStartSector;
	unsigned int sectorOffset;
	if(Directory->dataStartSector != FAT.currentSector)
		SD_ReadSector(sectorBuffer, tempSector);

	unsigned int clusterNum;
	unsigned int sectorInCluster = 0;
	if(parentDirectoryIndex != 0)
	{
		clusterNum = 2 + (Directory->dataStartSector - FAT.dataStartAddress) / FAT.clusterSize;
		//UART_Print("Cluster ", clusterNum);
		//UART_Print("Data start at ", Directory[parentDirectoryIndex].dataStartSector);
	}

	File = (struct sFile *) malloc(sizeof(struct sFile)); // creating new node for file to be opened
	i = 0;
	while(i < 8)
	{
		File->filename[i] = *(filename + i);
		++i;
	}
	while(i < 8)
		File->filename[i++] = ' ';

	do
	{
		sectorOffset = 0;
		unsigned char temp;
		//UART_Print("Reading sector ", tempSector);
		while(sectorOffset < 512)
		{
			temp = 0;
			//while((sectorBuffer[sectorOffset + temp] == filename[temp]) && temp < 8)
			while((sectorBuffer[sectorOffset + temp] == File->filename[temp]) && temp < 8)
				temp++;

			if(temp == 8) // file found
			{
				File->index = nextFreeFileIndex;
				UART_Print("File found\n\r");
				//UART_Print("Data found on sector ", tempSector);
				break;
			}
			sectorOffset += 32;
		}

		if(sectorOffset >= 512) // end of sector
		{
			if(parentDirectoryIndex == 0) // file is on root
				if ((tempSector - FAT.rootStartAddress) >= FAT.sizeOfRoot) // PROVERI!!!
				{
					UART_Print("File not found on ROOT\n\r");
					free(File);
					return 0; // end of root, file not found
				}
				else
					tempSector++;
			else
			{
				// ZA PRETRAGU VAN ROOTa
				// potrebno naci sledeci sektor direktorijuma preko fata
				if(++sectorInCluster >= FAT.clusterSize)
				{
					tempSector = FAT.fatStartAddress + (clusterNum * 2) / 512;
					sectorOffset = (clusterNum * 2) % 512;

					SD_ReadSector(sectorBuffer, tempSector);

					clusterNum = ((sectorBuffer[sectorOffset + 1] << 8) | sectorBuffer[sectorOffset]);
					if(clusterNum == 0xFFFF) // end of directory, file not found
					{
						UART_Print("File not found on DATA\n\r");
						free(File);
						return 0;
					}

					tempSector = (clusterNum - 2) * FAT.clusterSize + FAT.dataStartAddress;
					sectorInCluster = 0;
				}
				else
					tempSector++;
			}

			SD_ReadSector(sectorBuffer, tempSector);
			sectorOffset = 0;
		}
	}while(File->index == 0);

	// FILE FOUND, GETTING INFO STORED IN  ARRAY of opened files, IT REMAINS THERE UNTIL FILE REMAINS OPENED
	File->descriptorSector = FAT.currentSector;
	File->parentDirectoryIndex = parentDirectoryIndex;
	File->descriptorOffset = sectorOffset;
	File->dataStartSector = ( (sectorBuffer[sectorOffset + 0x1A] | (sectorBuffer[sectorOffset + 0x1B] << 8)) - 2 )
			                                                         * FAT.clusterSize + FAT.dataStartAddress;

	unsigned int cluster = sectorBuffer[sectorOffset + 0x1A] | (sectorBuffer[sectorOffset + 0x1B] << 8);
	do
	{
		File->dataEndCluster = cluster;
		tempSector = FAT.fatStartAddress + cluster / 256;
		sectorOffset = (cluster * 2) % 512;
		SD_ReadSector(sectorBuffer, tempSector);
		cluster = sectorBuffer[sectorOffset] | (sectorBuffer[sectorOffset + 1] << 8);
	}while(cluster != 0xFFFF);

	SD_ReadSector(sectorBuffer, File->dataStartSector);
	nextFreeFileIndex++;

	File->previousNode = NULL;
	if(fileHead != NULL)
	{
		File->nextNode = fileHead;
		fileHead->previousNode = File;
	}
	else
		fileHead->nextNode = NULL;

	fileHead = File;

	return File->index;
}

unsigned char FAT_OpenDirectory(char *directoryName, unsigned char parentDirectoryIndex)
{
	unsigned char i = 0;
	struct sDirectory *Directory = directoryHead;

	while(Directory != NULL) // search if directory already opened
	{
		if(Directory->parentDirectoryIndex == parentDirectoryIndex)
		{
			i = 0;
			while(i < 8)
			{
				if(Directory->directoryName[i] != *(directoryName + i))
					break;
				++i;
			}

			if(i == 8)
				return Directory->index;

		}
		Directory = Directory->nextNode;
	}

	Directory = directoryHead;
	while(Directory != NULL) // search for parent dir
	{
		if(Directory->index == parentDirectoryIndex)
			break;
		Directory = Directory->nextNode;
	}

	if(Directory == NULL)
		return 0xFF; // parent dir not opened

	// need to have access to descriptors of files in parent directory-> reading first data block of parent directory
	unsigned int tempSector = Directory->dataStartSector;
	unsigned int sectorOffset;
	if(Directory->dataStartSector != FAT.currentSector)
		SD_ReadSector(sectorBuffer, tempSector);

	unsigned int clusterNum;
	unsigned int sectorInCluster = 0;
	if(parentDirectoryIndex != 0)
		clusterNum = 2 + (Directory->dataStartSector - FAT.dataStartAddress) / FAT.clusterSize;

	Directory = (struct sDirectory *) malloc(sizeof(struct sDirectory)); // reserving new node for list

	i = 0;
	while(i < 8)
	{
		Directory->directoryName[i] = *(directoryName + i);
		i++;
	}
	while(i < 8)
		Directory->directoryName[i++] = ' ';

	do
	{
		sectorOffset = 0;
		unsigned char temp;
		while(sectorOffset < 512)
		{
			temp = 0;
			//if(sectorBuffer[temp++] == 0x2E)
			{
				//while((sectorBuffer[sectorOffset + temp] == directoryName[temp]) && temp < 8)
				while((sectorBuffer[sectorOffset + temp] == Directory->directoryName[temp]) && temp < 8)
					temp++;

				if(temp == 8) // file found
				{
					Directory->index = nextFreeDirectoryIndex;
					UART_Print("Directory found.\n\r");
					break;
				}
			}
			sectorOffset += 32;
		}

		if(sectorOffset >= 512) // end of sector
		{
			if(parentDirectoryIndex == 0) // file is on root
				if((tempSector - FAT.rootStartAddress) == FAT.sizeOfRoot) // PROVERI!!!
				{
					free(Directory);
					UART_Print("Directory not found on ROOT.\n\r");
					return 0; // end of root, file not found
				}
				else
					tempSector++;
			else
			{
				// ZA PRETRAGU VAN ROOTa
				// potrebno naci sledeci sektor direktorijuma preko fata
				if(++sectorInCluster >= FAT.clusterSize)
				{
					tempSector = FAT.fatStartAddress + (clusterNum * 2) / 512;
					sectorOffset = (clusterNum * 2) % 512;

					SD_ReadSector(sectorBuffer, tempSector);

					clusterNum = ((sectorBuffer[sectorOffset + 1] << 8) | sectorBuffer[sectorOffset]);
					if(clusterNum == 0xFFFF)
					{
						UART_Print("Directory not found on DATA.\n\r");
						free(Directory);
						return 0;
					}

					tempSector = (clusterNum - 2) * FAT.clusterSize + FAT.dataStartAddress;
					sectorInCluster = 0;
				}
				else
					tempSector++;
			}

			SD_ReadSector(sectorBuffer, tempSector);
			sectorOffset = 0;
		}
	}while(Directory->index == 0);

	// FILE FOUND, GETTING INFO STORED IN  ARRAY of opened files, IT REMAINS THERE UNTIL FILE REMAINS OPENED
	Directory->descriptorSector = FAT.currentSector;
	Directory->parentDirectoryIndex = parentDirectoryIndex;
	Directory->descriptorOffset = sectorOffset;
	Directory->dataStartSector = ((sectorBuffer[sectorOffset + 0x1A] | (sectorBuffer[sectorOffset + 0x1B] << 8)) - 2)
																					   * FAT.clusterSize + FAT.dataStartAddress;

	unsigned int cluster = sectorBuffer[sectorOffset + 0x1A] | (sectorBuffer[sectorOffset + 0x1B] << 8);
	do
	{
		Directory->dataEndCluster = cluster;
		tempSector = FAT.fatStartAddress + cluster / 256;
		sectorOffset = (cluster * 2) % 512;
		SD_ReadSector(sectorBuffer, tempSector);
		cluster = sectorBuffer[sectorOffset] | (sectorBuffer[sectorOffset + 1] << 8);
	}while(cluster != 0xFFFF);

	SD_ReadSector(sectorBuffer, Directory->dataStartSector);
	nextFreeDirectoryIndex++;

	Directory->previousNode = NULL;
	if(directoryHead != NULL)
	{
		Directory->nextNode = directoryHead;
		Directory->previousNode = Directory;
	}
	else
		directoryHead->nextNode = NULL;

	directoryHead = Directory;
	return Directory->index;
}

char FAT_WriteFile(unsigned char fileIndex, char mode, char *content)
{
	// must check if file opened
	unsigned int sector, sectorOffset;
	unsigned int sectorCounter, byteCounter, arrayCounter;
	unsigned long contentLength = 0;
	unsigned long newSize, oldSize;

	struct sFile *File = fileHead;
	while(File != NULL) // searching trough file list for file node
	{
		if(File->index == fileIndex)
			break;

		File = File->nextNode;
	}

	if(File == NULL)
		return -1;

	while(*(content + contentLength) != '\0')
		++contentLength;

	if(FAT.currentSector != File->descriptorSector)
		SD_ReadSector(sectorBuffer, File->descriptorSector);

	if(mode == OVERWRITE)
	{
		unsigned int cluster;

		// getting data start cluster
		cluster = sectorBuffer[File->descriptorOffset + 0x1A] | ((unsigned int)(sectorBuffer[File->descriptorOffset + 0x1B]) << 8);
		File->dataEndCluster = cluster;
		FAT.lastFreeFatSector = FAT.fatStartAddress + cluster / 256; // start of file cluster list in FAT area

		sector = FAT.lastFreeFatSector;
		sectorOffset = (cluster * 2) % 512;
		SD_ReadSector(sectorBuffer, sector);
		cluster = sectorBuffer[sectorOffset] | (((unsigned int)sectorBuffer[sectorOffset + 1]) << 8);
		sectorBuffer[sectorOffset] = sectorBuffer[sectorOffset + 1] = 0xFF;
		SD_WriteSector(sectorBuffer, sector);

		do
		{
			sector = FAT.fatStartAddress + cluster / 256; // getting position of cluster in FAT
			sectorOffset = (cluster * 2) % 512;
			if(sector != FAT.currentSector)
				SD_ReadSector(sectorBuffer, sector);
			cluster = sectorBuffer[sectorOffset] | ((unsigned int)(sectorBuffer[sectorOffset + 1]) << 8); // getting next cluster in the list
			sectorBuffer[sectorOffset] = sectorBuffer[sectorOffset + 1] = 0;
			SD_WriteSector(sectorBuffer, sector);
		}while(cluster != 0xFFFF);

		SD_ReadSector(sectorBuffer, File->descriptorSector);
		oldSize = 0;
	}
	else
	{
		oldSize = sectorBuffer[File->descriptorOffset + 0x1C]
	   					| ((unsigned long) (sectorBuffer[File->descriptorOffset + 0x1D]) << 8)
	   					| ((unsigned long) (sectorBuffer[File->descriptorOffset + 0x1E]) << 16)
	   					| ((unsigned long) (sectorBuffer[File->descriptorOffset + 0x1F]) << 24);
	}
	newSize = contentLength + oldSize;
	sectorBuffer[File->descriptorOffset + 0x1C] = newSize;
	sectorBuffer[File->descriptorOffset + 0x1D] = newSize >> 8;
	sectorBuffer[File->descriptorOffset + 0x1E] = newSize >> 16;
	sectorBuffer[File->descriptorOffset + 0x1F] = newSize >> 24;
	if(SD_WriteSector(sectorBuffer, File->descriptorSector) == -1)
	{
		UART_Print("Failed at writting desriptor.\n\r");
		return -1;
	}

	//sectorCounter = oldSize / 512; //??? glupost
	sectorCounter = (oldSize / 512) % FAT.clusterSize; // number of last sector in last taken cluster
	sector = sectorCounter + (File->dataEndCluster - 2) * FAT.clusterSize + FAT.dataStartAddress;
	arrayCounter = oldSize % 512;

	SD_ReadSector(sectorBuffer, sector);

	for(byteCounter = 0; byteCounter < contentLength; byteCounter++)
	{
		sectorBuffer[arrayCounter++] = *(content + byteCounter);
		if(arrayCounter == 512)
		{
			SD_WriteSector(sectorBuffer, sector);
			if(++sectorCounter >= FAT.clusterSize) // reserve new cluster
			{
				UART_Print("New cluster reservation during write needed.\n\r");
				sector = FAT.fatStartAddress + (File->dataEndCluster /256);
				sectorOffset = (File->dataEndCluster * 2) % 512;
				SD_ReadSector(sectorBuffer, sector);
				sectorBuffer[sectorOffset] = FAT.nextFreeCluster;
				sectorBuffer[sectorOffset + 1] = FAT.nextFreeCluster >> 8;
				if(sector != FAT.fatStartAddress + (FAT.nextFreeCluster / 256))
				{
					SD_WriteSector(sectorBuffer, sector);
					sector = FAT.fatStartAddress + (FAT.nextFreeCluster / 256);
					if(FAT.currentSector != sector)
						SD_ReadSector(sectorBuffer, sector);
				}
				sectorOffset = (FAT.nextFreeCluster * 2) % 512;
				sectorBuffer[sectorOffset] = sectorBuffer[sectorOffset + 1] = 0xFF;
				SD_WriteSector(sectorBuffer, sector);
				File->dataEndCluster = FAT.nextFreeCluster;
				FAT_FindFreeCluster();
				sector = FAT.dataStartAddress + (File->dataEndCluster - 2) * FAT.clusterSize;
				sectorCounter = 0;
			}
			else
				sector++;

			arrayCounter = 0;
		}
	}

	if(arrayCounter != 0)
	{
		UART_Print("Trying to write data on sector %d\n\r", sector);

		if(SD_WriteSector(sectorBuffer, sector) == -1)
			UART_Print("Writting data failed\n\r");
	}

	return 1;
}

char FAT_ReadFile(unsigned char fileIndex, unsigned char *buffer, unsigned long startOffset, unsigned int numOfBytesToRead)
{
	unsigned int sector, cluster, sectorOffset, sectorCounter, i;
	unsigned long size;
	struct sFile *File = fileHead;

	//while(File->index != fileIndex)
	while(File != NULL)
	{
		//if(File->nextNode == NULL)
		if(File->index == fileIndex)
			break;

		File = File->nextNode;
	}

	if(File == NULL)
		return 0;

	if(FAT.currentSector != File->descriptorSector)
		SD_ReadSector(sectorBuffer, File->descriptorSector);

	size = sectorBuffer[File[fileIndex].descriptorOffset + 0x1C] |
			((unsigned long)(sectorBuffer[File->descriptorOffset + 0x1D]) << 8) |
			((unsigned long)(sectorBuffer[File->descriptorOffset + 0x1E]) << 16) |
			((unsigned long)(sectorBuffer[File->descriptorOffset + 0x1F]) << 24);

	numOfBytesToRead = (numOfBytesToRead + startOffset) >= size ? size : numOfBytesToRead;

	cluster = ((unsigned int)(sectorBuffer[File->descriptorOffset + 0x1B]) << 8) | sectorBuffer[File->descriptorOffset + 0x1A]; // start cluster
	sector = FAT.dataStartAddress + (cluster - 2) * FAT.clusterSize; // start sector
	cluster += (startOffset / (FAT.clusterSize * 512)); // adding offset
	sectorCounter = (startOffset / 512) % FAT.clusterSize; // number sector in cluster from where reading begins
	sector += sectorCounter; // adding offset
	sectorOffset = startOffset % 512;

	for(i = 0; i < numOfBytesToRead; ++i)
	{
		SD_ReadSector(sectorBuffer, sector);
		*(buffer + i) = sectorBuffer[sectorOffset++];

		if(sectorOffset == 512)
		{
			if(++sectorCounter >= FAT.clusterSize)
			{
				sectorCounter = 0;
				sector = FAT.fatStartAddress + cluster / 256;
				sectorOffset = (2 * cluster) % 512;
				SD_ReadSector(sectorBuffer, sector);
				cluster = ((unsigned int)(sectorBuffer[sectorOffset + 1]) << 8) | sectorBuffer[sectorOffset];
				if(cluster == 0xFFFF)
					return -1;
				sector = FAT.dataStartAddress + (cluster - 2) * FAT.clusterSize;
			}
			else
				sector++;

			sectorOffset = 0;
		}
	}

	return 1;
}

unsigned char FAT_CreateDirectory(char *name, unsigned char parentDirectoryIndex)
{

	return 0;
}

static unsigned char reserved[10] = {0x18, 0x1A, 0x40, 0x77, 0x91, 0x46, 0x91, 0x46, 00, 00};
unsigned char FAT_CreateFile(char *name, unsigned char parentDirectoryIndex)
{
	unsigned int sector, sectorOffset = 0;
	struct sDirectory *Directory = directoryHead;

	while(Directory != NULL)
	{
		//if(Directory->nextNode == NULL) // parent dir not opened
		if(Directory->index == parentDirectoryIndex)
			break;

		Directory = Directory->nextNode;
	}

	if(Directory == NULL)
		return 0xFF;

	sector = Directory->dataStartSector;

	if(FAT.currentSector != Directory->dataStartSector)
		SD_ReadSector(sectorBuffer, Directory->dataStartSector);

	if(parentDirectoryIndex == 0)
	{
		while(sectorBuffer[sectorOffset] != 0 && sectorBuffer[sectorOffset] != 0xE5)
			if(sectorOffset >= 512)
			{
				sectorOffset = 0;
				if((++sector - FAT.rootStartAddress) > FAT.sizeOfRoot)
					return 0;
				SD_ReadSector(sectorBuffer, sector);
			}
			else
				sectorOffset += 32;
	}
	else
	{
		// stvaranje prako fata
	}

	unsigned char i = 0;
	//for(i = 0; i < 8; ++i)
	while(name[i] != '\0' && i < 8)
	{
		sectorBuffer[sectorOffset + i] = name[i];
		i++;
	}
	while(i < 8)
	{
		sectorBuffer[sectorOffset + i] = ' ';
		i++;
	}

	sectorBuffer[sectorOffset + 0x08] = sectorBuffer[sectorOffset + 0x0A] = 'T';
	sectorBuffer[sectorOffset + 0x09] = 'X';
	sectorBuffer[sectorOffset + 0x0B] = 0x20;
	for(i = 0; i < 10; i++)
		sectorBuffer[sectorOffset + 0x0C + i] = reserved[i];

	sectorBuffer[sectorOffset + 0x1A] = FAT.nextFreeCluster;
	sectorBuffer[sectorOffset + 0x1B] = FAT.nextFreeCluster >> 8;
	sectorBuffer[sectorOffset + 0x1C] = 0;
	sectorBuffer[sectorOffset + 0x1D] = 0;
	sectorBuffer[sectorOffset + 0x1E] = 0;
	sectorBuffer[sectorOffset + 0x1F] = 0;

	sectorBuffer[sectorOffset + 0x16] = 0x48;
	sectorBuffer[sectorOffset + 0x17] = 0x77;
	sectorBuffer[sectorOffset + 0x18] = 0x91;
	sectorBuffer[sectorOffset + 0x19] = 0x46;

	UART_Print("Attempting to write descriptor sector creating file %d\n\r ", sector);
	UART_Print("Reserved cluster for file: %d\n\r", FAT.nextFreeCluster);
	if(SD_WriteSector(sectorBuffer, sector) == -1)
		return 0;

	struct sFile *File = (struct sFile *) malloc(sizeof(struct sFile));
	File->descriptorSector = sector;
	File->descriptorOffset = sectorOffset;

	sector = (FAT.nextFreeCluster * 2) / 512 + FAT.fatStartAddress;
	sectorOffset = (FAT.nextFreeCluster * 2) % 512;
	if(SD_ReadSector(sectorBuffer, sector) == -1)
		return 0;

	sectorBuffer[sectorOffset] = sectorBuffer[sectorOffset + 1] = 0xFF;
	UART_Print("Attempting to write FAT sector creating file %d\n\r", sector);

	if(SD_WriteSector(sectorBuffer, sector) == -1)
		return 0xFF;

	File->index = nextFreeFileIndex++;
	File->dataStartSector = (FAT.nextFreeCluster - 2) * FAT.clusterSize + FAT.dataStartAddress;
	File->dataEndCluster = FAT.nextFreeCluster;
	File->parentDirectoryIndex = parentDirectoryIndex;
	for(i = 0; i < 8; ++i)
		File->filename[i] = name[i];

	FAT_FindFreeCluster();

	File->previousNode = NULL;
	if(fileHead != NULL)
	{
		File->nextNode = fileHead;
		fileHead->previousNode = File;
	}
	else
		File->nextNode = NULL;

	fileHead = File;

	SD_ReadSector(sectorBuffer, File->descriptorSector);
	return File->index;
}

char FAT_RemoveFile(unsigned char fileIndex)
{
	unsigned int cluster, sector, sectorOffset;
	struct sFile *File = fileHead;


	while(File != NULL)
	{
		if(File->index == fileIndex)
			break;

		File = File->nextNode;
	}

	if(File == NULL)
		return -1;

	SD_ReadSector(sectorBuffer, File->descriptorSector);
	cluster = sectorBuffer[File->descriptorOffset + 0x1A] | // start cluster
					((unsigned int)(sectorBuffer[File[fileIndex].descriptorOffset + 0x1B]) << 8);
	FAT.lastFreeFatSector = FAT.fatStartAddress + cluster / 256;

	unsigned char i;
	for(i = 0; i < 32; ++i)
		sectorBuffer[File->descriptorOffset + i] = 0; // deleteing descriptor
	SD_WriteSector(sectorBuffer, File->descriptorSector);
	do
	{
		sector = FAT.fatStartAddress + cluster / 256;
		sectorOffset = (cluster * 2) % 512;
		SD_ReadSector(sectorBuffer, sector);
		cluster = sectorBuffer[sectorOffset] | (((unsigned int)sectorBuffer[sectorOffset + 1]) << 8);
		sectorBuffer[sectorOffset] = sectorBuffer[sectorOffset + 1] = 0;
		SD_WriteSector(sectorBuffer, sector);
	}while(cluster != 0xFFFF);

	if(File == fileHead)
	{
		fileHead = fileHead->nextNode;
		fileHead->previousNode = NULL;
	}
	else
	{
		File->previousNode->nextNode = File->nextNode;
		File->nextNode->previousNode = File->previousNode;
	}

	free(File);
	return 0;
}

char FAT_CloseFile(unsigned char fileIndex)
{
	struct sFile *File = fileHead;

	while(File != NULL)
	{
		if(File->index == fileIndex)
			break;

		File = File->nextNode;
	}

	if(File == fileHead)
	{
		fileHead = fileHead->nextNode;
		fileHead->previousNode = NULL;
	}
	else
	{
		File->nextNode->previousNode = File->previousNode;
		File->previousNode->nextNode = File->nextNode;
	}

	free(File);
	return 0;
}

unsigned char FAT_CloseDirectory(unsigned char directoryIndex)
{
	struct sDirectory *Directory = directoryHead;

	while(Directory != NULL)
	{
		if(Directory->index == directoryIndex)
			break;

		Directory = Directory->nextNode;
	}

	if(Directory == directoryHead)
	{
		directoryHead = directoryHead->nextNode;
		directoryHead->previousNode = NULL;
	}
	else
	{
		Directory->nextNode->previousNode = Directory->previousNode;
		Directory->previousNode->nextNode = Directory->nextNode;
	}

	free(Directory);
	return 0;
}
