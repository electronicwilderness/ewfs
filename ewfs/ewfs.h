/******************************************************************************
 * FILE NAME:  ewfs.h
 *
 * FILE DESCRIPTION:
 * This is the header file for the Electronic Wilderness File System.
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
#ifndef _EWFS_H    /* Guard against multiple inclusion */
#define _EWFS_H

/******************************************************************************
 *                              FILE INCLUDES
 *****************************************************************************/
#include "system_config.h"
#include "system/fs/sys_fs.h"
#include "system/fs/sys_fs_media_manager.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/******************************************************************************
 *                          DEFINITIONS
 *****************************************************************************/
#define EWFS_INVALID            (0xffffffffu)
#define EWFS_INVALID_HANDLE     0xff

/******************************************************************************
 *                              TYPE DEFINES
 *****************************************************************************/
typedef enum{
    EWFS_OK = 0,    //success
    EWFS_DISK_ERR,  //a hard error occurred in the low level disk I/O layer
    EWFS_NO_FILE,   //could not find the file   
    EWFS_INVALID_PARAMETER  //given parameter is invalid
}ewfs_result_e;

extern const SYS_FS_FUNCTIONS EWFSFunctions;

/******************************************************************************
 *                          FUNCTION PROTOTYPES
 *****************************************************************************/
int EWFS_Mount (uint8_t disk_num);
int EWFS_Unmount(uint8_t disk_num);
int EWFS_Open(uintptr_t handle, const char * filewithDisk, uint8_t mode);
int EWFS_Read(uintptr_t handle, void* buffer, uint32_t btr, uint32_t *br);
int EWFS_Close(uintptr_t handle);
uint32_t EWFS_GetSize(uintptr_t handle);
uint32_t EWFS_GetPosition(uintptr_t handle);
int EWFS_Seek(uintptr_t handle, uint32_t dwOffset);

#endif /* _EWFS_H */
