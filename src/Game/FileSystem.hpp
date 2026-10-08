#pragma once

namespace Game
{
	typedef void(*FS_FreeFile_t)(void* buffer);
	extern FS_FreeFile_t FS_FreeFile;

	typedef int(*FS_ReadFile_t)(const char* path, void** buffer);
	extern FS_ReadFile_t FS_ReadFile;

	typedef char**(*FS_ListFiles_t)(const char* path, const char* extension, int behavior,
		int* numFiles, int tag);
	extern FS_ListFiles_t FS_ListFiles;

	typedef fileHandle_t(*FS_FOpenFileWrite_t)(const char* filename);
	extern FS_FOpenFileWrite_t FS_FOpenFileWrite;

	typedef int(*FS_FOpenFileRead_t)(const char* filename, fileHandle_t* file);
	extern FS_FOpenFileRead_t FS_FOpenFileRead;
	extern FS_FOpenFileRead_t FS_FOpenFileReadDatabase;

	typedef int(*FS_FOpenFileReadForThread_t)(const char* filename, fileHandle_t* file, FsThread thread);
	extern FS_FOpenFileReadForThread_t FS_FOpenFileReadForThread;

	typedef int(*FS_FOpenFileByMode_t)(const char* qpath, fileHandle_t* file, fsMode_t mode);
	extern FS_FOpenFileByMode_t FS_FOpenFileByMode;

	typedef void(*FS_FCloseFile_t)(fileHandle_t file);
	extern FS_FCloseFile_t FS_FCloseFile;

	typedef void(*FS_WriteToDemo_t)(const void* buffer, int length, fileHandle_t file);
	extern FS_WriteToDemo_t FS_WriteToDemo;

	typedef int(*FS_Write_t)(const void* buffer, int len, fileHandle_t file);
	extern FS_Write_t FS_Write;

	typedef int(*FS_Printf_t)(fileHandle_t file, const char* format, ...);
	extern FS_Printf_t FS_Printf;

	typedef int(*FS_Read_t)(void* buffer, int len, fileHandle_t file);
	extern FS_Read_t FS_Read;

	typedef int(*FS_Seek_t)(fileHandle_t file, int offset, int origin);
	extern FS_Seek_t FS_Seek;

	typedef int(*FS_FTell_t)(fileHandle_t file);
	extern FS_FTell_t FS_FTell;

	typedef void(*FS_Restart_t)(int localClientNum, int checksumFeed);
	extern FS_Restart_t FS_Restart;

	typedef int(*FS_Delete_t)(const char* filename);
	extern FS_Delete_t FS_Delete;

	extern searchpath_s** fs_searchpaths;

	int FS_FOpenFileReadCurrentThread(const char* filename, fileHandle_t* file);

	void BindFileSystem();
}
