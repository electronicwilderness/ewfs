/******************************************************************************
 * FILE NAME:  custom_file_app.c
 *
 * FILE DESCRIPTION:
 * Implementation to generate files from the EWFS file system.
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
#include "custom_file_app.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/******************************************************************************
 *                          DEFINITIONS
 *****************************************************************************/
#define FILE_LIST_COUNT     2

/******************************************************************************
 *                              TYPE DEFINES
 *****************************************************************************/
typedef struct{
    uint8_t *file_name;
    uint8_t file_name_length;
    uint16_t hash;
}gen_file_list_t;

//the user must fill in this list with the file name and size of file name and 0
gen_file_list_t my_file_list[FILE_LIST_COUNT] = {
    {"me.json",7,0},
    {"largefile.json",14,0}
};

/******************************************************************************
 *                          FUNCTION PROTOTYPES
 *****************************************************************************/
uint32_t GenerateLargeFileJson(uint16_t *index, uint32_t max_size, uint8_t *buffer);

/******************************************************************************
 * FUNCTION:  InitGeneratedFiles
 * 
 * DESCRIPTION:
 * Initialize the generated file list (my_file_list[]) array with additional 
 * data.  
 * 
 * PARAMETERS:
 * none   
 * 
 * RETURN VALUE:
 * none
 * 
 * NOTES:
 * The user must setup the FILE_LIST_COUNT with correct number of files.  The
 * varialbe my_file_list also needs to be updated with the file names.
 * 
******************************************************************************/
void InitGeneratedFiles(){
    uint8_t *ptr;
    uint16_t count = 0;
    uint16_t hash = 0;
    uint16_t size = 0;
    
    //calculate the hashes for the files that can be generated
    for (count = 0; count < FILE_LIST_COUNT; count ++){
        ptr = my_file_list[count].file_name;
        hash = 0;
        for (size = 0; size < my_file_list[count].file_name_length; size ++){
            hash <<=1;
            hash += *ptr ++;
        }
        my_file_list[count].hash = hash;
    }
}

/******************************************************************************
 * FUNCTION:  GenerateFileRead
 *
 * DESCRIPTION:
 * This function determines which file is being read and generates the data.
 *
 * PARAMETERS:
 * hash             uint16_t    hash of the file name
 * buffer           uint8_t *   pointer to the buffer
 * buffer_size      uint32_t    the maximum size of data that can be put into 
 *                              the buffer
 * num_bytes_read   uint32_t *  the number of bytes put into the buffer
 * index            uint16_t *  The index of the file generation process
 * offset           uint32_t    offset to the number of bytes to read
 *
 * RETURN VALUE:  None.
 *
 * NOTES:  None.
 *
 *****************************************************************************/
void GenerateFileRead(uint16_t hash, uint8_t *buffer, uint32_t buffer_size, 
        uint32_t *num_bytes_read, uint16_t *index, uint32_t *offset){
    volatile int16_t count = -1;
    volatile uint32_t my_num_bytes_read;
    volatile uint32_t my_offset = *offset;
    volatile uint8_t my_buf[buffer_size];
    
    
    for (count = 0; count < FILE_LIST_COUNT; count ++){
        if (my_file_list[count].hash == hash){
            if (strncmp(my_file_list[count].file_name, "largefile.json", strlen("largefile.json")) == 0){
                *num_bytes_read = GenerateLargeFileJson(index, buffer_size, buffer);
            }else{
                *num_bytes_read = 0;
            }
            if ((my_offset > 0) && (*num_bytes_read > 0)){
                my_num_bytes_read = *num_bytes_read;
                //move the buffered data
                memmove(buffer, (uint8_t *)(buffer + (*num_bytes_read - *offset)), *offset);
                *num_bytes_read = *offset;
                *offset = 0;
            }
            return;
        }
    }
    if (count == -1){
        *num_bytes_read = 0;
        return;
    }
}

/******************************************************************************
 * FUNCTION:  GenerateFileSize
 *
 * DESCRIPTION:
 * This function determines the size of the next itteration of calling 
 * GenerateFileRead().  This function checks the file name to determine the 
 * next file size.
 *
 * PARAMETERS:
 * hash             uint16_t    hash of the file name
 *
 * RETURN VALUE:
 * uint32_t     size of the next iteration of reading the file
 *
 * NOTES:  None.
 *
 *****************************************************************************/
uint32_t GenerateFileSize(uint16_t hash){
    volatile uint16_t count = 0;
    volatile uint16_t i;
    volatile uint32_t num_bytes_read = 0;
    volatile uint32_t sum=0;
    volatile uint8_t buf[512];
            
    for (count = 0; count < FILE_LIST_COUNT; count ++){
        if (my_file_list[count].hash == hash){
            if (strncmp(my_file_list[count].file_name, "largefile.json", strlen("largefile.json")) == 0){
                i = 0;
                //loop through all indexes to get the size
                do{
                    num_bytes_read = GenerateLargeFileJson((uint16_t *)&i,512, (uint8_t *)buf);
                    sum += num_bytes_read;
                }while (num_bytes_read > 0);
            }
            break;            
        }
    }
    return sum;
}

/******************************************************************************
 * FUNCTION:  GenerateLargeFileJson
 *
 * DESCRIPTION:
 * This function generates the file data for largefile.json.
 *
 * PARAMETERS:
 * index    uint16_t *  index or itteration count of generating this file
 * max_size     uint32_t    maximum size of the buffer
 * buffer       uint8_t *   pointer to the buffer for the generated data
 *
 * RETURN VALUE:
 * uint32_t     size of the next iteration of reading the file
 *
 * NOTES:  None.
 *
 *****************************************************************************/
uint32_t GenerateLargeFileJson(uint16_t *index, uint32_t max_size, uint8_t *buffer){
    volatile uint32_t size = 0;
    
    switch (*index){
        case 0:     //a
        case 1:     //b
        case 2:     //c
        case 3:     //d
        case 4:     //e
        case 5:     //f
        case 6:     //g
        case 7:     //h
        case 8:     //i
        case 9:     //j
        case 10:    //k
        case 11:    //l
        case 12:    //m
        case 13:    //n
        case 14:    //o
        case 15:    //p
        case 16:    //q
        case 17:    //r
        case 18:    //s
        case 19:    //t
        case 20:    //u 
        case 21:    //v
        case 22:    //w
        case 23:    //x
        case 24:    //y
        case 25:    //z
            memset(buffer, 'a'+*index, max_size);
            buffer[max_size-1] = '\r';
            buffer[max_size - 0] = '\n';
            size = max_size;
            break;
        default:
            size = 0;
            break;
    }
    
    if (size > 0){
        (*index) ++;   //increment json creation index
        if (size > max_size ){
            //the size of data is to large for the buffer.
            memset(buffer, 0x00, max_size); //clear the buffer
            size = 0;
            (*index) --;
        }
    }
    
    return size;
}