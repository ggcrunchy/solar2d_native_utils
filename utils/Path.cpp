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

#include "utils/Byte.h"
#include "utils/LuaEx.h"
#include "utils/Path.h"

namespace PathXS {
	static int FromSystem (lua_State * L, const char * name)
	{
		if (lua_isnil(L, -1)) return LUA_NOREF;

		lua_getfield(L, -1, name);	// system, system[name]

		return luaL_ref(L, LUA_REGISTRYINDEX);	// system
	}

	Directories * Directories::Instantiate (lua_State * L)
	{
		Directories * dirs = LuaXS::NewTyped<Directories>(L);	// ..., dirs

		lua_getglobal(L, "system");	// ..., dirs, system

		dirs->mPathForFile = FromSystem(L, "pathForFile");
		dirs->mDocumentsDir = FromSystem(L, "DocumentsDirectory");
		dirs->mResourceDir = FromSystem(L, "ResourceDirectory");

		lua_pop(L, 1);	// ..., dirs

		return dirs;
	}

	const char * Directories::Canonicalize (lua_State * L, bool bRead)
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

		mLib = nullptr;
	}

	void LibLoader::Load (const char * name)
	{
		Close();

	#ifdef _WIN32
		mLib = LoadLibraryExA(name, nullptr, 0); // following nvcore/Library.h...
	#else
		mLib = dlopen(name, RTLD_LAZY);
	#endif
	}

	WriteAux::WriteAux (lua_State * L, int w, int h, Directories * dirs) : mFilename(nullptr), mW(w), mH(h)
	{
		if (dirs) mFilename = dirs->Canonicalize(L, false);
	}

	const unsigned char * WriteAux::GetBytes (lua_State * L, const ByteReader & reader, size_t w, size_t size, int barg) const
	{
		return ByteXS::FitData(L, reader, barg, w, size_t(mH) * size);
	}

	WriteAuxReader::WriteAuxReader (lua_State * L, int w, int h, int barg, Directories * dirs) : WriteAux(L, w, h, dirs), mReader(L, barg), mBArg(barg)
	{
		if (!mReader.mBytes) lua_error(L);
	}

	const unsigned char * WriteAuxReader::GetBytes (lua_State * L, size_t w, size_t size)
	{
		return WriteAux::GetBytes(L, mReader, w, size, mBArg);
	}
}