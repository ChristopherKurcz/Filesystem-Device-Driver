////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the FS3 storage system.
//
//   Author        : Christopher Kurcz
//   Last Modified : 12/9/2021
//

// Includes
#include <string.h>
#include <cmpsc311_log.h>
#include <stdlib.h>

// Project Includes
#include <fs3_driver.h>
#include <fs3_controller.h>
#include <fs3_cache.h>
#include <fs3_network.h>

// Defines
#define SECTOR_INDEX_NUMBER(x) ((int)(x/FS3_SECTOR_SIZE))

// Static Global Variables
	bool diskMounted = false;
	FS3File FS3FileArray[FS3_MAX_TOTAL_FILES];
	int FS3DiskMap[FS3_MAX_TRACKS][FS3_TRACK_SIZE];
	int currentDiskTrack;

// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_mount_disk
// Description  : FS3 interface, mount/initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_mount_disk(void) {
	// checks that the disk is not already mounted
	if(diskMounted == true){
		return(-1);
	}

	// constructs command block for the mount opcode
	FS3CmdBlk cmdblock = construct_fs3_cmdblock(FS3_OP_MOUNT, 0, 0, 0);

	// creates a return command block and preforms the network system call with the command block
	FS3CmdBlk returnCmdblock;
	if(network_fs3_syscall(cmdblock, &returnCmdblock, NULL) == -1){
		// if the network call was a failure, returns -1
		return(-1);
	}

	// makes sure the system call went successfully
	if (getReturnBit(returnCmdblock) != 0){
		return(-1);
	}

	// sets the global disk mounted variable to true
	diskMounted = true;

	// sets the global current track variable to -1
	currentDiskTrack = -1;

	// sets each file in the file array to not have been created yet
	int i;
	for(i = 0; i<FS3_MAX_TOTAL_FILES; i++){
		FS3FileArray[i].created = false;
	}

	// sets each entry in the disk map to -1 (meaning there is no file there)
	int j;
	for(i = 0; i<FS3_MAX_TRACKS; i++){
		for(j = 0; j<FS3_TRACK_SIZE; j++){
			FS3DiskMap[i][j] = -1;
		}
	}

	return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_unmount_disk
// Description  : FS3 interface, unmount the disk, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_unmount_disk(void) {
	// checks that the disk is mounted
	if(diskMounted == false){
		return(-1);
	}

	// constructs command block for the unmount opcode
	FS3CmdBlk cmdblock = construct_fs3_cmdblock(FS3_OP_UMOUNT, 0, 0, 0);

	// creates a return command block and preforms the network system call with the command block
	FS3CmdBlk returnCmdblock;
	if(network_fs3_syscall(cmdblock, &returnCmdblock, NULL) == -1){
		// if the network call was a failure, returns -1
		return(-1);
	}

	// makes sure the system call went successfully
	if (getReturnBit(returnCmdblock) != 0){
		return(-1);
	}

	// updates the global variable
	diskMounted = false;

	// closes every file in the file array that has been created
	int i;
	for(i = 0; i<FS3_MAX_TOTAL_FILES; i++){
		if(FS3FileArray[i].created == true){
			FS3FileArray[i].open = false;
		} else {
			break;
		}
	}

	return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t fs3_open(char *path) {
	bool fileExists = false;
	int16_t fileHandle;
	
	int i;

	// checks if the file already exists, keeping track of its' file handle if it does
	for(i = 0; i<FS3_MAX_TOTAL_FILES; i++){
		if (strcmp(FS3FileArray[i].name,path) == 0){
			fileExists = true;
			fileHandle = (int16_t) i;
			break;
		}
	}

	if (fileExists == true){
		// if the file exists, it opens the file and sets the position to 0
		FS3FileArray[fileHandle].open = true;
		FS3FileArray[fileHandle].position = 0;
		return(fileHandle);	
	} else {
		// if the file does not exist, will attempt to create a new file
		// checks that max number of files are not already in disk
		if(FS3FileArray[FS3_MAX_TOTAL_FILES].created == true){
			return(-1);
		}

		// finds the next avaliable file handle for the new file
		for(i = 0; i<FS3_MAX_TOTAL_FILES; i++){
			if(FS3FileArray[i].created == false){
				fileHandle = (int16_t) i;
				break;
			}
		}

		// intializes all variables for the new file
		FS3FileArray[fileHandle].created = true;
		FS3FileArray[fileHandle].length = 0;
		FS3FileArray[fileHandle].position = 0;
		FS3FileArray[fileHandle].open = true;
		strcpy(FS3FileArray[fileHandle].name,path);

		return(fileHandle);
	}
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t fs3_close(int16_t fd) {
	// checks that the file handle is within the bounds of the maximum number of files
	if((fd > FS3_MAX_TOTAL_FILES) || (fd < (int16_t)0)){
		return(-1);
	}
	// checks that the file has already been created
	if(FS3FileArray[fd].created == false){
		return(-1);
	}
	// checks that the file is not already open
	if(FS3FileArray[fd].open == true){
		return(-1);
	}

	// closes the file
	FS3FileArray[fd].open = false;
	return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t fs3_read(int16_t fd, void *buf, int32_t count) {
	// checks that the disk is mounted
	if(diskMounted == false){
		return(-1);
	}
	// checks that the file handle is within the bounds of the maximum number of files
	if((fd > FS3_MAX_TOTAL_FILES) || (fd < (int16_t)0)){
		return(-1);
	}
	// checks that the file has already been created
	if(FS3FileArray[fd].created == false){
		return(-1);
	}
	// checks that the file is not closed
	if(FS3FileArray[fd].open == false){
		return(-1);
	}
	// checks that there are any bytes to even read
	if((FS3FileArray[fd].length - FS3FileArray[fd].position) == 0){
		return(0);
	}

	// initializes the number of bytes read as 0
	int bytesRead = 0;

	// while there are more bytes to read, the function will continue to preform read operations
	while(count > 0){
		// sets the number of bits to be read on this read operation
		int diskBitCount = count;
		// finds the position the file is on inside the sector its overall position is in
		int positionInSector = FS3FileArray[fd].position % FS3_SECTOR_SIZE;
		// saves the track and sector the position of the file is on
		int currentFileTrack = find_current_track(fd);
		int currentFileSector = find_current_sector(fd);

		//switches to the correct track
		if(currentDiskTrack != currentFileTrack){
			switch_disk_track(currentFileTrack);
		}

		// allocates memory for a new buffer for the data from the disk
		void *diskBuf = malloc(FS3_SECTOR_SIZE);

		// tries to get the data from the cache
		void *cacheData = fs3_get_cache((FS3TrackIndex)currentFileTrack, (FS3SectorIndex)currentFileSector);

		// if the data was not in the cache, preform a read system call and get it fomr the disk
		if(cacheData == NULL){
			// constructs command block for the read opcode
			FS3CmdBlk cmdblock = construct_fs3_cmdblock(FS3_OP_RDSECT, currentFileSector, 0, 0);

			// creates a return command block and preforms the network system call with the command block and buffer
			FS3CmdBlk returnCmdblock;
			if(network_fs3_syscall(cmdblock, &returnCmdblock, diskBuf) == -1){
				// if the network call was a failure, returns -1
				return(-1);
			}

			// makes sure the system call went successfully
			if (getReturnBit(returnCmdblock) != 0){
				return(-1);
			}
		} else {
			// if the data was in the cache, copy it over to the disk Buffer
			memcpy(diskBuf, cacheData, FS3_SECTOR_SIZE);
		}

		// checks the make sure the amount of bytes trying to be read will not go past the size of the sector
		if((FS3_SECTOR_SIZE - positionInSector) < diskBitCount){
			diskBitCount = FS3_SECTOR_SIZE - positionInSector;
		}
		// first checks if the end of the file is in the same sector as the current position
		if((FS3FileArray[fd].length / FS3_SECTOR_SIZE) == (FS3FileArray[fd].position / FS3_SECTOR_SIZE)){
			// checks the make sure the amount of bytes trying to be read will not go past the length of the file
			if(((FS3FileArray[fd].length % FS3_SECTOR_SIZE) - positionInSector) < diskBitCount){
				diskBitCount = (FS3FileArray[fd].length % FS3_SECTOR_SIZE) - positionInSector;
			}
		}

		// copies diskBitCount bytes from the current position of the file to the user's buffer 
		memcpy(buf + bytesRead, diskBuf + positionInSector, diskBitCount);

		// updates metadata
		FS3FileArray[fd].position = FS3FileArray[fd].position + diskBitCount;
		if(FS3FileArray[fd].position > FS3FileArray[fd].length){
			FS3FileArray[fd].length = FS3FileArray[fd].position;
		}
		bytesRead = bytesRead + diskBitCount;
		count = count - diskBitCount;

		// deallocates the memory of the disk buffer
		free(diskBuf);
		
	}

	return(bytesRead);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//the count
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t fs3_write(int16_t fd, void *buf, int32_t count) {
	// checks that the disk is mounted
	if(diskMounted == false){
		return(-1);
	}
	// checks that the file handle is within the bounds of the maximum number of files
	if((fd > FS3_MAX_TOTAL_FILES) || (fd < (int16_t)0)){
		return(-1);
	}
	// checks that the file has already been created
	if(FS3FileArray[fd].created == false){
		return(-1);
	}
	// checks that the file is not closed
	if(FS3FileArray[fd].open == false){
		return(-1);
	}

	// initializes the number of bytes written as 0
	int bytesWritten = 0;

	// while there are more bytes to write, the function will continue to preform write operations
	while(count > 0){
		// sets the number of bits to be written on this write operation
		int diskBitCount = count;
		// finds the position the file is on inside the sector its overall position is in
		int positionInSector = FS3FileArray[fd].position % FS3_SECTOR_SIZE;
		// saves the track and sector the position of the file is on
		int sectorToWriteTo = find_current_sector(fd);
		int trackToWriteTo = find_current_track(fd);

		// if the position is at the beginning of a new sector, it will find an empty sector to write more data to
		if((sectorToWriteTo==-1)||(trackToWriteTo==-1)){
			trackToWriteTo = find_open_track();

			//switches to the new track
			if(currentDiskTrack != trackToWriteTo){
				switch_disk_track(trackToWriteTo);
			}

			sectorToWriteTo = find_open_sector();
		}

		//switches to the file's position's current track
		if(currentDiskTrack != trackToWriteTo){
			switch_disk_track(trackToWriteTo);
		}

		// allocates memory for a new buffer for the data for the disk
		void *diskBuf = malloc(FS3_SECTOR_SIZE);

		// if there is data already written in the sector about to be written to,
		//	it will read the data already there into the buffer
		if(FS3DiskMap[trackToWriteTo][sectorToWriteTo] != -1){

			// tries to get the data from the cache
			void *cacheData = fs3_get_cache(trackToWriteTo, sectorToWriteTo);

			// if the data was not in the cache, preform a read system call and get it fomr the disk
			if(cacheData == NULL){
				// constructs command block for the read opcode
				FS3CmdBlk readCmdblock = construct_fs3_cmdblock(FS3_OP_RDSECT, sectorToWriteTo, 0, 0);

				// creates a return command block and preforms the network system call with the command block and buffer
				FS3CmdBlk returnReadCmdblock;
				if(network_fs3_syscall(readCmdblock, &returnReadCmdblock, diskBuf) == -1){
					// if the network call was a failure, returns -1
					return(-1);
				}

				// makes sure the system call went successfully
				if (getReturnBit(returnReadCmdblock) != 0){
					return(-1);
				}
			} else {
				// if the data was in the cache, copy it over to the disk buffer
				memcpy(diskBuf, cacheData, FS3_SECTOR_SIZE);
			}

		}

		// checks the make sure the amount of bytes trying to be written will not go past the size of the sector
		if((FS3_SECTOR_SIZE - positionInSector) < count){
			diskBitCount = FS3_SECTOR_SIZE - positionInSector;
		}

		// copies diskBitCount bytes from the buffer into the disk buffer at the position the file is at
		memcpy(diskBuf+positionInSector, buf+bytesWritten, diskBitCount);

		// put the new data into the cahce
		fs3_put_cache(trackToWriteTo, sectorToWriteTo, diskBuf);

		// constructs command block for the write opcode
		FS3CmdBlk writeCmdblock = construct_fs3_cmdblock(FS3_OP_WRSECT, sectorToWriteTo, 0, 0);

		// creates a return command block and preforms the network system call with the command block and buffer
		FS3CmdBlk returnWriteCmdblock;
		if(network_fs3_syscall(writeCmdblock, &returnWriteCmdblock, diskBuf) == -1){
			// if the network call was a failure, returns -1
			return(-1);
		}

		// makes sure the system call went successfully
		if (getReturnBit(returnWriteCmdblock) != 0){
			return(-1);
		}

		// updates metadata
		FS3DiskMap[trackToWriteTo][sectorToWriteTo] = fd;
		FS3FileArray[fd].position = FS3FileArray[fd].position + diskBitCount;
		if(FS3FileArray[fd].position > FS3FileArray[fd].length){
			FS3FileArray[fd].length = FS3FileArray[fd].position;
		}
		count = count - diskBitCount;
		bytesWritten = bytesWritten + diskBitCount;

		// deallocates the memory used for the disk buffer
		free(diskBuf);
	}

	return(bytesWritten);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_seek(int16_t fd, uint32_t loc) {
	// checks that the file handle is within the bounds of the maximum number of files
	if((fd > FS3_MAX_TOTAL_FILES) || (fd < (int16_t)0)){
		return(-1);
	}
	// checks that the file has already been created
	if(FS3FileArray[fd].created == false){
		return(-1);
	}
	// checks that the location is not beyond the end of the file
	if(loc > FS3FileArray[fd].length){
		return(-1);
	}
	// checks that the file is not closed
	if(FS3FileArray[fd].open == false){
		return(-1);
	}

	// sets the position of the file to loc
	FS3FileArray[fd].position = loc;

	return (0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : switch_disk_track
// Description  : Switches the track the disk in on to a new track
//
// Inputs       : trackNum - the track number to switch to
// Outputs      : 0 if successful, -1 if failure

int switch_disk_track(int trackNum){
	// checks that the disk is mounted
	if(diskMounted == false){
		return(-1);
	}
	// checks to make sure the trackNum is valid
	if((trackNum<0) || (trackNum>FS3_MAX_TRACKS)){
		return(-1);
	}

	// creates trackInt to be able to send it to construct_fs3_cmdblock
	uint_fast32_t trackInt = (uint_fast32_t) trackNum;

	// constructs command block for the seek opcode
	FS3CmdBlk cmdblock = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trackInt, 0);

	// creates a return command block and preforms the network system call with the command block
	FS3CmdBlk returnCmdblock;
	if(network_fs3_syscall(cmdblock, &returnCmdblock, NULL) == -1){
		// if the network call was a failure, returns -1
		return(-1);
	}

	// makes sure the system call went successfully
	if (getReturnBit(returnCmdblock) != 0){
		return(-1);
	}

	// updates global current disk track variable
	currentDiskTrack = trackNum;

	return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : find_open_track
// Description  : Finds a track in which there is a sector with no data in it
//
// Outputs      : first track number with open sector, or -1 if all sectors full

int find_open_track(){
	int i;
	int j;

	// loops through the entire disk map
	for(i = 0; i<FS3_MAX_TRACKS; i++){
		for(j = 0; j<FS3_TRACK_SIZE; j++){
			// if a sector has no file data in it, it returns that track number
			if(FS3DiskMap[i][j] == -1){
				return(i);
			}
		}
	}

	return(-1);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : find_open_sector
// Description  : Finds a sector with no data in it on the current track
//
// Outputs      : number of first open sector, or -1 if all sectors on track are full

int find_open_sector(){
	int i;

	// loops through the entire current track
	for(i = 0; i<FS3_TRACK_SIZE; i++){
		// if a sector has no file data in it, it returns that sector number
		if(FS3DiskMap[currentDiskTrack][i] == -1){
			return(i);
		}
	}

	return(-1);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : find_current_track
// Description  : Finds the track number in which the position of the file is on
//
// Inputs 		: fd - the file descriptor
// Outputs      : Current track number, or -1 if failure to find

int find_current_track(int16_t fd){
	// sees what part of the file the position is in
	int partNum = (int) (FS3FileArray[fd].position/FS3_SECTOR_SIZE);
	
	int i;
	int j;
	int count = -1;

	// loops through the entire disk map
	for(i = 0; i<FS3_MAX_TRACKS; i++){
		for(j = 0; j<FS3_TRACK_SIZE; j++){
			// if a match is found, it increases the count
			if(FS3DiskMap[i][j] == fd){
				count = count + 1;

				// if the count equals the partNum, it returns the track number
				if(count == partNum){
					return(i);
				}
			}

			// if it reaches the point where there are no more files, it ends the function
			if(FS3DiskMap[i][j] == -1){
				return(-1);
			}
		}
	}

	return(-1);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : find_current_sector
// Description  : Finds the sector number in which the position of the file is on
//
// Inputs		: fd - the file descriptor
// Outputs      : Current sector number, or -1 if failure to find

int find_current_sector(int16_t fd){
	// sees what part of the file the position is in
	int partNum = (int) (FS3FileArray[fd].position/FS3_SECTOR_SIZE);
	
	int i;
	int j;
	int count = -1;

	// loops through the entire disk map
	for(i = 0; i<FS3_MAX_TRACKS; i++){
		for(j = 0; j<FS3_TRACK_SIZE; j++){
			// if a match is found, it increases the count
			if(FS3DiskMap[i][j] == fd){
				count = count + 1;
				
				// if the count equals the partNum, it returns the sector number
				if(count == partNum){
					return(j);
				}
			}

			// if it reaches the point where there are no more files, it ends the function
			if(FS3DiskMap[i][j] == -1){
				return(-1);
			}
		}
	}

	return(-1);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : construct_fs3_cmdblock
// Description  : Create an FS3 array opcode from the variable fields
//
// Inputs       : op - 8 bit unsigned integer containing the opcode command
//				  sec - 16 bit unsigned integer containing the sector number
// 				  trk - 32 bit unsigned integer containing the track number
//				  ret - 8 bit unsigned integer containing the return value
// Outputs      : The constructed FS3CmdBlk

FS3CmdBlk construct_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret){
	FS3CmdBlk result = (FS3CmdBlk) 0;

	// converts op to 64 bit and shifts the opcode to bits 0-3
	uint64_t opBits = (uint64_t) op;	
	opBits = (FS3CmdBlk) opBits << 60;

	// converts sec to 64 bit and shifts the sector number to bits 4-19
	uint64_t secBits = (uint64_t) sec;	
	secBits = (FS3CmdBlk) secBits << 44;

	// converts trk to 64 bit and shifts the track number to bits 20-51
	uint64_t trkBits = (uint64_t) trk;	
	trkBits = (FS3CmdBlk) trkBits << 12;

	// converts ret to 64 bit and shifts the return value to bit 52
	uint64_t retBits = (uint64_t) ret;	
	retBits = (FS3CmdBlk) retBits << 60;

	// combines all bits into the resulting command block
	result = opBits | secBits | trkBits | retBits;		
	return(result);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : deconstruct_fs3_cmdblock
// Description  : Extract register state from bus values
//
// Inputs       : cmdblock - the command block to get deconstructed
//				  *op - pointer to where the opcode bits will be written to
//				  *sec - pointer to where the sector number bits will be written to
//				  *trk - pointer to where the track number bits will be written to
//				  *ret - pointer to where the return value bits will be written to
// Outputs      : 0 if successful, -1 if failure

int deconstruct_fs3_cmdblock(FS3CmdBlk cmdblock, uint8_t *op, uint16_t *sec, uint32_t *trk, uint8_t *ret) {
	// uses a mask to extract the opcode bits from the command block
	uint64_t opBits = cmdblock & create64BitMask(0,3);		
	opBits = opBits >> 60;
	// writes the opcode bits to the pointer
	*op = (uint8_t) opBits;		

	// uses a mask to extract the sector number bits
	uint64_t secBits = cmdblock & create64BitMask(4,19);	
	secBits = secBits >> 44;
	// writes the sector number to the pointer
	*sec = (uint16_t) secBits;	

	// uses a mask to extract the track number bits
	uint64_t trkBits = cmdblock & create64BitMask(20,51);	
	trkBits = trkBits >> 12;
	// writes the track number to the pointer
	*trk = (uint32_t) trkBits;	

	// uses a mask to extract the return number bit
	uint64_t retBits = cmdblock & create64BitMask(52,52);	
	retBits = retBits >> 11;
	// writes the return value to the pointer
	*ret = (uint8_t) retBits;	

	return(0);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : create64BitMask
// Description  : Creates a 64 bit mask
//
// Inputs       : startPos - starting position of mask
//				  endPos - ending position of mask
// Outputs      : Will return created Bit Mask, or mask of all zeros if unsuccessful

uint64_t create64BitMask(int startPos, int endPos){
	uint64_t result = 0;
	int i;

	// checks to make sure the start and end positions are valid, and returns a 0 mask if not
	if ((startPos < 0) || (endPos < 0) || (startPos > endPos) || (endPos >= 64)){
		return(result);
	}

	//	fills in 1's from the start position to the end position
	for (i = startPos; i<=endPos; i++){
		result = result | ((uint64_t) 1 << (63-i));
	}

	return(result);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : getReturnBit
// Description  : Retrieves the return bit value of an FS3 Command Block
//
// Inputs       : cmdblock - the command block to retrieve the return bit value from
// Outputs      : return bit value of cmdblock

uint8_t getReturnBit(FS3CmdBlk cmdblock){
	uint8_t returnBit;
	uint8_t opcodeBits; 
	uint16_t sectorBits; 
	uint32_t trackBits;

	// sends pointers of each variable to the deconstruct funcition
	deconstruct_fs3_cmdblock(cmdblock, &opcodeBits, &sectorBits, &trackBits, &returnBit);

	// returns the value of the return bit
	return(returnBit);
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : getOpCodeBits
// Description  : Retrieves the op code bits value of an FS3 Command Block
//
// Inputs       : cmdblock - the command block to retrieve the return bit value from
// Outputs      : op code bits value of cmdblock

uint8_t getOpCodeBits(FS3CmdBlk cmdblock){
	uint8_t returnBit;
	uint8_t opcodeBits; 
	uint16_t sectorBits; 
	uint32_t trackBits;

	// sends pointers of each variable to the deconstruct funcition
	deconstruct_fs3_cmdblock(cmdblock, &opcodeBits, &sectorBits, &trackBits, &returnBit);

	// returns the value of the return bit
	return(opcodeBits);
}
