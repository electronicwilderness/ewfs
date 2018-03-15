/******************************************************************************
 * FILE NAME:  custom_file_app.h
 *
 * FILE DESCRIPTION:
 * The header file for custom generated files.
 *
 * FILE NOTES:  None.
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
