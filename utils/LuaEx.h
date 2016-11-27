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
#include <initializer_list>
#include <type_traits>
#include <string>
#include <utility>

namespace LuaXS {
	struct AddParams {
		int mFirstPos;
		int mTablePos;
		bool mLeaveTableAtTop{true};

		AddParams (int tpos = 0, int first = 0) : mFirstPos{first}, mTablePos{tpos}
		{
		}
	};

	bool IsMainState (lua_State * L);
	bool IsType (lua_State * L, const char * name, int index = -1);
	bool IsType (lua_State * L, const char * name, const char * alt, int index = -1);

	void AddClosures (lua_State * L, luaL_Reg closures[], int n, const AddParams & params = AddParams{});
	void AddCloseLogic (lua_State * L, lua_CFunction func);
	void AddRuntimeListener (lua_State * L, const char * name);
	void AddRuntimeListener (lua_State * L, const char * name, lua_CFunction func, int nupvalues = 0);
	void AttachGC (lua_State * L, lua_CFunction gc);
	void AttachGC (lua_State * L, const char * type, lua_CFunction gc);
	void AttachMethods (lua_State * L, const char * type, void (*populate)(lua_State *));
	void AttachProperties (lua_State * L, lua_CFunction get_props, const char ** nullable = nullptr);
	void CallInMainState (lua_State * L, lua_CFunction func);
	void LoadClosureLibs (lua_State * L, luaL_Reg closures[], int n, const AddParams & params = AddParams{});
	void LoadFunctionLibs (lua_State * L, luaL_Reg funcs[], const AddParams & params = AddParams{});
	void NewWeakKeyedTable (lua_State * L);
	
	int BoolResult (lua_State * L, int b);
	int ErrorAfterFalse (lua_State * L);
	int ErrorAfterNil (lua_State * L);
	int GetFlags (lua_State * L, int arg, std::initializer_list<const char *> lnames, std::initializer_list<int> lflags, const char * def = nullptr);
	int GetFlags (lua_State * L, int arg, const char * name, std::initializer_list<const char *> lnames, std::initializer_list<int> lflags, const char * def = nullptr);
	int NoOp (lua_State * L);

	size_t Find (lua_State * L, int t, int item);

	template<typename T> int ResultOrNil (lua_State * L, T ok)
	{
		if (!ok) lua_pushnil(L);// ..., res[, ok]

		return 1;
	}

	template<typename ... Args> int WithError (lua_State * L, const char * format, Args && ... args)
	{
		lua_pushfstring(L, format, std::forward<Args>(args)...);// nil / false, err

		return 2;
	}

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

	template<typename T> size_t ArrayN (lua_State * L, int arg = 1)
	{
		return lua_objlen(L, arg) / sizeof(T);
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
		return PushArgAndReturn(L, ArrayN<T>(L));	// arr, n
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
		static_assert(std::is_nothrow_default_constructible<T>::value, "NewArray() type must be nothrow default-constructible");

		return static_cast<T *>(lua_newuserdata(L, n * sizeof(T)));	// ..., ud
	}

	template<typename T> T * AllocTyped (lua_State * L)
	{
		return static_cast<T *>(lua_newuserdata(L, sizeof(T)));	// ..., ud
	}

	template<typename T, typename ... Args> T * NewTyped (lua_State * L, Args && ... args)
	{
		T * instance = AllocTyped<T>(L);// ..., ud

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

	template<typename F> void ForEachI (lua_State * L, int arg, F && func, bool bIterOnceIfNonTable = false)
	{
		for (size_t i : Range(L, arg, bIterOnceIfNonTable)) func(L, i);
	}

	template<typename F> void ForEachI (lua_State * L, int arg, size_t n, F && func)
	{
		for (size_t i = 1; i <= n; ++i, lua_pop(L, 1))
		{
			lua_rawgeti(L, arg, i);	// ..., item

			func(L, i);
		}
	}

	template<typename P, typename F> void ForEachI (lua_State * L, int arg, P && preamble, F && func, bool bIterOnceIfNonTable = false)
	{
		size_t n = lua_objlen(L, arg);

		preamble(L, n);

		ForEachI(L, arg, std::forward<F>(func), bIterOnceIfNonTable);
	}

	struct Range {
		lua_State * mL;
		int mArg, mTop;
		bool mIsTable, mNonTableOK;

		struct Iter {
			Range & mParent;
			size_t mIndex;

			Iter & operator ++ (void);
			bool operator != (const Iter & other) const { return mIndex != other.mIndex; }
			Iter & operator * (void);
			operator size_t (void);

			int ReturnFrom (int n);

			Iter (Range & parent) : mParent{parent}
			{
			}
		};

		Iter begin (void);
		Iter end (void);

		Range (lua_State * L, int arg, bool bIterOnceIfNonTable = false);
		~Range (void);
	};

	template<typename T> bool BytesToValue (lua_State * L, int arg, T & value)
	{
		static_assert(std::is_trivially_copyable<T>::value, "BytesToValue() type must be trivially copyable");

		if (lua_type(L, arg) == LUA_TSTRING && lua_objlen(L, arg) == sizeof(T))
		{
			memcpy(&value, lua_tostring(L, arg), sizeof(T));

			return true;
		}

		return false;
	}

	template<typename T> void ValueToBytes (lua_State * L, const T & value)
	{
		static_assert(std::is_trivially_copyable<T>::value, "ValueToBytes() type must be trivially copyable");

		lua_pushlstring(L, reinterpret_cast<const char *>(&value), sizeof(T));	// ..., bytes
	}

	// Helper to get an argument from Lua's stack
	template<typename T, typename R = T> R GetArgBody (lua_State *, int);

	template<> inline bool GetArgBody<bool> (lua_State * L, int arg) { return lua_toboolean(L, arg) != 0; }
	template<> inline int GetArgBody<int> (lua_State * L, int arg) { return luaL_checkint(L, arg); }
	template<> inline long GetArgBody<long> (lua_State * L, int arg) { return luaL_checklong(L, arg); }
	template<> inline lua_Number GetArgBody<lua_Number> (lua_State * L, int arg) { return luaL_checknumber(L, arg); }
	template<> inline const char * GetArgBody<const char *> (lua_State * L, int arg) { return luaL_checkstring(L, arg); }

	template<> inline void * GetArgBody<void *> (lua_State * L, int arg)
	{
		luaL_argcheck(L, lua_isuserdata(L, arg), arg, "Non-userdata argument");

		return lua_touserdata(L, arg);
	}

	template<> inline lua_Integer GetArgBody<unsigned int, lua_Integer> (lua_State * L, int arg)
	{
		lua_Integer ret = luaL_checkinteger(L, arg);

		return ret >= 0 ? ret : 0U;
	}

	template<typename T> T GetArg (lua_State * L, int arg = -1)
	{
		using opt_type = std::conditional<!std::is_arithmetic<T>::value || std::is_same<T, bool>::value,// Is the type non-arithmetic (or bool)?
			T, // If so, use it as is.
			std::conditional<std::is_floating_point<T>::value,	// Otherwise, is it a float?
				lua_Number, // If so, use numbers.
				std::conditional<std::is_same<T, long>::value, long,// Failing that, we have either a long...
					std::conditional<std::is_unsigned<T>::value, unsigned int, int>::type	// ...or finally an int with a certain signedness.
				>::type
			>::type
		>::type;
		using ret_type = std::conditional<std::is_same<opt_type, unsigned int>::value, lua_Integer, opt_type>::type;

		return static_cast<T>(GetArgBody<opt_type, ret_type>(L, arg));
	}

	bool Bool (lua_State * L, int arg = -1);
	float Float (lua_State * L, int arg = -1);
	double Double (lua_State * L, int arg = -1);
	int Int (lua_State * L, int arg = -1);
	long Long (lua_State * L, int arg = -1);
	unsigned int Uint (lua_State * L, int arg = -1);
	const char * String (lua_State * L, int arg = -1);
	void * Userdata (lua_State * L, int arg = -1);

	//
	class Options {
		lua_State * mL;	// Current state
		int mArg;	// Stack position

	public:
		Options (lua_State * L, int arg);

		Options & Add (const char * name, bool & opt);
		Options & ArgCheck (bool bOK, const char * message);

		void Replace (const char * field);
		bool WasSkipped (void) { return mArg == 0; }

		//
		template<typename F> Options & WithFieldDo (const char * name, F && func)
		{
			if (mArg)
			{
				lua_getfield(mL, mArg, name);// ..., value

				if (!lua_isnil(mL, -1)) func();

				lua_pop(mL, 1);	// ...
			}

			return *this;
		}

		//
		template<typename F, typename A> Options & WithFieldDo (const char * name, F && func, A && alt)
		{
			if (mArg)
			{
				lua_getfield(mL, mArg, name);// ..., value

				!lua_isnil(mL, -1) ? func() : alt();

				lua_pop(mL, 1);	// ...
			}

			return *this;
		}

		//
		template<typename T> Options & Add (const char * name, T & opt, T def)
		{
			return WithFieldDo(name, [=, &opt](){
				opt = GetArg<T>(mL);
			}, [def, &opt](){
				opt = def;
			});
		}

		//
		template<typename T> Options & Add (const char * name, T & opt)
		{
			return Add(name, std::forward<T &>(opt), opt);
		}

		//
		template<typename F, typename ... Args> Options & Call (const char * name, F && func, Args && ... args)
		{
			return WithFieldDo(name, [&](){
				func(mL, std::forward<Args>(args)...);
			});
		}
	};

	//
	struct StackIndex {
		int mIndex;

		StackIndex (lua_State * L, int index)
		{
			mIndex = CoronaLuaNormalize(L, index);
		}

		static StackIndex Top (lua_State * L) { return StackIndex(L, -1); }

		operator int (void) { return mIndex; }
	};

	// Helper to push argument onto Lua's stack
	template<typename T> void PushArgBody (lua_State *, T arg);

	template<> inline void PushArgBody<nullptr_t> (lua_State * L, nullptr_t) { lua_pushnil(L); }
	template<> inline void PushArgBody<bool> (lua_State * L, bool b) { lua_pushboolean(L, b ? 1 : 0); }
	template<> inline void PushArgBody<lua_CFunction> (lua_State * L, lua_CFunction f) { lua_pushcfunction(L, f); }
	template<> inline void PushArgBody<lua_Integer> (lua_State * L, lua_Integer i) { lua_pushinteger(L, i); }
	template<> inline void PushArgBody<lua_Number> (lua_State * L, lua_Number n) { lua_pushnumber(L, n); }
	template<> inline void PushArgBody<const char *> (lua_State * L, const char * s) { lua_pushstring(L, s); }
	template<> inline void PushArgBody<const std::string &> (lua_State * L, const std::string & s) { lua_pushlstring(L, s.data(), s.length()); }
	template<> inline void PushArgBody<void *> (lua_State * L, void * p) { lua_pushlightuserdata(L, p); }
	template<> inline void PushArgBody<StackIndex> (lua_State * L, StackIndex si) { return lua_pushvalue(L, si); }

	template<typename T> static void PushArg (lua_State * L, T arg)
	{
		using arg_type = std::conditional<std::is_pointer<T>::value && !std::is_same<T, lua_CFunction>::value,	// Is the argument a (non-Lua function) pointer?
			std::conditional<std::is_same<std::decay<T>::type, char>::value,
				const char *,	// If so, use either strings...
				void *	// ...or light userdata.
			>::type,
			std::conditional<std::is_floating_point<T>::value,
				lua_Number, // Otherwise, is it a float...
				std::conditional<std::is_integral<T>::value && !std::is_same<T, bool>::value,	// ...or a non-boolean integer?
					lua_Integer,
					T	// Boolean, null, Lua function, stack index, or user-specialized: use the raw type.
				>::type
			>::type
		>::type;

		PushArgBody(L, arg_type(arg));
	}

	template<typename T> int PushArgAndReturn (lua_State * L, T arg)
	{
		PushArg(L, std::forward<T>(arg));

		return 1;
	}

	template<typename T> void PushMultipleArgs (lua_State * L, T && arg) { PushArg(L, std::forward<T>(arg)); }

	template<typename T, typename ... Args> void PushMultipleArgs (lua_State * L, T && arg, Args && ... args)
	{
		PushArg(L, std::forward<T>(arg));
		PushMultipleArgs(L, std::forward<Args>(args)...);
	}

	template<typename ... Args> int PushMultipleArgsAndReturn (lua_State * L, Args && ... args)
	{
		PushMultipleArgs(L, std::forward<Args>(args)...);

		return sizeof...(args);
	}

	template<typename ... Args> bool PCall (lua_State * L, lua_CFunction func, Args && ... args)
	{
		PushMultipleArgs(L, func, std::forward<Args>(args)...);	// ..., func, ...

		return lua_pcall(L, sizeof...(args), 0, 0) == 0;// ...[, err]
	}

	template<typename ... Args> bool PCallN (lua_State * L, lua_CFunction func, int nresults, Args && ... args)
	{
		PushMultipleArgs(L, func, std::forward<Args>(args)...);	// ..., func, ...

		return lua_pcall(L, sizeof...(args), nresults, 0) == 0;	// ..., err / results
	}
};