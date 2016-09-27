#include "PathUtils.h"

static int FromSystem (lua_State * L, const char * name)
{
	if (lua_isnil(L, -1)) return LUA_NOREF;

	lua_getfield(L, -1, name);	// system, system[name]

	return luaL_ref(L, LUA_REGISTRYINDEX);	// system
}

PathData * PathData::Instantiate (lua_State * L)
{
	PathData * pd = (PathData *)lua_newuserdata(L, sizeof(PathData));	// ..., pd

	lua_getglobal(L, "system");	// ..., pd, system

	pd->mPathForFile = FromSystem(L, "pathForFile");
	pd->mDocumentsDir = FromSystem(L, "DocumentsDirectory");
	pd->mResourceDir = FromSystem(L, "ResourceDirectory");

	lua_pop(L, 1);	// ..., pd

	return pd;
}

const char * PathData::Canonicalize (lua_State * L, bool bRead)
{
	const char * str = luaL_checkstring(L, 1);

//	if (*str == '!') return ++str; // <- probably more problems than it's worth

	lua_rawgeti(L, LUA_REGISTRYINDEX, mPathForFile);// str[, dir], ..., pathForFile
	luaL_argcheck(L, !lua_isnil(L, -1), -1, "Missing pathForFile()");
	lua_pushvalue(L, 1);// str[, dir], ..., pathForFile, str

	bool has_userdata = lua_isuserdata(L, 2) != 0;

	if (has_userdata) lua_pushvalue(L, 2);	// str, dir, ..., pathForFile, str, dir

	else lua_rawgeti(L, LUA_REGISTRYINDEX, bRead ? mResourceDir : mDocumentsDir);	// str, ..., pathForFile, str, def_dir

	if (lua_pcall(L, 2, 1, 0) == 0)	// str[, dir], ..., file
	{
		if (has_userdata) lua_remove(L, 2);	// str, ..., file

		lua_replace(L, 1);	// file, ...
	}

	else lua_error(L);

	return lua_tostring(L, 1);
}

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

void LibLoader::Close (void)
{
	if (IsLoaded())
	{
#ifdef _WIN32
		FreeLibrary(mLib);
#else
		dlclose(mLib);
#endif
	}

	mLib = NULL;
}

void LibLoader::Load (const char * name)
{
	Close();

#ifdef _WIN32
	mLib = LoadLibraryExA(name, NULL, 0); // following nvcore/Library.h...
#else
	mLib = dlopen(name, RTLD_LAZY);
#endif
}