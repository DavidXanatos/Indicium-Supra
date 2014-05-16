#include "client.h"

#include <dllmain.h>
#include <shared/PipeMessages.h>
#include <utils/misc.h>
#include <string>
#include <ShlObj.h>

#include <boost/algorithm/string.hpp>

struct sParamInfo
{
	std::string szParamName;
	std::string szParamValue;
};

sParamInfo paramArray[3] =
{
	"procName", "",
	"windowName", "",
	"isWindow", "1"
};

bool IsServerAvailable()
{
	BitStream bsIn, bsOut;
	bsIn.Write((short) PipeMessages::Ping);
	return CNamedPipeClient("Overlay_Server", &bsIn, &bsOut).Success();
}

EXPORT void SetParam(char *_szParamName, char *_szParamValue)
{
	for (int i = 0; i < ARRAYSIZE(paramArray); i++)
	if (boost::iequals(_szParamName, paramArray[i].szParamName))
		paramArray[i].szParamValue = _szParamValue;

	return;
}

std::string GetParam(char *_szParamName)
{
	for (int i = 0; i < ARRAYSIZE(paramArray); i++)
	if (boost::iequals(_szParamName, paramArray[i].szParamName))
		return paramArray[i].szParamValue;

	return "";
}


EXPORT int Init()
{
	char szDLLPath[MAX_PATH + 1] = { 0 };
	DWORD dwPId = 0;
	BOOL bRetn;

	GetModuleFileName((HMODULE) g_hDllHandle, szDLLPath, sizeof(szDLLPath));
	if (!atoi(GetParam("isWindow").c_str()))
	{
		std::string szSearchName = GetParam("procName");
		dwPId = GetProcIdByProcName(szSearchName);
	}
	else
	{
		std::string szSearchName = GetParam("windowName");
		dwPId = GetProcIdByWindowName(szSearchName);
	}

	if (dwPId == 0)
	{
		return 0;
	}

	HANDLE hHandle = OpenProcess((STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xFFF), FALSE, dwPId);
	if (hHandle == 0 || hHandle == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	void *pAddr = VirtualAllocEx(hHandle, NULL, strlen(szDLLPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (pAddr == NULL)
	{
		CloseHandle(hHandle);
		return 0;
	}

	bRetn = WriteProcessMemory(hHandle, pAddr, szDLLPath, strlen(szDLLPath) + 1, NULL);
	if (bRetn == FALSE)
	{
		VirtualFreeEx(hHandle, pAddr, strlen(szDLLPath) + 1, MEM_RELEASE);
		CloseHandle(hHandle);
		return 0;
	}

	LPTHREAD_START_ROUTINE pFunc = (LPTHREAD_START_ROUTINE) GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	if (pFunc == NULL)
	{
		VirtualFreeEx(hHandle, pAddr, strlen(szDLLPath) + 1, MEM_RELEASE);
		CloseHandle(hHandle);
		return 0;
	}

	HANDLE hThread = CreateRemoteThread(hHandle, 0, 0, pFunc, pAddr, 0, 0);
	if (hThread == NULL || hThread == INVALID_HANDLE_VALUE)
	{
		VirtualFreeEx(hHandle, pAddr, strlen(szDLLPath) + 1, MEM_RELEASE);
		CloseHandle(hHandle);
		return 0;
	}

	WaitForSingleObject(hThread, INFINITE);
	VirtualFreeEx(hHandle, pAddr, strlen(szDLLPath) + 1, MEM_RELEASE);
	CloseHandle(hThread);
	CloseHandle(hHandle);

	return 1;
}