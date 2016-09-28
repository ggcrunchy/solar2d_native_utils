/*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

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