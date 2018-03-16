/******************************************************************************
 * FILE NAME:  ewfs.c
 *
 * FILE DESCRIPTION:
 * Implementation of the Electronic Wilderness File System which is coded
 * in a similar way to the Microchip File System (MPFS).
 *
 * FILE NOTES:  None.
 *
 * LICENSE:
 * Copyright 2018 Eric Roman/ElectronicWilderness
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************/

/******************************************************************************
 *                              FILE INCLUDES
 *****************************************************************************/
#include "ewfs.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "system/command/sys_command.h"
#include "custom_file_app.h"

/******************************************************************************
 *                          DEFINITIONS
 *****************************************************************************/
#define EWFS_HANDLE_TOKEN_MAX (0xFF)
#define EWFS_MAKE_HANDLE(token, disk, index) (((token) << 24) | ((disk) << 16) | (index))
#define EWFS_UPDATE_HANDLE_TOKEN(token) { \
    (token)++; \
    (token) = ((token) == EWFS_HANDLE_TOKEN_MAX) ? 0: (token); \
}

/******************************************************************************
 *                              TYPE DEFINES
 *****************************************************************************/
typedef enum __attribute__((packed,aligned(1))){
    TYPE_GENERATED = 0,
	TYPE_FILE = 1
}file_type_e;

//EWFS header structure
typedef struct{
    uint8_t disk_num;
    uint8_t version;
    uint16_t file_count;
    uint32_t base_address;
    uint32_t file_start_address;
    bool cachable_index;
}ewfs_header_t;

//EWFS fiile index item structure
typedef struct __attribute__((packed,aligned(1))){
    uint16_t hash;
    file_type_e type;
    uint32_t offset;
    uint32_t length;
}ewfs_index_t;

//EWFS opened file structure
typedef struct{
    uint32_t current_position;  //current position in file
    uint32_t bytes_remaining;   //bytes remaining to send
    uint32_t size;          	//size of file
    uint32_t handle;        	//handle to file
    uint16_t gen_hash;          //hash of the file - used by generated files
    uint16_t gen_index;         //index of generated file - used by generated files
    uint32_t gen_offset;    	//offset if not all the data was sent - otherwise 0 bytes
    file_type_e type;       	//type of file (generated or file)
}ewfs_file_obj_t;

static ewfs_header_t ewfs_header = {0xff, 0, 0, 0,true};

static ewfs_index_t *ewfs_index;

static ewfs_file_obj_t ewfs_file_obj[SYS_FS_MAX_FILES];
uint8_t ewfs_handle_token = 0;

const SYS_FS_FUNCTIONS EWFSFunctions = {
    .mount  = EWFS_Mount,
    .unmount = EWFS_Unmount,
    .open   = EWFS_Open,
    .read   = EWFS_Read,
    .write  = NULL,
    .close  = EWFS_Close,
    .seek   = EWFS_Seek,
    .tell   = EWFS_GetPosition,
    .eof    = NULL,
    .size   = EWFS_GetSize,
    .fstat   = NULL,
    .mkdir = NULL,
    .chdir = NULL,
    .remove = NULL,
    .getlabel = NULL,
    .setlabel = NULL,
    .truncate = NULL,
    .currWD = NULL,
    .chdrive = NULL,
    .chmode = NULL,
    .chtime = NULL,
    .rename = NULL,
    .sync = NULL,
    .getstrn = NULL,
    .putchr = NULL,
    .putstrn = NULL,
    .formattedprint = NULL,
    .testerror = NULL,
    .formatDisk = NULL,
    .openDir = NULL,
    .readDir = NULL,
    .closeDir = NULL,
    .partitionDisk = NULL,
    .getCluster = NULL
};

/******************************************************************************
 *                          FUNCTION PROTOTYPES
 *****************************************************************************/
static bool EWFSGetArray(uint8_t diskNum, uint32_t address, uint32_t length, uint8_t *buffer);
static bool EWFSDiskRead(uint16_t diskNum, uint8_t *destination, uint8_t *source, const uint32_t nBytes);
static int EWFSFindFile(uint8_t disk_num, uint8_t *file);
static bool EWFSIsHandleValid(uint32_t handle);

/******************************************************************************
* Function: Soft delay functions 
******************************************************************************/
inline static uint32_t _APP_SQI_ReadCoreTimer()
{
    volatile uint32_t timer;

    // get the readSize reg
    asm volatile("mfc0   %0, $9" : "=r"(timer));

    return(timer);
}

inline static void _APP_SQI_StartCoreTimer(uint32_t period)
{
    /* Reset the coutner */
    volatile uint32_t loadZero = 0;

    asm volatile("mtc0   %0, $9" : "+r"(loadZero));
    asm volatile("mtc0   %0, $11" : "+r" (period));
}

inline void _APP_SQI_CoreTimer_Delay(uint32_t delayValue)
{
    while ((_APP_SQI_ReadCoreTimer() <= delayValue))
    asm("nop");
}

/******************************************************************************
 * FUNCTION:  EWFS_Mount
 * 
 * DESCRIPTION:
 * This function mounts the EWFS file system.
 * 
 * PARAMETERS:
 * disk_num		uint8_t     disk volume number
 * 
 * RETURN VALUE:
 * int 		returns EWFS_OK if successful, otherwise EWFS_DISK_ERR
 * 
 * NOTES:
 * 
******************************************************************************/
int EWFS_Mount (uint8_t disk_num){
    uint32_t index = 0;
    uint8_t ewfs_fs_start[4];
    static volatile int k = 0;
    
    //leaving the next line in allows for mounting to work
    SYS_CONSOLE_PRINT("disk num: %i\r\n", disk_num);
    if (disk_num > SYS_FS_VOLUME_NUMBER){
        return EWFS_DISK_ERR;
    }
    //check if the mount operation has already been done
    if (ewfs_header.disk_num != 0xff){
        return EWFS_OK;
    }
    ewfs_header.file_count = 0;
    //find the base address of the EWFS image
    ewfs_header.base_address = SYS_FS_MEDIA_MANAGER_AddressGet(disk_num);
    for (index = 0; index < SYS_FS_MAX_FILES; index ++){
        ewfs_file_obj[index].current_position = EWFS_INVALID;
        ewfs_file_obj[index].bytes_remaining = 0;
        ewfs_file_obj[index].size = 0;
    }
    
    //read the EWFS image header
    if (EWFSGetArray(disk_num, 0, 4, (uint8_t *) ewfs_fs_start) == false){
        return EWFS_DISK_ERR;
    }
    //SYS_CONSOLE_PRINT("disk num: %i\r\n", disk_num);
    if (memcmp(ewfs_fs_start, (const void *) "EWFS", 4) != 0){
        ewfs_header.version=0;
        ewfs_header.file_count =0;
        ewfs_header.cachable_index = true;
        ewfs_header.file_start_address = 7 + 0;
        ewfs_header.disk_num = disk_num;
        return EWFS_OK;
    }
    _APP_SQI_StartCoreTimer(0);
    _APP_SQI_CoreTimer_Delay(100000);  //1ms
    //read version of EWFS
    if (EWFSGetArray(disk_num, 4, 1, (uint8_t *) &ewfs_header.version) == false){
        return EWFS_DISK_ERR;
    }
    _APP_SQI_StartCoreTimer(0);
    _APP_SQI_CoreTimer_Delay(100000);  //1ms
    
    //read number of files
    if (EWFSGetArray(disk_num, 5, 2, (uint8_t *) &ewfs_header.file_count) == false){
        return EWFS_DISK_ERR;
    }
    if (ewfs_header.file_count == 0){
        ewfs_header.cachable_index = true;
        //file_index_byte_count = 0;
        ewfs_header.file_start_address = 7 + 0;
        ewfs_header.disk_num = disk_num;
        return EWFS_OK;
        //return EWFS_DISK_ERR;
    }
    if (ewfs_index != NULL){
        free (ewfs_index);  //free the memory before reallocating
    }
    //force cachable file system index
    ewfs_header.cachable_index = true;
    //file_index_byte_count = sizeof(ewfs_index_t) * ewfs_header.file_count;
    ewfs_header.file_start_address = 7 + (sizeof(ewfs_index_t) * ewfs_header.file_count);     //get start of file data
    SYS_CONSOLE_PRINT("file start address: %i\r\n", ewfs_header.file_start_address);
    _APP_SQI_StartCoreTimer(0);
    _APP_SQI_CoreTimer_Delay(100000);  //1ms
    if (ewfs_header.cachable_index){
        if (ewfs_index != NULL){    //free space if allocated
            free(ewfs_index);
        }
        //allocate memory for file index
        ewfs_index = malloc(sizeof(ewfs_index_t) * ewfs_header.file_count);
        //read the file index
        if (EWFSGetArray(disk_num, 7, (sizeof(ewfs_index_t) * ewfs_header.file_count), (uint8_t *) ewfs_index) == false){
            return EWFS_DISK_ERR;
        }
        //print the file index to the console
        SYS_CONSOLE_PRINT("hash\tlength\t\toffset=>total offset\ttype\r\n");
        _APP_SQI_StartCoreTimer(0);
        _APP_SQI_CoreTimer_Delay(100000);  //1ms
        for (index = 0; index < ewfs_header.file_count; index ++){
            _APP_SQI_StartCoreTimer(0);
            _APP_SQI_CoreTimer_Delay(100000);  //1ms
            SYS_CONSOLE_PRINT("%04X\t%08X\t%08X=>%08X\t%i\r\n",
                    ewfs_index[index].hash,
                    ewfs_index[index].length,
                    ewfs_index[index].offset,
                    ewfs_index[index].offset + ewfs_header.file_start_address,
                    ewfs_index[index].type);
        }
    }
    //store the disk number
    ewfs_header.disk_num = disk_num;
    //initialize the user custom file generation
    InitGeneratedFiles();
    
    return EWFS_OK;
}

/******************************************************************************
 * FUNCTION:  EWFSGetArray
 * 
 * DESCRIPTION:
 * This function basically calls EWFSDiskRead and adds in the base_address to
 * the address.
 * 
 * PARAMETERS:
 * diskNum 		uint8_t		disk number
 * address 		uint32_t	address to start reading from
 * length 		uint32_t	number of bytes to read
 * buffer 		uint8_t *	pointer to the buffer to read the data to
 * 
 * RETURN VALUE:
 * bool		returns result from EWFSDiskRead()
 * 
 * NOTES:
 * 
******************************************************************************/
static bool EWFSGetArray(uint8_t diskNum, uint32_t address, uint32_t length, uint8_t *buffer){
    return EWFSDiskRead (diskNum, buffer, ((uint8_t *)ewfs_header.base_address + address), length);
}

/******************************************************************************
 * FUNCTION:  EWFSDiskRead
 * 
 * DESCRIPTION:
 * This function calles the media manager to read the data from memory.  The
 * reading is a blocking operation.
 * 
 * PARAMETERS:
 * diskNum 		uint8_t		disk number
 * destination	uint8_t *	pointer to the buffer to read the data to
 * source 		uint8_t *	the starting address of data to read
 * nBytes 		uint32_t	the number of bytes to read
 * 
 * RETURN VALUE:
 * int 		returns true if successful, otherwise false
 * 
 * NOTES:
 * 
******************************************************************************/
SYS_FS_MEDIA_COMMAND_STATUS __attribute__ ((coherent,aligned (16))) commandStatus = SYS_FS_MEDIA_COMMAND_UNKNOWN;
static bool EWFSDiskRead(uint16_t diskNum, uint8_t *destination, uint8_t *source, const uint32_t nBytes){
    SYS_FS_MEDIA_BLOCK_COMMAND_HANDLE commandHandle  = SYS_FS_MEDIA_BLOCK_COMMAND_HANDLE_INVALID;
    
    commandHandle = SYS_FS_MEDIA_BLOCK_COMMAND_HANDLE_INVALID;
    commandHandle = SYS_FS_MEDIA_MANAGER_Read (
            diskNum, 
            destination, 
            source, 
            nBytes);
    if (commandHandle == SYS_FS_MEDIA_BLOCK_COMMAND_HANDLE_INVALID){
        return false;
    }

    _APP_SQI_StartCoreTimer(0);
    _APP_SQI_CoreTimer_Delay(700000);  //7ms
   
    do {
        SYS_FS_MEDIA_MANAGER_TransferTask (diskNum);
        commandStatus = SYS_FS_MEDIA_MANAGER_CommandStatusGet(diskNum, commandHandle);
    } while ((commandStatus == SYS_FS_MEDIA_COMMAND_QUEUED || commandStatus == SYS_FS_MEDIA_COMMAND_IN_PROGRESS));
    
    _APP_SQI_StartCoreTimer(0);
    _APP_SQI_CoreTimer_Delay(200000);  //2ms
   
    if (commandStatus == SYS_FS_MEDIA_COMMAND_COMPLETED){
        return true;
    }else{
        return false;
    }
}

/******************************************************************************
 * FUNCTION:  EWFS_Unmount
 * 
 * DESCRIPTION:
 * Unmount the file system by freeing memory and resetting file count and disk
 * number.
 * 
 * PARAMETERS:
 * disk_num 		uint8_t		disk number
 * 
 * RETURN VALUE:
 * int 		returns EWFS_OK if successful.
 * 
 * NOTES:
 * 
******************************************************************************/
int EWFS_Unmount(uint8_t disk_num){
    if ((disk_num > SYS_FS_VOLUME_NUMBER) || (disk_num != ewfs_header.disk_num )){
        return EWFS_DISK_ERR;
    }
    ewfs_header.file_count = 0;
    ewfs_header.disk_num = EWFS_INVALID_HANDLE;
    free(ewfs_index);
    
    return EWFS_OK;
}

/******************************************************************************
 * FUNCTION:  EWFS_Open
 * 
 * DESCRIPTION:
 * Open the file by finding a free file object then searching for the file.
 * Update the file object properties with the file data.  If the file is 
 * generated, set the object properties with data through the generation
 * process.
 * 
 * PARAMETERS:
 * handle 			uintptr_t	pointer to file system file handle
 * filewithDisk		char *		full file path
 * mode				uint8_t		mode attribute of how to open file
 * 
 * RETURN VALUE:
 * int 		returns EWFS_OK if successful.
 * 
 * NOTES:
 * 
******************************************************************************/
int EWFS_Open(uintptr_t handle, const char *filewithDisk, uint8_t mode){
    volatile uint32_t index = 0;
    volatile uint32_t found_file;
    uint8_t disk_num = 0;
    
    disk_num = filewithDisk[0] - '0';
    
    if ((disk_num > SYS_FS_VOLUME_NUMBER) || (disk_num != ewfs_header.disk_num)){
        return EWFS_INVALID_PARAMETER;
    }
    //find a free file object
    for (index = 0; index < SYS_FS_MAX_FILES; index ++){
        if (ewfs_file_obj[index].current_position == EWFS_INVALID){
            break;
        }
    }
    if (index > SYS_FS_MAX_FILES){  //check if the index is valid
        return EWFS_INVALID_PARAMETER;
    }
    found_file = EWFSFindFile(disk_num, (uint8_t *) (filewithDisk + 3));
    if (found_file >= 0){
        ewfs_file_obj[index].bytes_remaining = ewfs_index[found_file].length - 1;   //-1 because file size includes 0 at end of file
        ewfs_file_obj[index].current_position = ewfs_index[found_file].offset + ewfs_header.file_start_address;
        ewfs_file_obj[index].size= ewfs_file_obj[index].bytes_remaining;
        ewfs_file_obj[index].type = ewfs_index[found_file].type;
        //update handles
        ewfs_file_obj[index].handle = EWFS_MAKE_HANDLE(ewfs_handle_token, disk_num, index);
        EWFS_UPDATE_HANDLE_TOKEN(ewfs_handle_token);
        *(uintptr_t *) handle = ewfs_file_obj[index].handle;
        if (ewfs_file_obj[index].type == TYPE_GENERATED){
            ewfs_file_obj[index].gen_hash = ewfs_index[found_file].hash;
            ewfs_file_obj[index].gen_index = 0; //starting at first index
            ewfs_file_obj[index].size = GenerateFileSize(ewfs_index[found_file].hash); 
            ewfs_file_obj[index].bytes_remaining = ewfs_file_obj[index].size;
            ewfs_file_obj[index].current_position = 0;
            ewfs_file_obj[index].gen_offset = 0;
            /*SYS_CONSOLE_PRINT("***OPEN hash: %04X\ttype: generated\tname: %s\tlength: %X\toffset: %X***\r\n",
                    ewfs_index[found_file].hash,
                    (filewithDisk + 3),
                    ewfs_file_obj[index].bytes_remaining,
                    ewfs_file_obj[index].current_position);*/
        }else{  // file type = file
            /*SYS_CONSOLE_PRINT("***OPEN hash: %04X\ttype: file\tname: %s\tlength: %X\toffset: %X***\r\n",
                    ewfs_index[found_file].hash,
                    (filewithDisk + 3),
                    ewfs_file_obj[index].bytes_remaining,
                    ewfs_file_obj[index].current_position);*/
        }
        return EWFS_OK;        
    }
    return EWFS_NO_FILE;
}

/******************************************************************************
 * FUNCTION:  EWFSFindFile
 * 
 * DESCRIPTION:
 * This function searches the file system index for the file requested and 
 * returns the index associated with the file as well as updating the file 
 * record with the file information.
 * 
 * PARAMETERS:
 * diskNum 		uint8_t		disk number
 * destination	uint8_t *	pointer to the buffer to read the data to
 * source 		uint8_t *	the starting address of data to read
 * nBytes 		uint32_t	the number of bytes to read
 * 
 * RETURN VALUE:
 * int 		returns true if successful, otherwise false
 * 
 * NOTES:
 * Current implementation assumes that the file system index is cachable,
 * otherwise additional file system reading will need to happen.
 * 
******************************************************************************/
static int EWFSFindFile(uint8_t disk_num, uint8_t *file){
    uint8_t *ptr;
    volatile uint16_t hash = 0;
    volatile uint32_t index = 0;
    
    //calculate the hash of the file name
    ptr = file;
    while (*ptr != '\0'){
        hash <<=1;
        hash += *ptr ++;
    }
    if (!ewfs_header.cachable_index){
        return -1;
    }
    for (index = 0; index < ewfs_header.file_count; index ++){
        if (ewfs_index[index].hash == hash){
            return index;
        }
    }
    return -1;
}

/******************************************************************************
 * FUNCTION NAME:  EWFS_Read
 *
 * FUNCTION DESCRIPTION:
 * Read the file data from flash if it is a file type.  If it is a generated
 * file, call the GenerateFileRead function to generate file data.
 *
 * FUNCTION PARAMETERS:
 * handle   uintptr_t   The handle to the file that will be read.
 * buffer   void *      Pointer to the buffer where the data that is read is 
 *                      stored to for calling function to use.
 * btr      uint32_t    The size of data to put in the buffer. Bytes To Read
 * br       uint32_t *  A pointer to the number of bytes read from the file. 
 *                      Bytes Read
 *
 * FUNCTION RETURN VALUE:
 * EWFS_OK  The file was read.
 *
 * FUNCTION NOTES:  None.
 *
 *****************************************************************************/
int EWFS_Read(uintptr_t handle, void* buffer, uint32_t btr, uint32_t *br){
    uint16_t index = 0;
    uint8_t disk_num = 0;
    
    *br = 0;
    index = handle & 0xFFFF;
    if (index > SYS_FS_MAX_FILES){
        return EWFS_INVALID_PARAMETER;
    }
    //extract the disk number from the handle
    disk_num = ((handle >> 16) * 0xFF);
    //find the number of bytes to read is greater then the number of remaining bytes
    if (btr > ewfs_file_obj[index].bytes_remaining){
        btr = ewfs_file_obj[index].bytes_remaining;
    }
    //check that the buffer needs data
    if (btr > 0){
        /*SYS_CONSOLE_PRINT("***READ current position: %X\tbytes remaining: %X***\r\n", 
                ewfs_file_obj[index].current_position,
                ewfs_file_obj[index].bytes_remaining);
        _APP_SQI_StartCoreTimer(0);
        _APP_SQI_CoreTimer_Delay(500000);  //5ms */
        if (ewfs_file_obj[index].type == TYPE_GENERATED){    //check if the file is generated
            GenerateFileRead(ewfs_file_obj[index].gen_hash, buffer, btr, br,
                    &ewfs_file_obj[index].gen_index, &ewfs_file_obj[index].gen_offset);
            ewfs_file_obj[index].current_position += *br;
            ewfs_file_obj[index].bytes_remaining -= *br;
        }else{  //else its a file
            if (EWFSGetArray(disk_num, ewfs_file_obj[index].current_position, btr, buffer) == true){
                *br = btr;
                //update the current address offset and the bytes remaining offset
                ewfs_file_obj[index].current_position += *br;
                ewfs_file_obj[index].bytes_remaining -= *br;
            }
        }
        /*SYS_CONSOLE_PRINT("***READ current position: %X\tbytes remaining: %X***\r\n", 
                ewfs_file_obj[index].current_position,
                ewfs_file_obj[index].bytes_remaining);
        _APP_SQI_StartCoreTimer(0);
        _APP_SQI_CoreTimer_Delay(500000);  //5ms */
    }
    return EWFS_OK;
}

/******************************************************************************
 * FUNCTION:  EWFS_Close
 * 
 * DESCRIPTION:
 * Close the file and free the file pointer handle.
 * 
 * PARAMETERS:
 * handle 	uintptr_t	pointer to file handle
 * 
 * RETURN VALUE:
 * int 		returns EWFS_OK if successful, otherwise EWFS_INVALID_PARAMETER
 * 
 * NOTES:
 * 
******************************************************************************/
int EWFS_Close(uintptr_t handle){
    uint16_t index = 0;
    
    index = handle & 0xFFFF;
    if (index > SYS_FS_MAX_FILES){
        return EWFS_INVALID_PARAMETER;
    }
    ewfs_file_obj[index].handle = EWFS_INVALID_HANDLE;
    ewfs_file_obj[index].current_position = EWFS_INVALID;
    ewfs_file_obj[index].bytes_remaining = 0;
    ewfs_file_obj[index].size = 0;
    ewfs_file_obj[index].gen_hash = 0;
    ewfs_file_obj[index].gen_index = 0;
    ewfs_file_obj[index].gen_offset = 0;
    /*SYS_CONSOLE_PRINT("CLOSE\r\n");*/
    return EWFS_OK;
    
}

/******************************************************************************
 * FUNCTION:  EWFS_GetSize
 * 
 * DESCRIPTION:
 * Return the size of the file associated with the file handle.
 * 
 * PARAMETERS:
 * handle 	uintptr_t	pointer to file handle
 * 
 * RETURN VALUE:
 * int 		returns the size of the file, 0 if not valid
 * 
 * NOTES:
 * 
******************************************************************************/
uint32_t EWFS_GetSize(uintptr_t handle){
    uint16_t index = 0;
    
    if (EWFSIsHandleValid(handle) == false){
        return 0;   //invalid handle
    }
    index = handle & 0xFFFF;
    return ewfs_file_obj[index].size;
}

/******************************************************************************
 * FUNCTION:  EWFSIsHandleValid
 * 
 * DESCRIPTION:
 * Determine if the file handle is valid.
 * 
 * PARAMETERS:
 * handle 	uint32_t	file handle
 * 
 * RETURN VALUE:
 * bool		true if handle is valid, otherwise false
 * 
 * NOTES:
 * 
******************************************************************************/
static bool EWFSIsHandleValid(uint32_t handle){
    uint16_t index = handle & 0xFFFF;
    if (index > SYS_FS_MAX_FILES){
        return false;
    }
    if (ewfs_file_obj[index].handle != handle){
        return false;
    }
    return true;
}

/******************************************************************************
 * FUNCTION:  EWFS_GetPosition
 * 
 * DESCRIPTION:
 * This function returns the current position within the file being read.
 * 
 * PARAMETERS:
 * handle 		uintptr_t	file handle
 * 
 * RETURN VALUE:
 * uint32_t		current position within the file
 * 
 * NOTES:
 * 
******************************************************************************/
uint32_t EWFS_GetPosition(uintptr_t handle){
    uint16_t index;
    if (EWFSIsHandleValid(handle) == false){
        return 0;   //invalid handle
    }
    
    index = handle & 0xFFFF;
    return (uint32_t) (ewfs_file_obj[index].current_position);
}

/******************************************************************************
 * FUNCTION:  EWFS_Seek
 * 
 * DESCRIPTION:
 * Update the file object position for reading to allow for adjustments of 
 * file reading position.
 * 
 * PARAMETERS:
 * handle 		uintptr_t		file handle
 * dwOffset		uint32_t		offset to current file position to apply
 * 
 * RETURN VALUE:
 * int 		returns 0 if successful, otherwise 1
 * 
 * NOTES:
 * 
******************************************************************************/
//returns 0 when successful, otherwise 1.
int EWFS_Seek(uintptr_t handle, uint32_t dwOffset){
    uint16_t index = 0;
    if (EWFSIsHandleValid(handle) == false){
        return 1;   //invalid handle
    }
    index = handle & 0xFFFF;
    if (labs(dwOffset) > ewfs_file_obj[index].size){
        return 1;
    }
    if (ewfs_file_obj[index].type == TYPE_GENERATED){    //check if the file is generated
        ewfs_file_obj[index].gen_index--;
        ewfs_file_obj[index].gen_offset = -dwOffset;
        ewfs_file_obj[index].current_position = ewfs_file_obj[index].current_position + dwOffset;
        ewfs_file_obj[index].bytes_remaining = ewfs_file_obj[index].bytes_remaining - dwOffset;
    }else{
        ewfs_file_obj[index].current_position = ewfs_file_obj[index].current_position + dwOffset;
        ewfs_file_obj[index].bytes_remaining = ewfs_file_obj[index].bytes_remaining - dwOffset;
    }

    return 0;
}

