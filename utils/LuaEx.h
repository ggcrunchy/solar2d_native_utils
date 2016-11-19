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
#include "CoronaGraphics.h"
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
	void AddCloseLogic (lua_State * L, lua_CFunction func);
	void AddRuntimeListener (lua_State * L, const char * name);
	void AddRuntimeListener (lua_State * L, const char * name, lua_CFunction func, int nupvalues = 0);
	void AttachGC (lua_State * L, lua_CFunction gc);
	void AttachGC (lua_State * L, const char * type, lua_CFunction gc);
	void AttachMethods (lua_State * L, const char * type, void (*populate)(lua_State *));
	void AttachProperties (lua_State * L, lua_CFunction get_props, const char ** nullable = nullptr);
	void CallInMainState (lua_State * L, lua_CFunction func);
	void LoadClosureLibs (lua_State * L, luaL_Reg closures[], int n, const AddParams & params = AddParams());
	void LoadFunctionLibs (lua_State * L, luaL_Reg funcs[], const AddParams & params = AddParams());
	void NewWeakKeyedTable (lua_State * L);

	int NoOp (lua_State * L);

	template<typename T> T * UD (lua_State * L, int arg)
	{
		return static_cast<T *>(lua_touserdata(L, arg));
	}

	template<typename T> T * CheckUD (lua_State * L, int arg, const char * name)
	{
		return static_cast<T *>(luaL_checkudata(L, arg, name));
	}

	template<typename T> T * ExtUD (lua_State * L, int arg)
	{
		return static_cast<T *>(CoronaExternalGetUserData(L, arg));
	}

	template<typename T> T * DualUD (lua_State * L, int arg, const char * name)
	{
		return IsType(L, name, arg) ? UD<T>(L, arg) : ExtUD<T>(L, arg);
	}

	template<typename T> int ArrayN (lua_State * L, int arg = 1)
	{
		return int(lua_objlen(L, arg) / sizeof(T));
	}

	template<typename T> void DestructTyped (lua_State * L, int arg = 1)
	{
		UD<T>(L, arg)->~T();
	}

	template<typename T> int TypedGC (lua_State * L)
	{
		DestructTyped<T>(L, 1);

		return 0;
	}

	template<typename T> void AttachTypedGC (lua_State * L, const char * type)
	{
		AttachGC(L, type, TypedGC<T>);
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
			{ nullptr, nullptr }
		};

		luaL_register(L, nullptr, len_methods);
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

	template<typename T> bool BytesToValue (lua_State * L, int arg, T & value)
	{
		static_assert(std::is_pod<T>::value, "BytesToValue() type must be plain-old-data");

		if (lua_type(L, arg) == LUA_TSTRING && lua_objlen(L, arg) == sizeof(T))
		{
			memcpy(&value, lua_tostring(L, arg), sizeof(T));

			return true;
		}

		return false;
	}

	template<typename T> void ValueToBytes (lua_State * L, const T & value)
	{
		static_assert(std::is_pod<T>::value, "ValueToBytes() type must be plain-old-data");

		lua_pushlstring(L, reinterpret_cast<const char *>(&value), sizeof(T));	// ..., bytes
	}

	// Helper to get an option from the top of Lua's stack, if present
	template<typename T, typename R = T> R GetOpt (lua_State *);

	template<> inline bool GetOpt<bool> (lua_State * L) { return lua_toboolean(L, -1) != 0; }
	template<> inline int GetOpt<int> (lua_State * L) { return luaL_checkint(L, -1); }
	template<> inline long GetOpt<long> (lua_State * L) { return luaL_checklong(L, -1); }
	template<> inline lua_Number GetOpt<lua_Number> (lua_State * L) { return luaL_checknumber(L, -1); }

	template<> inline lua_Integer GetOpt<unsigned int, lua_Integer> (lua_State * L)
	{
		lua_Integer ret = luaL_checkinteger(L, -1);

		return ret >= 0 ? ret : 0;
	}

	//
	class Options {
		lua_State * mL;	// Current state
		int mArg;	// Stack position

		// Helper to get the default value for a missing option
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

				if (!lua_isnil(mL, -1))
				{
					using opt_type = std::conditional<std::is_same<T, bool>::value, bool, // Is the option a bool? Use that, if so...
						std::conditional<std::is_floating_point<T>::value, lua_Number, // ...next try floats...
							std::conditional<std::is_same<T, long>::value, long,// ...then longs...
								std::conditional<std::is_unsigned<T>::value, unsigned int, int>::type	// ... and finally ints.
							>::type
						>::type
					>::type;
					using ret_type = std::conditional<std::is_same<opt_type, unsigned int>::value, lua_Integer, opt_type>::type;

					opt = static_cast<T>(GetOpt<opt_type, ret_type>(mL));
				}

				else opt = def;

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

	// Helper to push arguments onto Lua's stack
	template<typename T> void PushArg (lua_State *, T arg);

	template<> inline void PushArg<nullptr_t> (lua_State * L, nullptr_t) { lua_pushnil(L); }
	template<> inline void PushArg<bool> (lua_State * L, bool b) { lua_pushboolean(L, b ? 1 : 0); }
	template<> inline void PushArg<lua_Integer> (lua_State * L, lua_Integer i) { lua_pushinteger(L, i); }
	template<> inline void PushArg<lua_Number> (lua_State * L, lua_Number n) { lua_pushnumber(L, n); }
	template<> inline void PushArg<const char *> (lua_State * L, const char * s) { lua_pushstring(L, s); }
	template<> inline void PushArg<void *> (lua_State * L, void * p) { lua_pushlightuserdata(L, p); }

	template<typename T> int PushArgAndReturn (lua_State * L, T arg)
	{
		using arg_type = std::conditional<std::is_pointer<T>::value,// Is the argument a pointer?
			std::conditional<std::is_same<std::decay<T>::type, char>::value,
				const char *,	// If so, use either strings...
				void *	// ...or light userdata.
			>::type,
			std::conditional<std::is_floating_point<T>::value,
				lua_Number, // Otherwise, is it a float...
				std::conditional<std::is_integral<T>::value && !std::is_same<T, bool>::value,	// ...or a non-boolean integer?
					lua_Integer,
					T	// Boolean or null: use the raw type.
				>::type
			>::type
		>::type;

		PushArg<arg_type>(L, static_cast<arg_type>(arg));

		return 1;
	}
};