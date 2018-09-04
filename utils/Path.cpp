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
#include "utils/Platform.h"

CEU_BEGIN_NAMESPACE(PathXS) {
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
    
    bool Directories::UsesResourceDir (lua_State * L, int arg)
    {
        if (!IsDir(L, arg)) return true;

        lua_getref(L, mResourceDir);  // ..., dir, ..., ResourceDirectory
        
        bool bIsResDir = lua_equal(L, arg, -1) != 0;
        
        lua_pop(L, 1);  // ..., dir, ...
        
        return bIsResDir;
    }
    
#ifdef __ANDROID__
	static bool GetAssetReaderLib (lua_State * L)
	{
        lua_getglobal(L, "package");// ..., package
		lua_getfield(L, -2, "loaded");	// ..., package, package.loaded
		lua_pushliteral(L, "plugin.AssetReader");	// ..., package, package.loaded, "plugin.AssetReader"; n.b. not sure if one of these can be relied on :/
		lua_pushliteral(L, "plugin_AssetReader");	// ..., package, package.loaded, "plugin.AssetReader", "plugin_AssetReader"

		for (int i = 0; i < 2; ++i, lua_pop(L, 1))
		{
			lua_rawget(L, -3 + i);	// ..., package, package.loaded[, "plugin.AssetReader"], lib?

			if (lua_isnil(L, -1)) continue;

			luaL_checktype(L, -1, LUA_TTABLE);
			lua_replace(L, -4 + i);	// ..., lib, package.loaded[, "plugin.AssetReader"]
			lua_pop(L, 2 - i);	// ..., lib

			return true;
		}

		lua_pop(L, 2);	// ...

		return false;
	}

    static bool HasProxy (Directories * dirs, lua_State * L)
    {
        if (dirs->mProxy != LUA_NOREF) return true;

        if (GetAssetReaderLib(L))	// ...[, AssetReader]
        {
            lua_getfield(L, -1, "NewProxy");// ..., AssetReader, AssetReader.NewProxy
            lua_call(L, 0, 1);  // ..., AssetReader, proxy
            lua_remove(L, -2);  // ..., proxy

            dirs->mProxy = lua_ref(L, 1);   // ...

            return true;
        }

        return false;
    }

	static bool CheckAssets (Directories * dirs, lua_State * L, int arg)
    {
        if (dirs->UsesResourceDir(L, arg + 1))
        {
            if (!HasProxy(dirs, L)) return false;

            if (dirs->IsDir(L, arg + 1)) lua_remove(L, arg + 1);// ..., filename, ...

            lua_getref(L, dirs->mProxy);// ..., filename, ..., proxy
            lua_getfield(L, -1, "Bind");// ..., filename, ..., proxy, proxy:Bind
            lua_insert(L, -2);  // ..., filename, ..., proxy:Bind, proxy
            lua_pushvalue(L, arg);  // ..., filename, ..., proxy:Bind, proxy, filename
            lua_call(L, 2, 1);  // ..., filename, ..., ok

            if (lua_toboolean(L, -1)) lua_getref(L, dirs->mProxy);  // ..., filename, ..., true, proxy
            else lua_pushnil(L);// ..., filename, ..., false, nil

            lua_remove(L, -2);  // ..., filename, ..., proxy / nil

            return true;
        }

		return false;
	}
#endif

    void Directories::ReadFileContents (lua_State * L, int arg)
    {
        arg = CoronaLuaNormalize(L, arg);

        const char * filename = luaL_checkstring(L, arg);	// ensure string

    #ifdef __ANDROID__
        if (CheckAssets(this, L, arg)) return;  // ..., filename, ..., proxy / nil
	#endif

        if (mCanonicalize) filename = Canonicalize(L, true, arg);	// ..., filename[, base_dir], ...

        lua_getref(L, mIO_Open);// ..., filename[, base_dir], ..., io.open
		lua_pushvalue(L, arg);	// ..., filename[, base_dir], ..., io.open, filename
		lua_pushstring(L, mWantText ? "r" : "rb"); // ..., filename[, base_dir], ..., io.open, filename, mode
		lua_call(L, 2, 1);	// ..., filename[, base_dir], ..., file / nil

		if (!lua_isnil(L, -1))
		{
			lua_getfield(L, -1, "close");	// ..., filename[, base_dir], ..., file, file.close
			lua_getfield(L, -2, "read");// ..., filename[, base_dir], ..., file, file.close, file.read
			lua_pushvalue(L, -3);	// ..., filename[, base_dir], ..., file, file.close, file.read, file
			lua_pushliteral(L, "*a");	// ..., filename[, base_dir], ..., file, file.close, file.read, file, "*a"
			lua_call(L, 2, 1);	// ..., filename[, base_dir], ..., file, file.close, contents
			lua_insert(L, -3);	// ..., filename[, base_dir], ..., contents, file, file.close
			lua_insert(L, -2);	// ..., filename[, base_dir], ..., contents, file.close, file
			lua_call(L, 1, 0);	// ..., filename[, base_dir], ..., contents
		}
    }

    Directories::FileContentsRAII Directories::WithFileContents (lua_State * L, int arg)
    {
        ReadFileContents(L, arg);   // ..., filename[, base_dir], ..., contents / nil

        FileContentsRAII fc;

    #ifdef __ANDROID__
        if (!lua_isnil(L, -1))
        {
            fc.mL = L;

            if (mProxy != LUA_NOREF) fc.mPos = CoronaLuaNormalize(L, -1);
        }
    #endif

        return fc;
    }

    Directories::FileContentsRAII::~FileContentsRAII (void)
    {
    #ifdef __ANDROID__
        if (mPos)
        {
            lua_pushvalue(mL, mPos);// ..., proxy, ..., proxy
            lua_getfield(mL, -1, "Clear");  // ..., proxy, ..., proxy, proxy:Clear
            lua_insert(mL, -2); // ..., proxy, ..., proxy:Clear, proxy
            lua_pcall(mL, 1, 0, 0); // ..., proxy, ...
        } else
    #endif
		if (mFP) fclose(mFP);
    }

    bool Directories::AbsolutePathsOK (void)
    {
        return PlatformXS::is_win32 || (PlatformXS::is_apple && !PlatformXS::is_iphone);
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
CEU_CLOSE_NAMESPACE()
