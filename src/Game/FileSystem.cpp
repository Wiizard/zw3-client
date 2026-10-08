#include "STDInclude.hpp"

namespace Game
{
	FS_FreeFile_t FS_FreeFile = nullptr;
	FS_ReadFile_t FS_ReadFile = nullptr;
	FS_ListFiles_t FS_ListFiles = nullptr;
	FS_FOpenFileWrite_t FS_FOpenFileWrite = nullptr;
	FS_FOpenFileRead_t FS_FOpenFileRead = nullptr;
	FS_FOpenFileRead_t FS_FOpenFileReadDatabase = nullptr;
	FS_FOpenFileReadForThread_t FS_FOpenFileReadForThread = nullptr;
	FS_FOpenFileByMode_t FS_FOpenFileByMode = nullptr;
	FS_FCloseFile_t FS_FCloseFile = nullptr;
	FS_WriteToDemo_t FS_WriteToDemo = nullptr;
	FS_Write_t FS_Write = nullptr;
	FS_Printf_t FS_Printf = nullptr;
	FS_Read_t FS_Read = nullptr;
	FS_Seek_t FS_Seek = nullptr;
	FS_FTell_t FS_FTell = nullptr;
	FS_Restart_t FS_Restart = nullptr;
	FS_Delete_t FS_Delete = nullptr;

	searchpath_s** fs_searchpaths = nullptr;

	int FS_FOpenFileReadCurrentThread(const char* filename, fileHandle_t* file)
	{
		if (Sys_IsMainThread())
		{
			return FS_FOpenFileRead(filename, file);
		}

		if (Sys_IsDatabaseThread())
		{
			return FS_FOpenFileReadDatabase(filename, file);
		}

		*file = 0;
		return -1;
	}

	void BindFileSystem()
	{
		FS_FreeFile = BindFunction<FS_FreeFile_t>(0x140279000);
		FS_ReadFile = BindFunction<FS_ReadFile_t>(0x140279050);
		FS_ListFiles = BindFunction<FS_ListFiles_t>(0x140279020);
		FS_FOpenFileWrite = BindFunction<FS_FOpenFileWrite_t>(0x140276A10);
		FS_FOpenFileRead = BindFunction<FS_FOpenFileRead_t>(0x140275BB0);
		FS_FOpenFileReadDatabase = BindFunction<FS_FOpenFileRead_t>(0x140275BD0);
		FS_FOpenFileReadForThread = BindFunction<FS_FOpenFileReadForThread_t>(0x140275BE0);
		FS_FOpenFileByMode = BindFunction<FS_FOpenFileByMode_t>(0x1402759C0);
		FS_FCloseFile = BindFunction<FS_FCloseFile_t>(0x140275920);
		FS_WriteToDemo = BindFunction<FS_WriteToDemo_t>(0x140278E50);
		FS_Write = BindFunction<FS_Write_t>(0x140278CA0);
		FS_Printf = BindFunction<FS_Printf_t>(0x140277BD0);
		FS_Read = BindFunction<FS_Read_t>(0x140277CB0);
		FS_Seek = BindFunction<FS_Seek_t>(0x1402782F0);
		FS_FTell = BindFunction<FS_FTell_t>(0x140276B70);
		FS_Restart = BindFunction<FS_Restart_t>(0x140277FB0);
		FS_Delete = BindFunction<FS_Delete_t>(0x140275870);

		fs_searchpaths = reinterpret_cast<searchpath_s**>(Utils::Hook::Rebase(0x146644B90));
	}
}
