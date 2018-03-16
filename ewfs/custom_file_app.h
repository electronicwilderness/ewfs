/******************************************************************************
 * FILE NAME:  custom_file_app.h
 *
 * FILE DESCRIPTION:
 * The header file for custom generated files.
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
#ifndef _EXAMPLE_FILE_NAME_H    /* Guard against multiple inclusion */
#define _EXAMPLE_FILE_NAME_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/******************************************************************************
 *                          FUNCTION PROTOTYPES
 *****************************************************************************/
void InitGeneratedFiles();
void GenerateFileRead(uint16_t hash, uint8_t *buffer, uint32_t buffer_size, 
        uint32_t *num_bytes_read, uint16_t *index, uint32_t *offset);
uint32_t GenerateFileSize(uint16_t hash);

#endif /* _EXAMPLE_FILE_NAME_H */
