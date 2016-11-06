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

#pragma once

#include "CoronaLua.h"
#include "ByteReader.h"

struct PathData {
	int mDocumentsDir;	// Reference to documents directory
	int mPathForFile;	// Reference to pathForFile
	int mResourceDir;	// Reference to resource directory

	static PathData * Instantiate (lua_State * L);

	const char * Canonicalize (lua_State * L, bool bRead);
};


#ifdef _WIN32
	#include <Windows.h>
#else
	#include <dlfcn.h>
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

struct WriteAux {
	const char * mFilename;
	int mW, mH;

	WriteAux (lua_State * L, int w, int h, PathData * pd);

	const char * GetBytes (lua_State * L, const ByteReader & reader, size_t w, size_t size, int barg) const;
};

struct WriteAuxReader : WriteAux {
	ByteReader mReader;
	int mBArg;

	WriteAuxReader (lua_State * L, int w, int h, int barg, PathData * pd = NULL);

	const char * GetBytes (lua_State * L, size_t w, size_t size);
};

template<typename T> struct WriteData {
	const char * mData, * mFilename;
	int mW, mH, mComp, mStride;
	bool mAsUserdata;

	WriteData (lua_State * L, PathData * pd = NULL, bool bHasStride = false) : mData(NULL), mStride(0), mAsUserdata(false)
	{
		mW = luaL_checkint(L, 2);
		mH = luaL_checkint(L, 3);

		//
		WriteAuxReader waux(L, mW, mH, 5, pd);

		mFilename = waux.mFilename;
		mComp = luaL_checkint(L, 4);

		if (lua_istable(L, 6))
		{
			lua_getfield(L, 6, "stride");	// ..., stride
			lua_getfield(L, 6, "as_userdata");	// ..., stride, as_userdata

			if (bHasStride) mStride = luaL_optint(L, -2, 0);

			mAsUserdata = lua_toboolean(L, -1) != 0;

			lua_pop(L, 2);	// ...
		}

		else if (bHasStride) mStride = luaL_optint(L, 6, 0);

		//
		size_t w = mW * mComp;

		if (mStride != 0) w = mStride;

		mData = waux.GetBytes(L, w, sizeof(T));
	}

	operator T * (void) { return (T *)mData; }
};