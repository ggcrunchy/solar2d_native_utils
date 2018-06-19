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

#ifdef __ANDROID__
    #include <android/asset_manager_jni.h>
	#include <assert.h>
#endif

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

#ifdef __ANDROID__
	static bool InResourceDir (Directories * dirs, lua_State * L, int darg)
	{
        lua_getref(L, dirs->mResourceDir);  // ..., dir, ..., ResourceDirectory

        bool bIn = lua_equal(L, darg, -1);

        lua_pop(L, 1);  // ..., dir, ...

		return bIn;
	}

    static AAssetManager * GetAssets (Directories * dirs, JNIEnv * env)
    {
        if (!dirs->mAssets)
        {
            // Adapted from code in:
            // https://github.com/openfl/lime/blob/e511c0d2a5d616081a7826416d111aff1d428025/legacy
            jclass cls = env->FindClass("com/ansca/corona/CoronaActivity");
            jmethodID mid = env->GetStaticMethodID(cls, "getAssetManager", "()Landroid/content/res/AssetManager;");
            
            if (mid)
            {
                jobject amgr = (jobject)env->CallStaticObjectMethod(cls, mid);
                
                if (amgr)
                {
                    dirs->mAssets = AAssetManager_fromJava(env, amgr);
                    
                    if (dirs->mAssets) dirs->mAssetsRef = (jclass)env->NewGlobalRef(amgr);
                    
                    env->DeleteLocalRef(amgr);
                }
            }
        }

        return dirs->mAssets;
    }
    
	static bool CheckAssets (Directories * dirs, lua_State * L, std::vector<unsigned char> & contents, int arg)
	{
        const char * filename = luaL_checkstring(L, arg);

        int bHasDir = lua_isuserdata(L, arg + 1);

        if (!bHasDir || InResourceDir(dirs, L, arg + 1))
        {
			void * env;

			jint result = dirs->mVM->GetEnv(&env, JNI_VERSION_1_6);
            JNIEnv * jenv = static_cast<JNIEnv *>(env);

			assert(result != JNI_EVERSION);

			if (result == JNI_EDETACHED && dirs->mVM->AttachCurrentThread(&jenv, nullptr) < 0) return false;

            AAssetManager * assets = GetAssets(dirs, jenv);

            if (!assets) return false;

            AAsset * asset = AAssetManager_open(assets, filename, AASSET_MODE_BUFFER);
            
            if (asset)
            {
                size_t len = AAsset_getLength(asset);
                
                contents.resize(len);
                
                AAsset_read(asset, contents.data(), len);
                AAsset_close(asset);
                
                if (bHasDir) lua_remove(L, arg + 1);// ..., str, ...
            }

			if (result == JNI_EDETACHED) dirs->mVM->DetachCurrentThread();

			return asset != nullptr;
        }
	}
#endif

    bool Directories::ReadFileContents (lua_State * L, std::vector<unsigned char> & contents, int arg)
    {
        arg = CoronaLuaNormalize(L, arg);

    #ifdef __ANDROID__
        if (CheckAssets(this, L, contents, arg)) return true;
	#endif

		const char * filename = Canonicalize(L, true, arg);	// ..., filename, ...

        lua_getref(L, mIO_Open);// ..., filename, ..., io.open
		lua_pushvalue(L, arg);	// ..., filename, ..., io.open, filename
		lua_call(L, 1, 1);	// ..., filename, ..., file / nil

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
