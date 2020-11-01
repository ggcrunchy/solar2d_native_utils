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
#include "utils/Namespace.h"
#include <utility>

#ifdef _WIN32
	#include <Windows.h>
#else
	#include <dlfcn.h>
#endif

CEU_BEGIN_NAMESPACE(PathXS) {
	struct Directories {
		int mDirsList;	// List to compare arguments
		int mDocumentsDir;	// Reference to documents directory
		int mIO_Open;	// Reference to io.open
		int mPathForFile;	// Reference to pathForFile
		int mResourceDir;	// Reference to resource directory
    #ifdef __ANDROID__
        int mProxy{LUA_NOREF}; // Asset reader proxy
    #endif
        bool mCanonicalize{true};   // Canonicalize file names before fetching content?
        bool mWantText{false};  // Read in text mode?
        
		static Directories * Instantiate (lua_State * L);
        
        struct FileContentsRAII {
            lua_State * mL{nullptr};
			FILE * mFP{nullptr};
            int mPos{0};
#ifdef __ANDROID__
            int mProxy{LUA_NOREF};
#endif
            ~FileContentsRAII (void);
        };

		const char * Canonicalize (lua_State * L, bool bRead, int arg = 1);
		bool IsDir (lua_State * L, int arg);
        bool UsesResourceDir (lua_State * L, int arg);
        void ReadFileContents (lua_State * L, int arg = 1);
        FileContentsRAII WithFileContents (lua_State * L, int arg = 1);

        static bool AbsolutePathsOK (void);

        template<typename F> bool AuxWithFileContentsDo (lua_State * L, int findex, F && func, bool bOldCanonicalize)
        {
            auto fc = WithFileContents(L, findex);  // ..., proxy / contents / nil
            mCanonicalize = bOldCanonicalize;
            
            ByteReader bytes{L, -1};

            return bytes.mBytes ? func(bytes) : false;
        }

		template<typename F> bool AuxWithContentsDo (lua_State* L, int findex, F && func, bool)
		{
			lua_pushvalue(L, findex);	// ..., proxy / contents / nil

			ByteReader bytes{ L, -1 };

			return bytes.mBytes ? func(bytes) : false;
		}

        template<typename F> bool WithFileContentsDo (lua_State * L, int findex, int aindex, F && func)
        {
            bool bOldCanonicalize = mCanonicalize;

            if (lua_toboolean(L, aindex))
            {
                luaL_argcheck(L, AbsolutePathsOK(), aindex, "Absolute paths only allowed on desktop");

                mCanonicalize = false;
            }

			if (static_cast<Directories*>(lua_touserdata(L, aindex)) == this) return AuxWithContentsDo(L, findex, std::forward<F>(func), false);

            return AuxWithFileContentsDo(L, findex, std::forward<F>(func), bOldCanonicalize);
        }
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
            // TODO: these checks ^^^ should be against *func, no?
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
	};
CEU_END_NAMESPACE(PathXS)
