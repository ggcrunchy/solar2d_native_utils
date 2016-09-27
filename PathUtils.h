#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include "CoronaLua.h"

struct PathData {
	int mDocumentsDir;	// Reference to documents directory
	int mPathForFile;	// Reference to pathForFile
	int mResourceDir;	// Reference to resource directory

	static PathData * Instantiate (lua_State * L);

	const char * Canonicalize (lua_State * L, bool bRead);
};


#ifdef _WIN32
	#include <Windows.h>
#endif

struct LibLoader {
#ifdef _WIN32
	HMODULE mLib;
#else
	void * mLib;
#endif

	LibLoader (void) : mLib(NULL) {}
	
	bool IsLoaded (void) const { return mLib != NULL; }
	bool LateLoad (void);
	void Close (void);
	void Load (const char * name);

	template<typename F> bool Bind (F * func, const char * name)
	{
		if (!mLib) return false;

#ifdef _WIN32
		*func = (F)GetProcAddress(mLib, name);

		return func != NULL;
#else
		dlerror();

		*(void **)&func = dlsym(RTLD_NEXT, name);

		return func != NULL || dlerror() == NULL;
#endif
	}
};

#define LIB_BIND(lib, prefix, n) lib.Bind(&n, #prefix #n)

#endif