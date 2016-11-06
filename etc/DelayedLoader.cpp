#include "CoronaLua.h"
#include "utils/Path.h"

#ifdef _WIN32
	#include <Windows.h>
	#include <delayimp.h>
#endif

bool LibLoader::LateLoad (void)
{
	if (!mLib) return false;

#ifdef _WIN32
	char file[1024];

	DWORD n = GetModuleFileNameA(mLib, file, sizeof(file));

	for (; n > 0 && file[n - 1] != '\\' || file[n - 1] == '/'; --n);

	return !FAILED(__HrLoadAllImportsForDll(file + n));
#else
	return false;
#endif
}