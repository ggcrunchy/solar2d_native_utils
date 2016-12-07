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
#include "utils/Compat.h"
#include <pthread.h>
#include <stdint.h>
#include <limits>
#include <string>
#include <vector>

//
namespace CEU = CompatXS::ns_compat;

//
namespace LuaXS {
	//
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
	int NoOp (lua_State * L);

    size_t Find (lua_State * L, int t, int item);
    
    struct Range {
        lua_State * mL;
        int mArg, mTop;
        bool mIsTable, mNonTableOK;
        
        struct Iter {
            Range & mParent;
            int mIndex;
            
            Iter & operator ++ (void);
            Iter & operator * (void);
            operator int (void);
            bool operator != (const Iter & other) const { return mIndex != other.mIndex; }
            
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
    
    template<typename F> void ForEachI (lua_State * L, int arg, F && func, bool bIterOnceIfNonTable = false)
    {
        for (size_t i : Range(L, arg, bIterOnceIfNonTable)) func(L, i);
    }
    
    template<typename F> void ForEachI (lua_State * L, int arg, size_t n, F && func)
    {
        for (size_t i = 1; i <= n; ++i, lua_pop(L, 1))
        {
            lua_rawgeti(L, arg, int(i));// ..., item
            
            func(L, i);
        }
    }
    
    template<typename P, typename F> void ForEachI (lua_State * L, int arg, P && preamble, F && func, bool bIterOnceIfNonTable = false)
    {
        size_t n = lua_objlen(L, arg);
        
        preamble(L, n);
        
        ForEachI(L, arg, CompatXS::forward<F>(func), bIterOnceIfNonTable);
    }
    
	//
	struct FlagPair {
		const char * mName;
		int mFlag;
	};

	//
    template<size_t N> int GetFlags (lua_State * L, int arg, const FlagPair (&pairs)[N], const char * def = nullptr)
    {
		//
		int flags = 0, type = lua_type(L, arg);
        
        if (type == LUA_TTABLE || type == LUA_TSTRING)
        {
            //
            std::vector<const char *> vnames;
            std::vector<int> vflags;
            
			for (auto p : pairs)
			{
				vnames.push_back(p.mName);
				vflags.push_back(p.mFlag);
			}

            if (!def)
            {
                def = "";
                
                vnames.push_back("");
				vflags.push_back(0);
            }
            
            vnames.push_back(nullptr);
            
            //
            ForEachI(L, arg, [=, &flags](lua_State *, size_t) {
                flags |= vflags[luaL_checkoption(L, -1, def, vnames.data())];
            }, true);
        }
        
        return flags;
    }
    
    template<size_t N> int GetFlags (lua_State * L, int arg, const char * name, const FlagPair (&pairs)[N], const char * def = nullptr)
    {
        if (!lua_istable(L, arg)) return 0;
        
        lua_getfield(L, arg, name);	// ..., flags?
        
        int flags = GetFlags(L, arg, pairs, def);
        
        lua_pop(L, 1);	// ...
        
        return flags;
    }

	template<typename T> int ResultOrNil (lua_State * L, T ok)
	{
		if (!ok) lua_pushnil(L);// ..., res[, ok]

		return 1;
	}

	template<typename ... Args> int WithError (lua_State * L, const char * format, Args && ... args)
	{
		lua_pushfstring(L, format, CompatXS::forward<Args>(args)...);	// nil / false, err

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
        static_assert(CompatXS::NoThrowTraits<T>::is_default_constructible::value, "NewArray() type must be nothrow default-constructible");

		return static_cast<T *>(lua_newuserdata(L, n * sizeof(T)));	// ..., ud
	}

	template<typename T> T * AllocTyped (lua_State * L)
	{
		return static_cast<T *>(lua_newuserdata(L, sizeof(T)));	// ..., ud
	}

	template<typename T, typename ... Args> T * NewTyped (lua_State * L, Args && ... args)
	{
		T * instance = AllocTyped<T>(L);// ..., ud

		new (instance) T(CompatXS::forward<Args>(args)...);

		return instance;
	}

	template<typename T, typename ... Args> T * NewSizeTyped (lua_State * L, size_t size, Args && ... args)
	{
		if (size < sizeof(T)) luaL_error(L, "NewSizeTyped() called with insufficient size");

		T * instance = static_cast<T *>(lua_newuserdata(L, size));	// ..., ud

		new (instance) T(CompatXS::forward<Args>(args)...);

		return instance;
	}

	extern pthread_mutex_t symbols_mutex;

	template<typename T> size_t GenSym (lua_State * L, T & counter, std::vector<uint64_t> * cache = nullptr)
	{
		static_assert(CEU::is_integral<T>::value && CEU::is_unsigned<T>::value, "Counter must have unsigned integral type");

		lua_pushlightuserdata(L, &counter);	// ..., counter
		lua_rawget(L, LUA_REGISTRYINDEX);	// ..., index?

		uint64_t * index;

		if (!lua_isnil(L, -1)) index = UD<uint64_t>(L, -1);

		else
		{
			lua_pushlightuserdata(L, &counter);	// ..., nil, counter, index

			index = NewTyped<uint64_t>(L);	// ..., nil, index

			if (cache)
			{
				lua_createtable(L, 0, 1);	// ..., nil, index, mt
				lua_pushlightuserdata(L, cache);// ..., nil, index, mt, cache
				lua_pushcclosure(L, [](lua_State * L)
				{
					lua_pushvalue(L, lua_upvalueindex(1));	// index, cache
                    
                    pthread_mutex_lock(&symbols_mutex);
                    
					UD<std::vector<uint64_t>>(L, 2)->push_back(*UD<uint64_t>(L, 1));
                    
                    pthread_mutex_unlock(&symbols_mutex);

					return 0;
				}, 1);	// ..., nil, index, mt, gc
				lua_setfield(L, -2, "__gc");// ..., nil, index, mt = { __gc = gc }
				lua_setmetatable(L, -2);// ..., nil, index
			}

			lua_rawset(L, LUA_REGISTRYINDEX);	// ..., nil; registry = { ..., [counter] = index }

            pthread_mutex_lock(&symbols_mutex);

			if (cache && !cache->empty())
			{
				*index = cache->back();

				cache->pop_back();
			}

			else
			{
				*index = counter++;

				const uint64_t slice = (std::numeric_limits<uint64_t>::max)() / 65536ULL;	// Dole out 48-bit chunks (allows 64K clients)

				*index *= slice;
			}

            pthread_mutex_unlock(&symbols_mutex);
		}

		lua_pop(L, 1);	// ...

		return CEU::hash<uint64_t>{}(*index++);
	}

	template<typename T> bool BytesToValue (lua_State * L, int arg, T & value)
	{
		static_assert(CompatXS::TrivialTraits<T>::is_copyable::value, "BytesToValue() type must be trivially copyable");

		if (lua_type(L, arg) == LUA_TSTRING && lua_objlen(L, arg) == sizeof(T))
		{
			memcpy(&value, lua_tostring(L, arg), sizeof(T));

			return true;
		}

		return false;
	}

	template<typename T> void ValueToBytes (lua_State * L, const T & value)
	{
		static_assert(CompatXS::TrivialTraits<T>::is_copyable::value, "ValueToBytes() type must be trivially copyable");

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
		using opt_type = typename CompatXS::conditional<!CEU::is_arithmetic<T>::value || CEU::is_same<T, bool>::value,	// Is the type non-arithmetic (or bool)?
			T, // If so, use it as is.
			typename CompatXS::conditional<CEU::is_floating_point<T>::value,// Otherwise, is it a float?
				lua_Number, // If so, use numbers.
				typename CompatXS::conditional<CEU::is_same<T, long>::value, long,	// Failing that, we have either a long...
					typename CompatXS::conditional<CEU::is_unsigned<T>::value, unsigned int, int>::type	// ...or finally an int with a certain signedness.
				>::type
			>::type
		>::type;
		using ret_type = typename CompatXS::conditional<CEU::is_same<opt_type, unsigned int>::value, lua_Integer, opt_type>::type;

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
			return Add(name, CompatXS::forward<T &>(opt), opt);
		}

		//
		template<typename F, typename ... Args> Options & Call (const char * name, F && func, Args && ... args)
		{
			return WithFieldDo(name, [&](){
				func(mL, CompatXS::forward<Args>(args)...);
			});
		}
	};

    //
    struct Nil {};
    
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

    template<> inline void PushArgBody<lua_CFunction> (lua_State * L, lua_CFunction f) { lua_pushcfunction(L, f); }
	template<> inline void PushArgBody<lua_Integer> (lua_State * L, lua_Integer i) { lua_pushinteger(L, i); }
	template<> inline void PushArgBody<lua_Number> (lua_State * L, lua_Number n) { lua_pushnumber(L, n); }
    template<> inline void PushArgBody<const char *> (lua_State * L, const char * s) { lua_pushstring(L, s); }
    template<> inline void PushArgBody<void *> (lua_State * L, void * p) { lua_pushlightuserdata(L, p); }

    template<bool is_pointerlike> struct Pusher {
        template<typename U> void operator () (lua_State * L, U arg)
        {
            using arg_type = typename CompatXS::conditional<CEU::is_floating_point<U>::value,// ...or a float...
                lua_Number,
                typename CompatXS::conditional<CEU::is_integral<U>::value,	// ...or a non-boolean integer...
                    lua_Integer,
                    U	// Otherwise, use the raw type: boolean, null, stack index, this pointer, or user-specialized.
                >::type
            >::type;
            
            PushArgBody(L, static_cast<arg_type>(arg));
        }
    };
    
    template<> struct Pusher<true> {
        template<typename U> void operator () (lua_State * L, U arg)
        {
            using arg_type = typename CompatXS::conditional<CEU::is_same<U, const char *>::value || CEU::is_convertible<U, std::string>::value,	// Is it a string...
                const char *,
                typename CompatXS::conditional<CEU::is_same<U, lua_CFunction>::value || CEU::is_convertible<U, lua_CFunction>::value,	// ...or a Lua function...
                    lua_CFunction,
                    void *
                >::type
            >::type;
            
            PushArgBody<arg_type>(L, arg);
        }
    };
	template<typename U> static void PushArg (lua_State * L, U arg)
	{
        using T = typename CEU::remove_reference<U>::type;
        const bool is_pointerlike = CEU::is_pointer<T>::value || CEU::is_convertible<T, std::string>::value || CEU::is_convertible<T, lua_CFunction>::value;
/*		using arg_type = typename CompatXS::conditional<CEU::is_same<T, std::string>::value || CEU::is_convertible<T, std::string>::value,	// Is it a string...
			const char *,
			typename CompatXS::conditional<CEU::is_same<T, lua_CFunction>::value || CEU::is_convertible<T, lua_CFunction>::value,	// ...or a Lua function...
				lua_CFunction,
				typename CompatXS::conditional<CEU::is_pointer<T>::value,	// ...or a generic pointer...
                    void *,
					typename CompatXS::conditional<CEU::is_floating_point<T>::value,// ...or a float...
						lua_Number,
						typename CompatXS::conditional<CEU::is_integral<T>::value && !CEU::is_same<T, bool>::value,	// ...or a non-boolean integer...
							lua_Integer,
							T	// Otherwise, use the raw type: boolean, null, stack index, this pointer, or user-specialized.
						>::type
					>::type
				>::type
			>::type
		>::type;

 PushArgBody(L, arg_type(arg));*/
        Pusher<is_pointerlike>{}(L, arg);
    }
    
    template<> inline void PushArg<Nil> (lua_State * L, Nil) { lua_pushnil(L); }
    template<> inline void PushArg<bool> (lua_State * L, bool b) { lua_pushboolean(L, b ? 1 : 0); }
//    template<> inline void PushArg<const char *> (lua_State * L, const char * s) { lua_pushstring(L, s); }
    template<> inline void PushArg<const std::string &> (lua_State * L, const std::string & s) { lua_pushlstring(L, s.data(), s.length()); }
//    template<> inline void PushArg<lua_CFunction> (lua_State * L, lua_CFunction f) { lua_pushcfunction(L, f); }
    template<> inline void PushArg<StackIndex> (lua_State * L, StackIndex si) { lua_pushvalue(L, si); }
//    template<> inline void PushArg<const void *> (lua_State * L, const void * p) { lua_pushlightuserdata(L, const_cast<void *>(p)); }
    /*
    template<typename U> static void PushArg (lua_State * L, U * arg)
    {
        using T = typename CEU::remove_reference<U>::type;
        
        using arg_type = typename CompatXS::conditional<CEU::is_convertible<T, std::string>::value,	// Is it a string...
            const char *,
            typename CompatXS::conditional<CEU::is_same<T, lua_CFunction>::value || CEU::is_convertible<T, lua_CFunction>::value,	// ...or a Lua function...
                lua_CFunction,
                const void *
            >::type
        >::type;
        
        PushArg<arg_type>(L, arg);
    }*/
    
	template<typename T> int PushArgAndReturn (lua_State * L, T && arg)
	{
		PushArg(L, CompatXS::forward<T>(arg));

		return 1;
	}

    template<typename T> void PushMultipleArgs (lua_State * L, T && arg) { PushArg(L, CompatXS::forward<T>(arg)); }

	template<typename T, typename ... Args> void PushMultipleArgs (lua_State * L, T && arg, Args && ... args)
	{
		PushArg(L, CompatXS::forward<T>(arg));
		PushMultipleArgs(L, CompatXS::forward<Args>(args)...);
	}

	template<typename ... Args> int PushMultipleArgsAndReturn (lua_State * L, Args && ... args)
	{
		PushMultipleArgs(L, CompatXS::forward<Args>(args)...);

		return sizeof...(args);
	}

	template<typename T> void SetField (lua_State * L, int arg, const char * name, T && value)
	{
		arg = CoronaLuaNormalize(L, arg);

		PushArg(L, CompatXS::forward<T>(value));// ..., value

		lua_setfield(L, arg, name);	// ...
	}

	template<typename ... Args> bool PCall (lua_State * L, lua_CFunction func, Args && ... args)
    {
		PushMultipleArgs(L, func, CompatXS::forward<Args>(args)...);// ..., func, ...

		return lua_pcall(L, sizeof...(args), 0, 0) == 0;// ...[, err]
	}

    template<typename ... Args> bool PCallThis (lua_State * L, lua_CFunction func, void * pthis, Args && ... args)
    {
        PushMultipleArgs(L, func, pthis, CompatXS::forward<Args>(args)...);// ..., func, ...
        
        return lua_pcall(L, sizeof...(args) + 1, 0, 0) == 0;// ...[, err]
    }
    
	template<typename ... Args> bool PCallN (lua_State * L, lua_CFunction func, int nresults, Args && ... args)
	{
		PushMultipleArgs(L, func, CompatXS::forward<Args>(args)...);// ..., func, ...

		return lua_pcall(L, sizeof...(args), nresults, 0) == 0;	// ..., err / results
	}

	template<typename A1> inline bool CheckFuncArg (lua_State * L, A1 && arg1)
	{
		PushArg(L, CompatXS::forward<A1>(arg1));// ..., func

		if (lua_isfunction(L, -1)) return true;

		lua_pushfstring(L, "Argument is a %s, not a function", luaL_typename(L, -1));	// ..., func, err
		lua_replace(L, -2);	// ..., err

		return false;
	}

	template<typename A1, typename ... Args> bool PCall1 (lua_State * L, A1 && arg1, Args && ... args)
	{
		if (!CheckFuncArg(L, CompatXS::forward<A1>(arg1))) return false;// ..., func / err

		PushMultipleArgs(L, CompatXS::forward<Args>(args)...);	// ..., func, ...

		return lua_pcall(L, 1 + sizeof...(args), 0, 0) == 0;// ...[, err]
	}

	template<typename A1, typename ... Args> bool PCallN1 (lua_State * L, int nresults, A1 && arg1, Args && ... args)
	{
		if (!CheckFuncArg(L, CompatXS::forward<A1>(arg1))) return false;// ..., func / err

		PushMultipleArgs(L, CompatXS::forward<Args>(args)...);	// ..., func, ...

		return lua_pcall(L, 1 + sizeof...(args), nresults, 0) == 0;	// ...[, err]
	}
};