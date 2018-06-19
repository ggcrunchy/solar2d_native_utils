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

		return lua_ref(L, 1);	// system
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

	const char * Directories::Canonicalize (lua_State * L, bool bRead, int arg)
	{
		arg = CoronaLuaNormalize(L, arg);

		luaL_checkstring(L, arg);
		lua_getref(L, mPathForFile);// ..., str[, dir], ..., pathForFile
		lua_pushvalue(L, arg);	// ..., str[, dir], ..., pathForFile, str

		if (lua_isuserdata(L, arg + 1))
		{
			lua_pushvalue(L, arg + 1);	// ..., str, dir, ..., pathForFile, str, dir
			lua_remove(L, arg + 1);	// ..., str, ..., pathForFile, str, dir
		}

		else lua_getref(L, bRead ? mResourceDir : mDocumentsDir);	// ..., str, ..., pathForFile, str, def_dir

		lua_call(L, 2, 1);	// ..., str, ..., file

		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1);	// ..., str, ...
			lua_pushliteral(L, "");	// ..., str, ..., ""
		}

		lua_replace(L, arg);// ..., file / "", ...

		return lua_tostring(L, arg);
	}

    void Directories::ReadFileContents (lua_State * L, std::vector<unsigned char> & contents, int arg)
    {
        arg = CoronaLuaNormalize(L, arg);

        contents.clear();

    #ifdef __ANDROID__
        const char * filename = luaL_checkstring(L, arg);
        int bInResourcesDir = true, bHasDir = lua_isuserdata(L, arg + 1);

        if (bHasDir)
        {
            lua_getref(L, mResourceDir);// ..., str, dir, ..., ResourceDirectory

            bInResourcesDir = lua_equal(L, arg + 1, -1);

            lua_pop(L, 1);  // ..., str, dir, ...
        }

        if (bInResourcesDir)
        {
            AAsset * file = AAssetManager_open(mAssets, filename, AASSET_MODE_BUFFER);
            
            if (file)
            {
                size_t len = AAsset_getLength(file);
                
                contents.resize(len);
                
                AAsset_read(file, contents.data(), len);
                AAsset_close(file);
                
                if (bHasDir) lua_remove(L, arg + 1);// ..., str, ...

                return;
            }
        }
    #endif
        
        filename = Canonicalize(L, true, arg);

        //
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

	WriteAux::WriteAux (lua_State * L, int dim, Directories * dirs)
	{
		if (dirs) mFilename = dirs->Canonicalize(L, false); // n.b. might remove dir

		mW = luaL_checkint(L, dim);
		mH = luaL_checkint(L, dim + 1);
	}

	WriteAuxReader::WriteAuxReader (lua_State * L, int dim, int barg, Directories * dirs) : WriteAux{L, dim, dirs}, mReader{L, barg}
	{
		if (!mReader.mBytes) lua_error(L);
	}
}
