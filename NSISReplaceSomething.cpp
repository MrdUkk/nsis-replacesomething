/*
	NSIS Plug-in: ReplaceSomething
	can replace ansi text in files.
	can replace BINARY bytes in files.
	
	TODO:
	  - Allow to process Unicode text files (currently ONLY ANSI!)
	  - Carefully work with memory region mapped from file (should handle exceptions)

	(C) 2021 Wholesome Software (http://softdev.online)
*/
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x600
#include <windows.h>
#include <wincrypt.h>
#include "pluginapi.h"

HINSTANCE g_hInstance = NULL;
int g_string_size = 1024;
extra_parameters* g_extra = NULL;

HANDLE hFileIn, hFileOut, hMemMappedFile;
LPVOID mappedMemory;

void* LocalAllocZero(size_t cb) { return LocalAlloc(LPTR, cb); }
inline void* LocalAlloc(size_t cb) { return LocalAllocZero(cb); }

HMODULE hSelf;

UINT_PTR NSISCallback(enum NSPIM reason)
{
	if (reason == NSPIM_UNLOAD)
	{
	}
	return 0;
}

#define PUBLIC_FUNCTION(Name) \
extern "C" void __declspec(dllexport) __cdecl Name(HWND hWndParent, int string_size, TCHAR* variables, stack_t** stacktop, extra_parameters* extra) \
{ \
  EXDLL_INIT(); \
  g_string_size = string_size; \
  g_extra = extra; \
  extra->RegisterPluginCallback(hSelf, NSISCallback);

#define PUBLIC_FUNCTION_END \
}

PUBLIC_FUNCTION(TextInFile)
{
	TCHAR *FilePathIn = (TCHAR*)LocalAlloc(g_string_size * sizeof(TCHAR));
	TCHAR *FilePathOut;
	TCHAR *strToFind = (TCHAR*)LocalAlloc(g_string_size * sizeof(TCHAR));
	TCHAR *strToReplace = (TCHAR*)LocalAlloc(g_string_size * sizeof(TCHAR));

	if (popstring(FilePathIn))
	{
		pushint(-1);
		pushint(0);
		LocalFree(FilePathIn);
		LocalFree(strToFind);
		LocalFree(strToReplace);
		return;
	}

	bool CaseSensitive = popint() > 0 ? true : false;
	
	FilePathOut = (TCHAR *)LocalAlloc(lstrlen(FilePathIn) + 4 + 1);
	lstrcpy(FilePathOut, FilePathIn);
	lstrcat(FilePathOut, TEXT(".tmp"));

	if (popstring(strToFind) || popstring(strToReplace))
	{
		pushint(-2);
		pushint(0);
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(strToFind);
		LocalFree(strToReplace);
		return;
	}

	hFileIn = CreateFile(FilePathIn, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileIn == INVALID_HANDLE_VALUE)
	{
		pushint(-3);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(strToFind);
		LocalFree(strToReplace);
		return;
	}

	hMemMappedFile = CreateFileMapping( hFileIn, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!hMemMappedFile)
	{
		pushint(-4);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(strToFind);
		LocalFree(strToReplace);
		CloseHandle(hFileIn);
		return;
	}

	mappedMemory = MapViewOfFile( hMemMappedFile, FILE_MAP_READ, 0, 0, 0);
	if (!mappedMemory)
	{
		pushint(-5);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(strToFind);
		LocalFree(strToReplace);
		CloseHandle(hMemMappedFile);
		CloseHandle(hFileIn);
		return;
	}

	hFileOut = CreateFile(FilePathOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileOut == INVALID_HANDLE_VALUE)
	{
		pushint(-6);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(strToFind);
		LocalFree(strToReplace);
		UnmapViewOfFile(mappedMemory);
		CloseHandle(hMemMappedFile);
		CloseHandle(hFileIn);
		return;
	}

	TCHAR *mappedMemoryCur = (TCHAR *)mappedMemory;
	TCHAR *ProcessedBytesPtrStart = mappedMemoryCur;
	TCHAR *FindstrCur = strToFind;
	DWORD ReplStrSize = lstrlen(strToReplace);

	if (CaseSensitive)
	{
		while (*mappedMemoryCur)
		{
			if (*mappedMemoryCur == *FindstrCur)
			{
				if (FindstrCur == strToFind)
				{
					DWORD BytesToWrite = mappedMemoryCur - ProcessedBytesPtrStart;
					DWORD BytesWritten = 0;
					WriteFile(hFileOut, ProcessedBytesPtrStart, BytesToWrite, &BytesWritten, NULL);
					ProcessedBytesPtrStart = mappedMemoryCur;
				}
				FindstrCur++;
				if (!*FindstrCur)
				{
					DWORD BytesWritten = 0;
					WriteFile(hFileOut, strToReplace, ReplStrSize, &BytesWritten, NULL);
					ProcessedBytesPtrStart = mappedMemoryCur + 1;
					FindstrCur = strToFind;
				}
			}
			else
			{
				FindstrCur = strToFind;
			}

			mappedMemoryCur++;
		}
	}
	else
	{
		CharLower(strToFind);

		while (*mappedMemoryCur)
		{
			//hack and need proper way to convert single char to lowercase
			TCHAR ch = *mappedMemoryCur;
			CharLowerBuff(&ch, 1);

			if (ch == *FindstrCur)
			{
				if (FindstrCur == strToFind)
				{
					DWORD BytesToWrite = mappedMemoryCur - ProcessedBytesPtrStart;
					DWORD BytesWritten = 0;
					WriteFile(hFileOut, ProcessedBytesPtrStart, BytesToWrite, &BytesWritten, NULL);
					ProcessedBytesPtrStart = mappedMemoryCur;
				}
				FindstrCur++;
				if (!*FindstrCur)
				{
					DWORD BytesWritten = 0;
					WriteFile(hFileOut, strToReplace, ReplStrSize, &BytesWritten, NULL);
					ProcessedBytesPtrStart = mappedMemoryCur + 1;
					FindstrCur = strToFind;
				}
			}
			else
			{
				FindstrCur = strToFind;
			}

			mappedMemoryCur++;
		}
	}

	DWORD BytesToWrite = mappedMemoryCur - ProcessedBytesPtrStart;
	DWORD BytesWritten = 0;
	WriteFile(hFileOut, ProcessedBytesPtrStart, BytesToWrite, &BytesWritten, NULL);

	UnmapViewOfFile(mappedMemory);
	CloseHandle(hMemMappedFile);
	CloseHandle(hFileIn);
	CloseHandle(hFileOut);

	if (!MoveFileEx(FilePathOut, FilePathIn, MOVEFILE_REPLACE_EXISTING))
	{
		pushint(-7);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(strToFind);
		LocalFree(strToReplace);
		return;
	}

	LocalFree(FilePathIn);
	LocalFree(FilePathOut);
	LocalFree(strToFind);
	LocalFree(strToReplace);

	pushint(0);
	pushint(0);
}
PUBLIC_FUNCTION_END

PUBLIC_FUNCTION(BytesInFile)
{
	TCHAR *FilePathIn = (TCHAR*)LocalAlloc(g_string_size * sizeof(TCHAR));
	TCHAR *FilePathOut;
	TCHAR *BytesToFindTxt = (TCHAR*)LocalAlloc(g_string_size * sizeof(TCHAR));
	TCHAR *BytesToReplaceTxt = (TCHAR*)LocalAlloc(g_string_size * sizeof(TCHAR));
	BYTE *BytesToFind, *BytesToReplace;

	if (popstring(FilePathIn))
	{
		pushint(-1);
		pushint(0);
		LocalFree(FilePathIn);
		LocalFree(BytesToFindTxt);
		LocalFree(BytesToReplaceTxt);
		return;
	}

	FilePathOut = (TCHAR *)LocalAlloc(lstrlen(FilePathIn) + 4 + 1);
	lstrcpy(FilePathOut, FilePathIn);
	lstrcat(FilePathOut, TEXT(".tmp"));
	
	if (popstring(BytesToFindTxt) || popstring(BytesToReplaceTxt))
	{
		pushint(-2);
		pushint(0);
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFindTxt);
		LocalFree(BytesToReplaceTxt);
		return;
	}

	DWORD BytesToFindSz;
	if (CryptStringToBinary(BytesToFindTxt, 0, CRYPT_STRING_HEX_ANY, NULL, &BytesToFindSz, NULL, NULL))
	{
		BytesToFind = (BYTE *)LocalAlloc(BytesToFindSz);
		if (!CryptStringToBinary(BytesToFindTxt, 0, CRYPT_STRING_HEX_ANY, BytesToFind, &BytesToFindSz, NULL, NULL))
		{
			pushint(-3);
			pushint(GetLastError());
			LocalFree(FilePathIn);
			LocalFree(FilePathOut);
			LocalFree(BytesToFindTxt);
			LocalFree(BytesToReplaceTxt);
			LocalFree(BytesToFind);
			return;
		}
	}
	else
	{
		pushint(-3);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFindTxt);
		LocalFree(BytesToReplaceTxt);
		return;
	}

	DWORD BytesToReplaceSz;
	if (CryptStringToBinary(BytesToReplaceTxt, 0, CRYPT_STRING_HEX_ANY, NULL, &BytesToReplaceSz, NULL, NULL))
	{
		BytesToReplace = (BYTE *)LocalAlloc(BytesToReplaceSz);
		if (!CryptStringToBinary(BytesToReplaceTxt, 0, CRYPT_STRING_HEX_ANY, BytesToReplace, &BytesToReplaceSz, NULL, NULL))
		{
			pushint(-4);
			pushint(GetLastError());
			LocalFree(FilePathIn);
			LocalFree(FilePathOut);
			LocalFree(BytesToFindTxt);
			LocalFree(BytesToReplaceTxt);
			LocalFree(BytesToFind);
			LocalFree(BytesToReplace);
			return;
		}
	}
	else
	{
		pushint(-4);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFindTxt);
		LocalFree(BytesToReplaceTxt);
		LocalFree(BytesToFind);
		return;
	}

	LocalFree(BytesToFindTxt);
	LocalFree(BytesToReplaceTxt);

	hFileIn = CreateFile(FilePathIn, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileIn == INVALID_HANDLE_VALUE)
	{
		pushint(-5);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFind);
		LocalFree(BytesToReplace);
		return;
	}

	DWORD dwFileSize = GetFileSize(hFileIn, NULL);
	if (!dwFileSize)
	{
		pushint(-6);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFind);
		LocalFree(BytesToReplace);
		CloseHandle(hFileIn);
		return;
	}

	hMemMappedFile = CreateFileMapping(hFileIn, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!hMemMappedFile)
	{
		pushint(-7);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFind);
		LocalFree(BytesToReplace);
		CloseHandle(hFileIn);
		return;
	}

	mappedMemory = MapViewOfFile(hMemMappedFile, FILE_MAP_READ, 0, 0, 0);
	if (!mappedMemory)
	{
		pushint(-8);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFind);
		LocalFree(BytesToReplace);
		CloseHandle(hMemMappedFile);
		CloseHandle(hFileIn);
		return;
	}

	hFileOut = CreateFile(FilePathOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFileOut == INVALID_HANDLE_VALUE)
	{
		pushint(-8);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFind);
		LocalFree(BytesToReplace);
		UnmapViewOfFile(mappedMemory);
		CloseHandle(hMemMappedFile);
		CloseHandle(hFileIn);
		return;
	}

	BYTE *mappedMemoryCur = (BYTE *)mappedMemory;
	BYTE *ProcessedBytesPtrStart = mappedMemoryCur;
	BYTE *FindstrCur = BytesToFind;

	while (dwFileSize--)
	{
		if (*mappedMemoryCur == *FindstrCur)
		{
			if (FindstrCur == BytesToFind)
			{
				DWORD BytesToWrite = mappedMemoryCur - ProcessedBytesPtrStart;
				DWORD BytesWritten = 0;
				WriteFile(hFileOut, ProcessedBytesPtrStart, BytesToWrite, &BytesWritten, NULL);
				ProcessedBytesPtrStart = mappedMemoryCur;
			}
			FindstrCur++;
			if ((FindstrCur - BytesToFind) == BytesToFindSz)
			{
				DWORD BytesWritten = 0;
				WriteFile(hFileOut, BytesToReplace, BytesToReplaceSz, &BytesWritten, NULL);
				ProcessedBytesPtrStart = mappedMemoryCur + 1;
				FindstrCur = BytesToFind;
			}
		}
		else
		{
			FindstrCur = BytesToFind;
		}

		mappedMemoryCur++;
	}

	DWORD BytesToWrite = mappedMemoryCur - ProcessedBytesPtrStart;
	DWORD BytesWritten = 0;
	WriteFile(hFileOut, ProcessedBytesPtrStart, BytesToWrite, &BytesWritten, NULL);

	UnmapViewOfFile(mappedMemory);
	CloseHandle(hMemMappedFile);
	CloseHandle(hFileIn);
	CloseHandle(hFileOut);

	if (!MoveFileEx(FilePathOut, FilePathIn, MOVEFILE_REPLACE_EXISTING))
	{
		pushint(-9);
		pushint(GetLastError());
		LocalFree(FilePathIn);
		LocalFree(FilePathOut);
		LocalFree(BytesToFind);
		LocalFree(BytesToReplace);
		return;
	}

	LocalFree(FilePathIn);
	LocalFree(FilePathOut);
	LocalFree(BytesToFind);
	LocalFree(BytesToReplace);

	pushint(0);
	pushint(0);
}
PUBLIC_FUNCTION_END

#ifdef _VC_NODEFAULTLIB
#define DllMain _DllMainCRTStartup
#endif
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
  g_hInstance = (HINSTANCE)hInst;
  return TRUE;
}
