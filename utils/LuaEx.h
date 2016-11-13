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
#include <type_traits>
#include <utility>

namespace LuaXS {
	struct AddParams {
		int mFirstPos;
		int mTablePos;
		bool mLeaveTableAtTop;

		AddParams (int tpos = 0, int first = 0) : mFirstPos(first), mTablePos(tpos), mLeaveTableAtTop(true)
		{
		}
	};

	bool IsMainState (lua_State * L);
	bool IsType (lua_State * L, const char * name, int index = -1);
	bool IsType (lua_State * L, const char * name, const char * alt, int index = -1);

	void AddClosures (lua_State * L, luaL_Reg closures[], int n, const AddParams & params = AddParams());
	void AttachGC (lua_State * L, lua_CFunction gc);
	void AttachGC (lua_State * L, const char * type, lua_CFunction gc);
	void AttachMethods (lua_State * L, const char * type, void (*populate)(lua_State *));
	void LoadClosureLibs (lua_State * L, luaL_Reg closures[], int n, const AddParams & params = AddParams());
	void LoadFunctionLibs (lua_State * L, luaL_Reg funcs[], const AddParams & params = AddParams());
	void NewWeakKeyedTable (lua_State * L);

	template<typename T> T * UD (lua_State * L, int arg)
	{
		return static_cast<T *>(lua_touserdata(L, arg));
	}

	template<typename T> T * CheckUD (lua_State * L, int arg, const char * name)
	{
		return static_cast<T *>(luaL_checkudata(L, arg, name));
	}

	template<typename T> int ArrayN (lua_State * L, int arg = 1)
	{
		return int(lua_objlen(L, arg) / sizeof(T));
	}

	template<typename T> void DestructTyped (lua_State * L, int arg = 1)
	{
		UD<T>(L, arg)->~T();
	}

	template<typename T> void AttachTypedGC (lua_State * L, const char * type)
	{
		AttachGC(L, type, [](lua_State * L)
		{
			DestructTyped<T>(L, 1);

			return 0;
		});
	}

	template<typename T> int LenTyped (lua_State * L)
	{
		lua_pushinteger(L, ArrayN<T>(L));	// arr, n
    
		return 1;
	}

	template<typename T> void AttachTypedLen (lua_State * L)
	{
		luaL_Reg len_methods[] = {
			{
				"__len", LenTyped<T>
			},
			{ NULL, NULL }
		};

		luaL_register(L, NULL, len_methods);
	}

	template<typename T> T * NewArray (lua_State * L, size_t n)
	{
		static_assert(std::is_pod<T>::value, "NewArray() type must be plain-old-data");

		return static_cast<T *>(lua_newuserdata(L, n * sizeof(T)));	// ..., ud
	}

	template<typename T, typename ... Args> T * NewTyped (lua_State * L, Args && ... args)
	{
		T * instance = static_cast<T *>(lua_newuserdata(L, sizeof(T)));	// ..., ud

		new (instance) T(std::forward<Args>(args)...);

		return instance;
	}

	template<typename T, typename ... Args> T * NewSizeTyped (lua_State * L, size_t size, Args && ... args)
	{
		if (size < sizeof(T)) luaL_error(L, "NewSizeTyped() called with insufficient size");

		T * instance = static_cast<T *>(lua_newuserdata(L, size));	// ..., ud

		new (instance) T(std::forward<Args>(args)...);

		return instance;
	}

	//
	template<typename T> struct GetOpt {
		template<bool is_integral = std::is_integral<T>::value, bool is_signed = std::is_signed<T>::value> static T Get (lua_State * L);

		template<> static inline T Get<true, true> (lua_State * L)
		{
			return static_cast<T>(luaL_checkint(L, -1));
		}

		//
		template<typename T> static T GetUnsigned (lua_State * L)
		{
			lua_Integer ret = luaL_checkinteger(L, -1);

			if (ret < 0) ret = 0;

			return static_cast<T>(ret);
		}

		template<> static inline bool GetUnsigned<bool> (lua_State * L) { return lua_toboolean(L, -1) != 0; }

		template<> static inline T Get<true, false> (lua_State * L)
		{
			return GetUnsigned<T>(L);
		}

		template<> static inline T Get<false, true> (lua_State * L)
		{
			return static_cast<T>(luaL_checknumber(L, -1));
		}
	};

	//
	class Options {
		lua_State * mL;	// Current state
		int mArg;	// Stack position

		template<typename T> T GetDef (T def) { return def; }

		template<> inline bool GetDef<bool> (bool) { return false; }

	public:
		Options (lua_State * L, int arg);

		Options & Add (const char * name, const char *& opt);
		Options & ArgCheck (bool bOK, const char * message);

		void Replace (const char * field);
		bool WasSkipped (void) { return mArg == 0; }

		//
		template<typename T> Options & Add (const char * name, T & opt, T def)
		{
			static_assert(std::is_arithmetic<T>::value, "Expected a numeric type");

			if (mArg)
			{
				lua_getfield(mL, mArg, name);// ..., value

				opt = !lua_isnil(mL, -1) ? GetOpt<T>::Get(mL) : def;

				lua_pop(mL, 1);	// ...
			}

			return *this;
		}

		//
		template<typename T> Options & Add (const char * name, T & opt)
		{
			static_assert(std::is_arithmetic<T>::value, "Expected a numeric type");

			if (mArg) return Add(name, opt, GetDef(opt));

			return *this;
		}

		template<typename T, typename F, typename ... Args> Options & AddByCall (const char * name, T & opt, F func, Args && ... args)
		{
			if (mArg)
			{
				lua_getfield(mL, mArg, name);// ..., opt

				if (!lua_isnil(mL, -1)) opt = func(mL, -1, std::forward<Args>(args)...);

				lua_pop(mL, 1);	// ...
			}

			return *this;
		}

		template<typename T, typename F, typename ... Args> Options & AddByCall (const char * name, T * opt, F func, Args && ... args)
		{
			if (opt) return AddByCall(name, *opt, func, args...);

			return *this;
		}
	};
};