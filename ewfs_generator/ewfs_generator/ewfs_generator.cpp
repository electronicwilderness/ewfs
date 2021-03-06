/******************************************************************************
FILE:	EWFS_Generator.cpp

DESCRIPTION:  Generates the Electronic Wilderness File System image given a
folder with data
 
LICENSE:
Copyright 2018 Eric Roman/ElectronicWilderness

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

******************************************************************************/

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include "dirent.h"
#include <direct.h>
#include <iostream>
#include <string.h>

using namespace std;

/******************************************************************************
Defines
******************************************************************************/
#define APPLICATION_VERSION		"0.01"
#define EWFS_START				"EWFS"
#define EWFS_VERSION			1
#define GetCurrentDir			_getcwd
#define EWFS_GENERATE_LIST		"ewfslist.txt"

/******************************************************************************
Function Prototypes
******************************************************************************/
void CmdLineUsage();
void ExecutableInfo();
void ProcessFilesInDirectoryForIndex(char *file_dir);
void CountNumFiles(char *file_dir);
void ProcessFilesInDirectoryForFile(char *file_dir);
void UpdateGeneratedList();
int FindInGenList(char * file_name, char * dir);

/******************************************************************************
Typedefs
******************************************************************************/
typedef enum {
	TYPE_GENERATED = 0,
	TYPE_FILE = 1
}file_type_e;

//structure of file system index
typedef struct{
	uint16_t hash;
	file_type_e type ;
	uint32_t offset;
	uint32_t length;
}fs_index_t;
#define EWFS_SINGLE_INDEX_SIZE	11

/******************************************************************************
Variables
******************************************************************************/
bool fileOverwrite = false;
char *outputFile = NULL;
char *inputDir = NULL;
uint16_t num_files = 0;		//counter for the number of files in the given directory
fs_index_t  *ewfs_index = NULL;
uint16_t ewfs_file_count = 0;
uint32_t ewfs_file_offset = 0;
uint8_t *file_data=NULL;
char generated_list_path[FILENAME_MAX];
#define EWFS_GEN_LIST_LEN_MAX	(1024 *5)	//the number of bytes that the file generation list can be
uint8_t *file_gen_list;

/******************************************************************************
FUNCTION:  main

DESCRIPTION:
Generate the EWFS file system.

PARAMETERS:
argc		int     number of command line arguments
argv[]		char *	array of command line arguments

RETURN VALUE:
int  returns 0

NOTES:

******************************************************************************/
int main(int argc, char *argv[]) {
	uint32_t cmdOptIndex;
	uint8_t temp;
	char cCurrentPath[FILENAME_MAX];
	FILE *outputFileHandle;
	int response;
	uint32_t all_file_size = 0;
	uint32_t i;

	//check if the current path is too long
	if (!GetCurrentDir(cCurrentPath, sizeof(cCurrentPath)))	{
		return errno;
	}

	cCurrentPath[sizeof(cCurrentPath) - 1] = '\0';	//not really required
	printf("The current working directory is %s", cCurrentPath);

	ExecutableInfo();	//Display executable information

	//check for arguments
	if (argc == 1) {
		//not enough arguments
		CmdLineUsage();
		return 0;
	}
	//parse through the arguments to find what and where is the data going
	for (cmdOptIndex = 1; cmdOptIndex < argc; cmdOptIndex++) {
		if (strcmp(argv[cmdOptIndex], "-f") == 0) {
			fileOverwrite = true;	//enable file overwriting without prompting
			fprintf(stdout, "File overwriting enabled without prompting.\n");
		}
		if (strcmp(argv[cmdOptIndex], "-i") == 0) {
			fprintf(stdout, "Using folder: %s\n", argv[cmdOptIndex + 1]);
			inputDir = argv[cmdOptIndex + 1];
		}
		if (strcmp(argv[cmdOptIndex], "-o") == 0){
			fprintf(stdout, "Creating file: %s\n", argv[cmdOptIndex + 1]);
			outputFile = argv[cmdOptIndex + 1];
		}
	}
	
	//open output file
	if (!fileOverwrite) {
		outputFileHandle = fopen(outputFile, "r");
		if (outputFileHandle) {
			//the file exists, ask the user to overwrite it
			fclose(outputFileHandle);	//close the file first
			fprintf(stdout, "File %s exists, overwrite?\n", outputFile);
			response = getc(stdin);
			while (getc(stdin) != '\n') {
				;	//wait for user input
			}
			if (response != 'y' && response != 'Y') {
				//the user didn't respond with yes, don't overwrite
				return 0;
			}
		}
	}

	//check for ewfslist.txt (list of generated files) and add list to array
	file_gen_list = (uint8_t *)malloc(EWFS_GEN_LIST_LEN_MAX);
	UpdateGeneratedList();

	//open and possible overwrite the file
	outputFileHandle = fopen(outputFile, "wb");	//wb - open file for writing binary

	//write file system header
	fputs(EWFS_START, outputFileHandle);
	fputc(EWFS_VERSION, outputFileHandle);
	//find the number of files
	num_files = 0; 
	CountNumFiles(inputDir);
	temp = num_files >> 0;
	fputc(temp, outputFileHandle);
	temp = (num_files & 0xff00) >> 8;
	fputc(temp, outputFileHandle);
	fprintf(stdout, "%i files\n", num_files);

	//build the file system index
	//allocate memory objects to file index
	ewfs_index = (fs_index_t *) malloc(sizeof(fs_index_t) * num_files);
	//Note:  The size of fs_index_t is larger here then it will be in c code - couldn't get __atribute__((packed)) to work
	ewfs_file_count = 0;
	ewfs_file_offset = 0;	//setup file offset from start of file system

	//process files in input directory
	ProcessFilesInDirectoryForIndex(inputDir);
	
	for (i = 0; i < num_files; i++) {
		//write hash
		temp = (ewfs_index[i].hash & 0xff) >> 0;
		fputc(temp, outputFileHandle);
		temp = (ewfs_index[i].hash & 0xff00) >> 8;
		fputc(temp, outputFileHandle);
		//write file type
		if (ewfs_index[i].type == TYPE_FILE) {
			fputc(0x01, outputFileHandle);
		} else {
			fputc(0x00, outputFileHandle);	//generated file type
		}
		//Note data is stored on Microchip in LSB first
		//write data offset
		temp = (ewfs_index[i].offset & 0xff) >> 0;
		fputc(temp, outputFileHandle);
		temp = (ewfs_index[i].offset & 0xff00) >> 8;
		fputc(temp, outputFileHandle);
		temp = (ewfs_index[i].offset & 0xff0000) >> 16;
		fputc(temp, outputFileHandle);
		temp = (ewfs_index[i].offset & 0xff000000) >> 24;
		fputc(temp, outputFileHandle);
		//write length
		temp = (ewfs_index[i].length & 0xff) >> 0;
		fputc(temp, outputFileHandle);
		temp = (ewfs_index[i].length & 0xff00) >> 8;
		fputc(temp, outputFileHandle);
		temp = (ewfs_index[i].length & 0xff0000) >> 16;
		fputc(temp, outputFileHandle);
		temp = (ewfs_index[i].length & 0xff000000) >> 24;
		fputc(temp, outputFileHandle);

		//update all files size for allocating memory
		all_file_size += ewfs_index[i].length;
	}
	
	file_data = (uint8_t *) malloc(all_file_size);
	fprintf(stdout, "total file size: %i\n", all_file_size);
	ewfs_file_offset = 0; //setup file offset from start of file data
	//process files by saving them into the buffer for the output file
	ProcessFilesInDirectoryForFile(inputDir);
	//copy buffer to output file
	for (i = 0; i < all_file_size; i++) {
		fputc(file_data[i], outputFileHandle);
	}

	free(file_data);
	free(ewfs_index);	//free memory
	free(file_gen_list);
	fflush(outputFileHandle);
	fclose(outputFileHandle);	//close the output file

	return 0;
}

/******************************************************************************
FUNCTION:  ExecutableInfo

DESCRIPTION:
Display application version infromation to the console window.

PARAMETERS:
none

RETURN VALUE:
none

NOTES:

******************************************************************************/
void ExecutableInfo() {
	fprintf(stdout, "\n");
	fprintf(stdout, "--------------------------------------------------------------\n");
	fprintf(stdout, "Electronic Wilderness File System (EWFS)\n");
	fprintf(stdout, "Version: %s\n", APPLICATION_VERSION);
	fprintf(stdout, "--------------------------------------------------------------\n");
}

/******************************************************************************
FUNCTION:  CmdLineUsage

DESCRIPTION:
Display the command line usage for this application.

PARAMETERS:
none

RETURN VALUE:
none

NOTES:

******************************************************************************/
void CmdLineUsage() {
	fprintf(stdout, "\n");
	fprintf(stdout, "Usage: ewfs_generator [OPTIONS] -i [INPUT DIR] -o [OUTPUT FILE NAME]\n");
	fprintf(stdout, "Where:\n");
	fprintf(stdout, "    [OPTIONS] where if -f is forced file overwrite without prompting./n");
	fprintf(stdout, "    [INPUT DIR] is the input path to the files and directories to add to the EWFS image.\n");
	fprintf(stdout, "    [OUTPUT FILE NAME] is the output image file name.\n");
}

/******************************************************************************
FUNCTION:  UpdateGeneratedList

DESCRIPTION:
Update the list of generated files.

PARAMETERS:
none

RETURN VALUE:
none

NOTES:

******************************************************************************/
void UpdateGeneratedList() {
	FILE *list_file_handle;
	uint32_t file_size, file_read_size;

	sprintf(generated_list_path, "%s/%s", inputDir, EWFS_GENERATE_LIST);
	fprintf(stdout, "Generating file list for generated files - %s\n", generated_list_path);

	list_file_handle = fopen(generated_list_path, "rb");
	if (list_file_handle == NULL) {
		return;
	}
	//load the file into file_gen_list, searching is done by strcmp
	//find the file size to expect
	fseek(list_file_handle, 0, SEEK_END);	//go to the end of the file
	file_size = ftell(list_file_handle);	//find the size
	fseek(list_file_handle, 0, SEEK_SET);	//go to beginning of file
	file_read_size = fread(file_gen_list, 1, file_size, list_file_handle);
	//check that there is data and size is less then maximum
	if (file_read_size != 0 && file_read_size < EWFS_GEN_LIST_LEN_MAX -1) {
		file_gen_list[file_read_size ] = 0x00;	//terminate string
	}
	fclose(list_file_handle);	//close the file
}

/******************************************************************************
FUNCTION:  CountNumFiles

DESCRIPTION:
This is a recursive function to count the number of files in the directory. By
using recursion, navigating through sub-directories and files can be done 
easily.

PARAMETERS:
file_dir	char *	pointer to file directory string

RETURN VALUE:
none

NOTES:
There is no need to check if the files are generated or not, it all counts.

******************************************************************************/
void CountNumFiles(char *file_dir) {
	DIR * in_dir;
	int result;
	struct stat file_info;
	struct dirent *dir_entry;
	char file_path[FILENAME_MAX];

	in_dir = opendir(file_dir);
	if (!in_dir) {
		fprintf(stdout, "Can't open directory '%s'.\n", file_dir);
		return;
	}
	//read and process each file in the directory
	while ((dir_entry = readdir(in_dir)) != NULL) {
		// Ignore "." and ".." and EWFS_GENERATE_LIST directories
		if (!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, "..") ||
			!strcmp(dir_entry->d_name, EWFS_GENERATE_LIST)) {
			continue;	//break out of loop
		}
		snprintf(file_path, FILENAME_MAX, "%s/%s", file_dir, dir_entry->d_name);
		result = stat(file_path, &file_info);
		//fprintf(stdout, "%s%s\n", file_dir, dir_entry->d_name);
		if (result == 0) {
			//is it a directory?
			if (S_ISDIR(file_info.st_mode)) {
				//yes, make a recursive call to this function for the new directory
				CountNumFiles(file_path);
			} else {
				num_files++;	//no, its a file so increment counter
			}
		} else {
			closedir(in_dir);	//close the directory
			return;	//something went wrong, so return
		}
	}
	closedir(in_dir);	//close the directory
}

/******************************************************************************
FUNCTION:  FindInGenList

DESCRIPTION:
This function mounts the EWFS file system.

PARAMETERS:
file_name	char *	file name to look for
dir			char *	directory of file system image

RETURN VALUE:
int  returns 1 if found, otherwise 0

NOTES:

******************************************************************************/
int FindInGenList(char * file_name, char * dir) {
	int result = 0;
	char *ptr_dir;
	char *file_path;
	int size;

	file_path = (char *)malloc(FILENAME_MAX);	//allocate memory

	//remove inputDir from dir string
	ptr_dir = strstr(dir, inputDir);
	ptr_dir += strlen(inputDir);	
	if (ptr_dir[0] == 0x00) {
		strcpy(file_path, file_name);
		// add \r\n
		size = strlen(file_path);
		file_path[size + 0] = '\r';
		file_path[size + 1] = '\n';
		file_path[size + 2] = 0x00;
	} else {
		ptr_dir++;	//increment for the '/' char
		sprintf(file_path, "%s/%s", ptr_dir, file_name);
		// add \r\n
		size = strlen(file_path);
		file_path[size + 0] = '\r';
		file_path[size + 1] = '\n';
		file_path[size + 2] = 0x00;
	}

	if (strstr((const char *)file_gen_list, file_path) == NULL) {
		result = 0;
	} else {
		result = 1;
	}
	free(file_path);	//deallocate memory
	return result;
}

/******************************************************************************
FUNCTION:  ProcessFilesInDirectoryForIndex

DESCRIPTION:
This function is recursively called to process all the files in a directory
to build the index of files and then digging deeper into the sub-directories.

PARAMETERS:
file_dir	char *	the directory of files to process

RETURN VALUE:
none

NOTES:

******************************************************************************/
void ProcessFilesInDirectoryForIndex(char *file_dir) {
	DIR * in_dir;
	int result;
	struct stat file_info;
	struct dirent *dir_entry;
	char file_path[FILENAME_MAX];
	int i;
	char *token;

	in_dir = opendir(file_dir);
	if (!in_dir) {
		fprintf(stdout, "Can't open directory '%s'.\n", file_dir);
		return;
	}
	//read and process each file in the directory
	while ((dir_entry = readdir(in_dir)) != NULL) {
		// Ignore "." and ".." and EWFS_GENERATE_LIST directories
		if (!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, "..") ||
			!strcmp(dir_entry->d_name, EWFS_GENERATE_LIST)) {
			continue;	//break out of loop
		}
		
		snprintf(file_path, FILENAME_MAX, "%s/%s", file_dir, dir_entry->d_name);
		result = stat(file_path, &file_info);
		
		if (result == 0) {
			//is it a directory?
			if (S_ISDIR(file_info.st_mode)) {
				//yes, make a recursive call to this function for the new directory
				ProcessFilesInDirectoryForIndex(file_path);
			}else {
				//no, its a file so process it
				fprintf(stdout, "FILE: %s\tSIZE: %i\tCOUNT: %i\n", file_path,file_info.st_size, ewfs_file_count);
				//check if the file is in the generated list
				if (FindInGenList(dir_entry->d_name, file_dir)) {
					ewfs_index[ewfs_file_count].type = TYPE_GENERATED;
					ewfs_index[ewfs_file_count].length = 0;
					ewfs_index[ewfs_file_count].offset = 0;
				} else {
					ewfs_index[ewfs_file_count].type = TYPE_FILE;
					ewfs_index[ewfs_file_count].length = file_info.st_size + 1;	//+1 for adding a 0 at the end of the file
					ewfs_index[ewfs_file_count].offset = ewfs_file_offset;
				}
				//calculate the file name hash
				ewfs_index[ewfs_file_count].hash = 0;
				token = strtok(file_path, "/");	//find the start of the folders where the files are located
				for (i = strlen(token) + 1; i < FILENAME_MAX; i++) {
					if (file_path[i] == 0x00) {
						break;	//done with string
					}
					ewfs_index[ewfs_file_count].hash = ewfs_index[ewfs_file_count].hash << 1;
					ewfs_index[ewfs_file_count].hash = ewfs_index[ewfs_file_count].hash + file_path[i];
				}
				fprintf(stdout, "\n\nHASH: %i\tOFFSET: %i\n", ewfs_index[ewfs_file_count].hash, ewfs_index[ewfs_file_count].offset);
				if (ewfs_index[ewfs_file_count].type == TYPE_GENERATED) {
					//don't change the offset if its a generated file because there is no file length for generated files
				} else {
					ewfs_file_offset = ewfs_index[ewfs_file_count].offset + ewfs_index[ewfs_file_count].length;
				}
				ewfs_file_count++;
			}
		} else {
			closedir(in_dir);	//close the directory
			return;	//something went wrong, so return
		}
	}
	closedir(in_dir);	//close the directory
}

/******************************************************************************
FUNCTION:  ProcessFilesInDirectoryForFile

DESCRIPTION:
This function is recursively called to process all the files in a directory to
build the file system image of the files.

PARAMETERS:
file_dir	char *	directory of fiiles to process

RETURN VALUE:
none

NOTES:

******************************************************************************/
void ProcessFilesInDirectoryForFile(char *file_dir) {
	DIR * in_dir;
	int result;
	struct stat file_info;
	struct dirent *dir_entry;
	char file_path[FILENAME_MAX];
	FILE *outputFileHandle;
	uint8_t * ptr_data = NULL;
	int file_size;

	in_dir = opendir(file_dir);
	if (!in_dir) {
		fprintf(stdout, "Can't open directory '%s'.\n", file_dir);
		return;
	}
	//read and process each file in the directory
	while ((dir_entry = readdir(in_dir)) != NULL) {
		// Ignore "." and ".." and EWFS_GENERATE_LIST directories
		if (!strcmp(dir_entry->d_name, ".") || !strcmp(dir_entry->d_name, "..") ||
			!strcmp(dir_entry->d_name, EWFS_GENERATE_LIST)) {
			continue;	//break out of loop
		}
		//check if the file is in the generated list
		if (FindInGenList(dir_entry->d_name, file_dir)) {
			continue;	//break out of loop
		}

		snprintf(file_path, FILENAME_MAX, "%s/%s", file_dir, dir_entry->d_name);
		result = stat(file_path, &file_info);

		if (result == 0) {
			//is it a directory?
			if (S_ISDIR(file_info.st_mode)) {
				//yes, make a recursive call to this function for the new directory
				ProcessFilesInDirectoryForFile(file_path);
			} else {
				//no, its a file so process it
				fprintf(stdout, "filename opened: %s/%s\n", file_dir, dir_entry->d_name);
				outputFileHandle = fopen(file_path, "rb");	//rb = read binary
				if (outputFileHandle == NULL){
					return;
				}
				ptr_data = file_data + ewfs_file_offset;	//update pointer where file is read into
				file_size = fread(ptr_data, 1, file_info.st_size, outputFileHandle);
				if (file_size != file_info.st_size) {
					return;
				}
				file_data[ewfs_file_offset + file_size] = 0x00;
				file_size++;
				fclose(outputFileHandle);
				ewfs_file_offset = ewfs_file_offset + file_size;
			}
		} else {
			closedir(in_dir);	//close the directory
			return;	//something went wrong, so return
		}
	}
	closedir(in_dir);	//close the directory
}
