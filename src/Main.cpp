#include "Common.h"

namespace
{
	typedef HRESULT (WINAPI* DirectInput8CreateFn)(HINSTANCE, DWORD, const void*, void**, void*);

	char gameFolder[MAX_PATH];
}

const char* GameFolder()
{
	return gameFolder;
}

HRESULT CreateRealDirectInput(HINSTANCE instance, DWORD version, const void* iid, void** out, void* outer)
{
	static DirectInput8CreateFn create = nullptr;

	if (!create)
	{
		char systemFolder[MAX_PATH];
		const UINT length = GetSystemDirectoryA(systemFolder, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
			return E_FAIL;

		char path[MAX_PATH];
		snprintf(path, sizeof(path), "%s\\dinput8.dll", systemFolder);

		if (HMODULE module = LoadLibraryA(path))
			create = reinterpret_cast<DirectInput8CreateFn>(GetProcAddress(module, "DirectInput8Create"));
	}

	return create ? create(instance, version, iid, out, outer) : E_FAIL;
}

extern "C" __declspec(dllexport)
HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, const void* iid, void** out, void* outer)
{
	return CreateRealDirectInput(instance, version, iid, out, outer);
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(module);

		DWORD length = GetModuleFileNameA(nullptr, gameFolder, MAX_PATH);
		if (length >= MAX_PATH)
			length = MAX_PATH - 1;
		while (length > 0 && gameFolder[length - 1] != '\\' && gameFolder[length - 1] != '/')
			length--;
		gameFolder[length] = '\0';

		char lockName[64];
		snprintf(lockName, sizeof(lockName), "Local\\TS3Menu_%lu", GetCurrentProcessId());
		CreateMutexA(nullptr, FALSE, lockName);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
			return TRUE;

		hooks::Start();
	}
	else if (reason == DLL_PROCESS_DETACH && !reserved)
	{
		hooks::Stop();
	}

	return TRUE;
}
