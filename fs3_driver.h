#ifndef FS3_DRIVER_INCLUDED
#define FS3_DRIVER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.h
//  Description    : This is the header file for the standardized IO functions
//                   for used to access the FS3 storage system.
//
//  Author         : Christopher Kurcz
//  Last Modified  : 11/19/2021
//

// Include files
#include <stdint.h>
#include <fs3_controller.h>
#include <fs3_common.h>

// Defines
#define FS3_MAX_TOTAL_FILES 1024 // Maximum number of files ever
#define FS3_MAX_PATH_LENGTH 128 // Maximum length of filename length

// Type Definitions
	// simple boolean enum
	typedef enum { 
		false = 0, 
		true = 1 
	} bool;

	// struct for keeping track of a file and its metadata
	typedef struct {
		bool created;
		bool open;
		char name[FS3_MAX_PATH_LENGTH];
		int length;
		int position;
	} FS3File;

// Interface functions

int32_t fs3_mount_disk(void);
	// FS3 interface, mount/initialize filesystem

int32_t fs3_unmount_disk(void);
	// FS3 interface, unmount the disk, close all files

int16_t fs3_open(char *path);
	// This function opens a file and returns a file handle

int16_t fs3_close(int16_t fd);
	// This function closes a file

int32_t fs3_read(int16_t fd, void *buf, int32_t count);
	// Reads "count" bytes from the file handle "fh" into the buffer  "buf"

int32_t fs3_write(int16_t fd, void *buf, int32_t count);
	// Writes "count" bytes to the file handle "fh" from the buffer  "buf"

int32_t fs3_seek(int16_t fd, uint32_t loc);
	// Seek to specific point in the file

int switch_disk_track(int trackNum);
	// Switches the track the disk in on to a new track

int find_open_track();
	// Finds a track in which there is a sector with no data in it

int find_open_sector();
	// Finds a sector with no data in it on the current track

int find_current_track(int16_t fd);
	// Finds the track number in which the position of the file is on

int find_current_sector(int16_t fd);
	// Finds the sector number in which the position of the file is on

FS3CmdBlk construct_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret);
	// Create an FS3 array opcode from the variable fields

int deconstruct_fs3_cmdblock(FS3CmdBlk cmdblock, uint8_t *op, uint16_t *sec, uint32_t *trk, uint8_t *ret);
	// Extract register state from bus values

uint64_t create64BitMask(int startPos, int endPos);
	// Creates a 64 bit mask

uint8_t getReturnBit(FS3CmdBlk cmdblock);
	// Retrieves the return bit value of an FS3 Command Block

uint8_t getOpCodeBits(FS3CmdBlk cmdblock);
	// Retrieves the op code bits value of an FS3 Command Block

#endif