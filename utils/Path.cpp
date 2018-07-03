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

		lua_newtable(L);// ..., dirs, system, dlist

		for (lua_pushnil(L); lua_next(L, -3); lua_pop(L, 1))
		{
			if (!lua_isstring(L, -2) || !lua_isuserdata(L, -1)) continue;

			size_t nchars = lua_objlen(L, -2);

			if (nchars <= sizeof("Directory")) continue;

			if (strcmp(lua_tostring(L, -2) + nchars - sizeof("Directory") + 1, "Directory") == 0)
			{
				lua_pushvalue(L, -1);	// ..., dirs, system, dlist, name, nonce, nonce
				lua_pushboolean(L, 1);	// ..., dirs, system, dlist, name, nonce, nonce, true
				lua_rawset(L, -5);	// ..., dirs, system, dlist = { ..., [nonce] = true }, name, nonce
			}
		}

		dirs->mDirsList = lua_ref(L, 1);	// ..., dirs, system

		lua_getglobal(L, "require");// ..., dirs, system, require; n.b. io might not be loaded, e.g. in luaproc process
		lua_pushliteral(L, "io");	// ..., dirs, system, require, "io"
		lua_call(L, 1, 1);	// ..., dirs, system, io
		lua_getfield(L, -1, "open");// ..., dirs, system, io, io.open

		dirs->mIO_Open = lua_ref(L, 1);	// ..., dirs, system, io

		lua_pop(L, 2);	// ..., dirs

		return dirs;
	}

	const char * Directories::Canonicalize (lua_State * L, bool bRead, int arg)
	{
		arg = CoronaLuaNormalize(L, arg);

		luaL_checkstring(L, arg);
		lua_getref(L, mPathForFile);// ..., str[, dir], ..., pathForFile
		lua_pushvalue(L, arg);	// ..., str[, dir], ..., pathForFile, str

		if (IsDir(L, arg + 1))
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

	bool Directories::IsDir (lua_State * L, int arg)
	{
		lua_pushvalue(L, arg);	// ..., dir?, ..., dir?
		lua_getref(L, mDirsList);	// ..., dir?, list
		lua_insert(L, -2);	// ..., dir?, ..., list, dir?
		lua_rawget(L, -2);	// ..., dir?, ..., list, is_dir

		bool bIsDir = LuaXS::Bool(L);

		lua_pop(L, 2);	// ..., dir?, ...

		return bIsDir;
	}

#ifdef __ANDROID__
	static AAssetManager * sAssets;

    void Directories::SetAssets (AAssetManager * am)
    {
        sAssets = am;
    }

    static bool InResourceDir (Directories * dirs, lua_State * L, int darg)
	{
        lua_getref(L, dirs->mResourceDir);  // ..., dir, ..., ResourceDirectory

        bool bIn = lua_equal(L, darg, -1) != 0;

        lua_pop(L, 1);  // ..., dir, ...

		return bIn;
	}
    
	struct AssetRequest {
		const char * mFilename;
		std::vector<unsigned char> & mContents;
		bool mOK{false};

        AssetRequest (const char * filename, std::vector<unsigned char> & contents) : mFilename{filename}, mContents(contents)
        {
        }
	};

	static int GetAsset (lua_State * L)
	{
		AssetRequest * ar = LuaXS::UD<AssetRequest>(L, 1);
		AAsset * asset = AAssetManager_open(sAssets, ar->mFilename, AASSET_MODE_BUFFER);
            
        if (asset)
        {
            size_t len = AAsset_getLength(asset);
                
            ar->mContents.resize(len);
                
            AAsset_read(asset, ar->mContents.data(), len);
            AAsset_close(asset);

            ar->mOK = true;
        }

		return 0;
	}

	static bool CheckAssets (Directories * dirs, lua_State * L, std::vector<unsigned char> & contents, int arg)
	{
		if (!sAssets) return false;

        const char * filename = luaL_checkstring(L, arg);
        bool bHasDir = dirs->IsDir(L, arg + 1);

        if (!bHasDir || InResourceDir(dirs, L, arg + 1))
        {
			AssetRequest ar{filename, contents};

            LuaXS::CallInMainState(L, GetAsset, &ar);
            
            if (bHasDir) lua_remove(L, arg + 1);// ..., str, ...

			return ar.mOK;
        }

		return false;
	}
#endif

    bool Directories::ReadFileContents (lua_State * L, std::vector<unsigned char> & contents, int arg)
    {
        arg = CoronaLuaNormalize(L, arg);

    #ifdef __ANDROID__
        if (CheckAssets(this, L, contents, arg)) return true;
	#endif

        const char * filename;

        if (mCanonicalize) filename = Canonicalize(L, true, arg);	// ..., filename, ...
        else filename = luaL_checkstring(L, arg);

        lua_getref(L, mIO_Open);// ..., filename, ..., io.open
		lua_pushvalue(L, arg);	// ..., filename, ..., io.open, filename
		lua_pushstring(L, mWantText ? "r" : "rb"); // ..., filename, ..., io.open, filename, mode
		lua_call(L, 2, 1);	// ..., filename, ..., file / nil

		bool bOpened = !lua_isnil(L, -1);

		if (bOpened)
		{
			lua_getfield(L, -1, "read");// ..., filename, ..., file, file.read
			lua_insert(L, -2);	// ..., filename, ..., file.read, file
			lua_pushliteral(L, "*a");	// ..., filename, ..., file.read, file, "*a"
			lua_call(L, 2, 1);	// ..., filename, ..., contents

			const unsigned char * uc = reinterpret_cast<const unsigned char *>(lua_tostring(L, -1));

			contents.assign(uc, uc + lua_objlen(L, -1));
		}

		lua_pop(L, 1);	// ..., filename, ...

		return bOpened;
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
