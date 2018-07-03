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
#include "utils/Byte.h"
#include "utils/LuaEx.h"

#ifdef _WIN32
	#include <Windows.h>
#else
	#include <dlfcn.h>
#endif

#ifdef __ANDROID__
    #include <sys/types.h>
    #include <android/asset_manager.h>
    #include <android/asset_manager_jni.h>

    #define ASSETS_JNI_BOILERPLATE(PLUGIN)  static jfieldID sLuaThreadID;               \
                                                                                        \
    /* Adapted from JNLua source: */                                                    \
    static jclass referenceclass (JNIEnv * env, const char * name)                      \
    {                                                                                   \
        jclass clazz = env->FindClass(name);                                            \
                                                                                        \
        if (!clazz) return nullptr;                                                     \
                                                                                        \
        return (jclass)env->NewGlobalRef(clazz);                                        \
    }                                                                                   \
                                                                                        \
    static lua_State * getluathread (JNIEnv * env, jobject javastate)                   \
    {                                                                                   \
        return (lua_State *) (uintptr_t) env->GetLongField(javastate, sLuaThreadID);    \
    }                                                                                   \
                                                                                        \
    extern "C" {                                                                        \
        JNIEXPORT jint JNICALL Java_plugin_##PLUGIN##_LuaLoader_OpenLib (               \
            JNIEnv * env, jobject thiz, jobject javastate                               \
        ) {                                                                             \
            lua_State * L = getluathread(env, javastate);                               \
                                                                                        \
            lua_pushcfunction(L, luaopen_plugin_##PLUGIN);   /* ..., open_PLUGIN */     \
            lua_call(L, 0, 1);  /* ..., plugin */                                       \
                                                                                        \
            return 1;                                                                   \
        }                                                                               \
                                                                                        \
        JNIEXPORT void JNICALL Java_plugin_Bytemap_LuaLoader_SetAssets (                \
            JNIEnv * env, jobject thiz, jobject assets                                  \
        ) {                                                                             \
            PathXS::Directories::SetAssets(AAssetManager_fromJava(env, assets));        \
        }                                                                               \
                                                                                        \
        JNIEXPORT jint JNICALL JNI_OnLoad (JavaVM * vm, void *)                         \
        {                                                                               \
            JNIEnv * env;                                                               \
                                                                                        \
            jint result = vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6); \
                                                                                        \
            if (result != JNI_OK) return JNI_VERSION_1_6;                               \
                                                                                        \
            jclass luastate_class;                                                      \
                                                                                        \
            if (!(luastate_class = referenceclass(env, "com/naef/jnlua/LuaState")) ||   \
                !(sLuaThreadID = env->GetFieldID(luastate_class, "luaThread", "J")))    \
                return JNI_VERSION_1_6;                                                 \
                                                                                        \
            return JNI_VERSION_1_6;                                                     \
        }                                                                               \
    }
#else
    #define ASSETS_JNI_BOILERPLATE(PLUGIN)
#endif

namespace PathXS {
	struct Directories {
		int mDirsList;	// List to compare arguments
		int mDocumentsDir;	// Reference to documents directory
		int mIO_Open;	// Reference to io.open
		int mPathForFile;	// Reference to pathForFile
		int mResourceDir;	// Reference to resource directory
        bool mCanonicalize{true};   // Canonicalize file names before fetching content?
        bool mWantText{false};  // Read in text mode?
	#ifdef __ANDROID__
		static void SetAssets (AAssetManager * am);
	#endif
        
		static Directories * Instantiate (lua_State * L);
        
		const char * Canonicalize (lua_State * L, bool bRead, int arg = 1);
		bool IsDir (lua_State * L, int arg);
        bool ReadFileContents (lua_State * L, std::vector<unsigned char> & contents, int arg = 1);
	};

	struct LibLoader {
	#ifdef _WIN32
        HMODULE mLib{nullptr};
	#else
        void * mLib{nullptr};
	#endif

		bool IsLoaded (void) const { return mLib != nullptr; }
		bool LateLoad (void);
		void Close (void);
		void Load (const char * name);

		template<typename F> bool Bind (F * func, const char * name)
		{
			if (!mLib) return false;

	#ifdef _WIN32
			*func = (F)GetProcAddress(mLib, name);

			return func != nullptr;
	#else
			dlerror();

			*(void **)&func = dlsym(RTLD_NEXT, name);

			return func != nullptr || dlerror() == nullptr;
	#endif
		}
	};

	#define LIB_BIND(lib, prefix, n) lib.Bind(&n, #prefix #n)

	struct WriteAux {
		const char * mFilename{nullptr};
		int mW, mH;

		WriteAux (lua_State * L, int dim, Directories * dirs);

		template<typename T = unsigned char> const T * GetBytes (lua_State * L, const ByteReader & reader, size_t w) const
		{
			return ByteXS::EnsureN<T>(L, reader, w, size_t(mH) * sizeof(T));
		}
	};

	struct WriteAuxReader : WriteAux {
		ByteReader mReader;

		WriteAuxReader (lua_State * L, int dim, int barg, Directories * dirs = nullptr);

		template<typename T = unsigned char> const T * GetBytes (lua_State * L, size_t w)
		{
			return WriteAux::GetBytes<T>(L, mReader, w);
		}
	};

	template<typename T = unsigned char> struct WriteData {
		const T * mData;
		const char * mFilename;
		int mW, mH, mComp, mExtra;
		bool mAsUserdata;

		enum Extra { None, Quality, Stride };

		WriteData (lua_State * L, Directories * dirs = nullptr, Extra extra = None) : mData{nullptr}, mExtra{0}, mAsUserdata{false}
		{
			//
			WriteAuxReader waux{L, 2, 5, dirs};

			mFilename = waux.mFilename;
			mComp = luaL_checkint(L, 4);
			mW = waux.mW;
			mH = waux.mH;

			LuaXS::Options opts{L, 6};
			
			opts.Add("as_userdata", mAsUserdata);

			if (extra == Stride) opts.Add("stride", mExtra);
			else if (extra == Quality) opts.Add("quality", mExtra);

			//
			size_t w = mW * mComp;

			if (extra == Stride && mExtra != 0) w = mExtra;
			else if (extra == Quality && mExtra == 0) mExtra = 90;

			mData = waux.GetBytes<T>(L, w);
		}

		operator const T * (void) { return mData; }
	};
}
