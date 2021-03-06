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

#include "utils/LuaEx.h"
#include <vector>

CEU_BEGIN_NAMESPACE(LuaXS) {
	std::mutex symbols_mutex;

	bool IsMainState (lua_State * L)
	{
		int top = lua_gettop(L);
		const char * levels[] = { "package", "loaded", "luaproc", "is_main_state" };

		for (int i = 0; i < sizeof(levels) / sizeof(const char *); ++i)
		{
			int index = i > 0 ? -1 : LUA_GLOBALSINDEX;

			if (!lua_istable(L, index))
			{
				if (i < 3) printf("globals, package, or package.loaded not a table\n");

				lua_settop(L, top);	// ...

				return true;// main state if luaproc not loaded, or best guess in case of error
			}

			lua_getfield(L, index, levels[i]);	// ..., t[key]
		}

		bool bOK = lua_isfunction(L, -1) != 0 && lua_pcall(L, 0, 1, 0) == 0;

		if (!bOK) fprintf(stderr, "luaproc.is_main_state() failed\n");

		bool bMainState = !bOK || lua_toboolean(L, -1) != 0; // if something went wrong, main state as good a guess as any

		lua_settop(L, top);	// ...

		return bMainState;
	}

	static bool AuxEq (lua_State * L, const char * name)
	{
		luaL_getmetatable(L, name);	// ..., object_mt, named_mt

		return lua_equal(L, -2, -1) != 0;
	}

	bool IsType (lua_State * L, const char * name, int index)
	{
		if (!lua_getmetatable(L, index)) return false;	// ..., object_mt

		bool eq = AuxEq(L, name); // ..., object_mt, named_mt

		lua_pop(L, 2);	// ...

		return eq;
	}

	bool IsType (lua_State * L, const char * name, const char * alt, int index)
	{
		if (!lua_getmetatable(L, index)) return false;	// ..., object_mt

		bool eq = AuxEq(L, name);	// object_mt, named_mt

		if (!eq) 
		{
			lua_pop(L, 1);	// object_mt

			eq = AuxEq(L, alt);	// object_mt, alt_mt
		}

		lua_pop(L, 2);	// ...

		return eq;
	}

	static int Index (lua_State * L, int index)
	{
		if (index <= LUA_REGISTRYINDEX) return index;

		else return CoronaLuaNormalize(L, index);
	}

	static int GetTablePos (lua_State * L, const AddParams & params, int top)
	{
		int tpos = Index(L, params.mTablePos);

		luaL_argcheck(L, tpos < 0 || (tpos >= 1 && tpos <= top), top, "Table outside stack");

		return tpos;
	}

	static void AuxClosure (lua_State * L, int n, const AddParams & params, int & first, int & tpos)
	{
		int top = lua_gettop(L);

		if (n <= 0) luaL_error(L, "%d upvalues supplied to closures", n);

		//
		if (params.mFirstPos == 0) first = top - n + 1;

		else
		{
			first = Index(L, params.mFirstPos);

			luaL_argcheck(L, first < 0 || (first >= 1 && first + n <= top + 1), first, "Interval not within stack");
		}

		//
		tpos = params.mTablePos == 0 ? top - n : GetTablePos(L, params, top);
	}

	void AddClosures (lua_State * L, luaL_Reg closures[], int n, const AddParams & params)
	{
		int first, tpos;

		AuxClosure(L, n, params, first, tpos);

		for (int i = 0; closures[i].func; ++i)
		{
			for (int j = 0; j < n; ++j) lua_pushvalue(L, first + j);// ..., values

			lua_pushcclosure(L, closures[i].func, n);	// ..., func
			lua_setfield(L, tpos, closures[i].name);// ..., t = { name = func }
		}

		if (params.mLeaveTableAtTop) lua_settop(L, tpos);	// ..., t
	}

	void AddCloseLogic (lua_State * L, lua_CFunction func, int nupvalues)
	{
		lua_newuserdata(L, 0U);	// ..., (upvalues), dummy
		lua_insert(L, -(nupvalues + 1));// ..., dummy, (upvalues)

		LuaXS::AttachGC(L, func, nupvalues);// ..., dummy

		lua_pushboolean(L, 1);	// ..., dummy, true
		lua_rawset(L, LUA_REGISTRYINDEX);	// ...; registry = { [dummy] = true }
	}

	void AddRuntimeListener (lua_State * L, const char * name)
	{
		CoronaLuaPushRuntime(L);// ..., func, Runtime

		lua_getfield(L, -1, "addEventListener");// ..., func, Runtime, Runtime.addEventListener
		lua_insert(L, -2);	// ..., func, Runtime.addEventListener, Runtime
		lua_pushstring(L, name);// ..., func, Runtime.addEventListener, Runtime, name
		lua_pushvalue(L, -4);	// ..., func, Runtime.addEventListener, Runtime, name, func
		lua_remove(L, -5);	// ..., Runtime.addEventListener, Runtime, name, func
		lua_call(L, 3, 0);	// ...
	}

	void AddRuntimeListener (lua_State * L, const char * name, lua_CFunction func, int nupvalues)
	{
		lua_pushcclosure(L, func, nupvalues);	// ..., func

		AddRuntimeListener(L, name);// ...
	}

	void AttachGC (lua_State * L, lua_CFunction gc, int nupvalues)
	{
		lua_pushcclosure(L, gc, nupvalues);	// ... ud, gc
		lua_newtable(L);// ..., ud, gc, mt
		lua_insert(L, -2);	// ..., ud, mt, gc
		lua_setfield(L, -2, "__gc");// ..., ud, mt = { __gc = gc }
		lua_setmetatable(L, -2);// ..., ud
	}

	void AttachGC (lua_State * L, const char * type, lua_CFunction gc)
	{
		if (luaL_newmetatable(L, type))	// ..., ud, meta
		{
			luaL_Reg gc_func[] = {
				{
					"__gc", gc
				},
				{ nullptr, nullptr }
			};

			luaL_register(L, nullptr, gc_func);
		}

		lua_setmetatable(L, -2);// ..., ud
	}

	void AttachMethods (lua_State * L, const char * type, void (*populate)(lua_State * L))
	{
		if (luaL_newmetatable(L, type))	// ..., ud, meta
		{
			lua_pushvalue(L, -1);	// ..., ud, meta, meta
			lua_setfield(L, -2, "__index");	// ..., ud, meta = { __index = meta }

			int top = lua_gettop(L);

			populate(L);

			lua_settop(L, top);	// ..., ud, meta
		}

		lua_setmetatable(L, -2);// ..., ud
	}

	void AttachProperties (lua_State * L, lua_CFunction get_props, const AttachPropertyParams & params)
	{
		lua_pushcclosure(L, get_props, params.mUpvalueCount);	// ..., meta, get_props
		lua_getfield(L, -2, "__index");	// ..., meta, get_props, index

		// Build up a list of entries that may be null, since these would otherwise be
		// interpreted as missing properties when nil.
		if (params.mNullable)
		{
			lua_newtable(L);// ..., meta, index, nullable

			for (int i = 0; params.mNullable[i]; ++i)
			{
				lua_pushstring(L, params.mNullable[i]);	// ..., meta, get_props, index, nullable, entry
				lua_rawseti(L, -2, i + 1);	// ..., meta, get_props, index, nullable = { ..., entry }
			}
		}

		else lua_pushnil(L);// ..., meta, get_props, index, nil

		// Chain the property getter.
		lua_pushcclosure(L, [](lua_State * L)
		{
			// Try to get the property first.
			lua_pushvalue(L, lua_upvalueindex(1));	// ud, key, get_props
			lua_pushvalue(L, 1);// ud, key, get_props, ud
			lua_pushvalue(L, 2);// ud, key, get_props, ud, key
			lua_call(L, 2, 1);	// ud, key, value?

			if (!lua_isnil(L, 3)) return 1;

			// Failing that, see if the property is allowed to be nil.
			lua_pushvalue(L, lua_upvalueindex(3));	// ud, key, nil, nullable

			if (Find(L, 4, 2)) return PushArgAndReturn(L, Nil{});	// ud, key, nil, nullable, nil

			// Finally, go to the next getter in the chain.
			lua_pop(L, 2);	// ud, key
			lua_pushvalue(L, lua_upvalueindex(2));	// ud, key, index
			lua_insert(L, 2);	// ud, index, key

			if (lua_istable(L, -2)) lua_rawget(L, -2);	// ud, index, value?

			else lua_call(L, 1, 1);	// ud, value?

			return 1;
		}, 3);	// ... meta, Index
		lua_setfield(L, -2, "__index");	// ..., meta = { ..., __index = Index }
	}

	void CallInMainState (lua_State * L, lua_CFunction func, void * ud)
	{
		bool bOK;

		if (IsMainState(L)) bOK = lua_cpcall(L, func, ud) == 0;	// ...[, err]

		else
		{
			lua_getfield(L, LUA_REGISTRYINDEX, "LUAPROC_CALLER_FUNC");	// ..., caller?
			luaL_checktype(L, -1, LUA_TFUNCTION);

			lua_CFunction * pfunc = LuaXS::NewSizeTypedExtra<lua_CFunction>(L, ud ? sizeof(void *) : 0U);	// ..., caller, func

			*pfunc = func;

			if (ud) *reinterpret_cast<void **>(&pfunc[1]) = ud;

			bOK = lua_pcall(L, 1, 0, 0) == 0;	// ...[, err]
		}

		if (!bOK) lua_error(L);
	}

	void LoadClosureLibs (lua_State * L, luaL_Reg closures[], int n, const AddParams & params)
	{
		int first, tpos;

		AuxClosure(L, n, params, first, tpos);

		for (int i = 0; closures[i].func; ++i)
		{
			for (int j = 0; j < n; ++j) lua_pushvalue(L, first + j);// ..., values

			lua_pushcclosure(L, closures[i].func, 1);	// ..., func
			lua_call(L, 0, 1);	// ..., lib
			lua_setfield(L, tpos, closures[i].name);// ..., t = { name = lib }
		}

		if (params.mLeaveTableAtTop) lua_settop(L, tpos);	// ..., t
	}

	void LoadFunctionLibs (lua_State * L, luaL_Reg funcs[], const AddParams & params)
	{
		int tpos = GetTablePos(L, params, lua_gettop(L));

		for (int i = 0; funcs[i].func; ++i)
		{
			lua_pushcfunction(L, funcs[i].func);// ..., func
			lua_call(L, 0, 1);	// ..., lib
			lua_setfield(L, tpos, funcs[i].name);	// ..., t = { name = lib }
		}

		if (params.mLeaveTableAtTop) lua_settop(L, tpos);	// ..., t
	}

	void NewWeakKeyedTable (lua_State * L)
	{
		lua_newtable(L);// ..., t
		lua_createtable(L, 0, 1);	// ..., t, mt
		lua_pushliteral(L, "k");// ..., t, mt, "k"
		lua_setfield(L, -2, "__mode");	// ..., t, mt = { __mode = "k" }
		lua_setmetatable(L, -2);// ..., t
	}

	int BoolResult (lua_State * L, int b)
	{
		lua_pushboolean(L, b);	// ..., b

		return 1;
	}

	int ErrorAfterFalse (lua_State * L)
	{
		return ErrorAfter(L, false);// ..., false, err
	}

	int ErrorAfterNil (lua_State * L)
	{
		return ErrorAfter(L, Nil{});// ..., nil, err
	}

	int NoOp (lua_State * L)
	{
		return 0;
	}

	size_t Find (lua_State * L, int t, int item)
	{
		size_t index = 0;

		if (lua_istable(L, t))
		{
			item = CoronaLuaNormalize(L, item);

			for (size_t cur : Range(L, t))
			{
				if (lua_equal(L, item, -1))
				{
					index = cur;

					break;
				}
			}
		}

		return index;
	}

	void LibEntry::MoveIntoArray (lua_State * L, int arr)
	{
		arr = CoronaLuaNormalize(L, arr);

		TransferAndPush(L);	// ..., t, ... ud / nil

		lua_rawseti(L, arr, int(lua_objlen(L, arr)) + 1);   // ..., t = { ..., ud }, ...
	}

	void LibEntry::TransferAndPush (lua_State * L)
	{
		if (*mLib)
		{
			void ** ud = (void **)lua_newuserdata(L, sizeof(const void *));	// ..., ud

			*ud = *mLib;
			*mLib = nullptr;
		}

		else lua_pushnil(L);// ..., nil
	}

	LibEntry FindLib (lua_State * L, const char * name, size_t len)
	{
		for (lua_pushnil(L); lua_next(L, LUA_REGISTRYINDEX); lua_pop(L, 1))
		{
			if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TUSERDATA)
			{
				const char * key = lua_tostring(L, -2);

				if (strncmp(key, "LOADLIB: ", sizeof("LOADLIB: ") - 1) == 0)
				{
					const char * pre_ext = strrchr(key, '.'), * sep = pre_ext;

					if (!sep) continue;

					do {
						--sep;
					} while (sep != key && *sep != '\\' && *sep != '/' && *sep != '_');

					if (sep == key || pre_ext - (sep + 1) != len) continue;

                    #ifdef __ANDROID__
                        #define PLUGIN_PREFIX "libplugin"
                    #else
                        #define PLUGIN_PREFIX "plugin"
                    #endif
                    
                    
					if (strncmp(sep - sizeof(PLUGIN_PREFIX) + 1, PLUGIN_PREFIX, sizeof(PLUGIN_PREFIX) - 1) == 0 && strncmp(sep + 1, name, len) == 0)
					{
						LibEntry entry;

						entry.mPath = key + sizeof("LOADLIB: ") - 1;
						entry.mLib = (void **)lua_touserdata(L, -1);

						return entry;
					}
                    
                    #undef PLUGIN_PREFIX
				}
			}
		}

		return LibEntry{};
	}

	static void GetLibGC (lua_State * L)
	{
		luaL_getmetatable(L, "_LOADLIB");	// ..., mt
		lua_getfield(L, -1, "__gc");// ..., mt, gc
	}

	void CleanUpArrayOfLibs (lua_State * L, int arr)
	{
		arr = CoronaLuaNormalize(L, arr);

		GetLibGC(L);// ..., mt, gc

		lua_getref(L, arr);// ..., mt, gc, arr

		for (int i = 1, n = int(lua_objlen(L, -1)); i <= n; ++i, lua_pop(L, 1))
		{
			lua_rawgeti(L, -1, i);	// ..., mt, gc, arr, lib
			lua_pushvalue(L, -4);	// ..., mt, gc, arr, lib, mt
			lua_setmetatable(L, -2);// ..., mt, gc, arr, lib
		}
		// ^^^ TODO: did this serve any purpose?

		for (int i = 1, n = int(lua_objlen(L, -1)); i <= n; ++i, lua_pop(L, 1))
		{
			lua_rawgeti(L, -1, i);	// ..., mt, gc, arr, lib
			lua_pushvalue(L, -3);	// ..., mt, gc, arr, lib, gc
			lua_pushvalue(L, -2);	// ..., mt, gc, arr, lib, gc, lib
			lua_pcall(L, 1, 0, 0);	// ..., mt, gc, arr, lib
			lua_pushnil(L);	// ..., mt, gc, arr, lib, nil
			lua_setmetatable(L, -2);// ..., mt, gc, arr, lib
		}

		lua_pop(L, 3);	// ...
	}

	void CleanUpLib (lua_State * L, int pos)
	{
		if (lua_isnoneornil(L, pos)) return;

		GetLibGC(L);// ..., mt, gc

		lua_pushvalue(L, pos);	// ..., mt, gc, lib
		lua_pcall(L, 1, 0, 0);	// ..., mt
		lua_pop(L, 1);	// ...
	}

	Range::Iter & Range::Iter::operator ++ (void)
	{
		++mIndex;

		lua_settop(mParent.mL, mParent.mTop);	// ...

		return *this;
	}

	Range::Iter & Range::Iter::operator * (void)
	{
		if (mParent.mIsTable) lua_rawgeti(mParent.mL, mParent.mArg, mIndex + 1);// ..., elem

		else lua_pushvalue(mParent.mL, mParent.mArg);	// ..., elem

		return *this;
	}

	Range::Iter::operator int (void)
	{
		return mIndex + 1;
	}

	int Range::Iter::ReturnFrom (int n)
	{
		mParent.mTop = lua_gettop(mParent.mL);

		return n;
	}

	Range::Iter Range::begin (void)
	{
		if (mIsTable || mNonTableOK)
		{
			Iter ei{*this};

			ei.mIndex = 0;

			return ei;
		}
		
		else return end();
	}

	Range::Iter Range::end (void)
	{
		Iter ei{*this};

		if (mIsTable) ei.mIndex = int(lua_objlen(mL, mArg));

		else ei.mIndex = mNonTableOK ? 1 : 0;

		return ei;
	}

	Range::Range (lua_State * L, int arg, bool bIterOnceIfNonTable) : mL{L}
	{
		mArg = CoronaLuaNormalize(L, arg);
		mTop = lua_gettop(L);
		mIsTable = lua_istable(L, arg);
		mNonTableOK = bIterOnceIfNonTable;
	}

	Range::~Range (void)
	{
		lua_settop(mL, mTop);	// ...
	}

	bool Bool (lua_State * L, int arg) { return GetArg<bool>(L, arg); }
	float Float (lua_State * L, int arg) { return GetArg<float>(L, arg); }
	double Double (lua_State * L, int arg) { return GetArg<double>(L, arg); }
	int Int (lua_State * L, int arg) { return GetArg<int>(L, arg); }
	long Long (lua_State * L, int arg) { return GetArg<long>(L, arg); }
	unsigned int Uint (lua_State * L, int arg) { return GetArg<unsigned int>(L, arg); }
	const char * String (lua_State * L, int arg) { return GetArg<const char *>(L, arg); }
	void * Userdata (lua_State * L, int arg) { return GetArg<void *>(L, arg); }

	Options::Options (lua_State * L, int arg) : mL{L}, mArg{0}
	{
		if (lua_istable(L, arg)) mArg = CoronaLuaNormalize(L, arg);
	}

	Options & Options::Add (const char * name, bool & opt)
	{
		return Add(name, opt, false);
	}

	Options & Options::ArgCheck (bool bOK, const char * message)
	{
		if (mArg) luaL_argcheck(mL, bOK, mArg, message);

		return *this;
	}

	void Options::Replace (const char * field)
	{
		if (mArg)
		{
			lua_getfield(mL, mArg, field);// ..., opts, ..., value
			lua_replace(mL, mArg);	// ..., value, ...
		}
	}

	bool PCallWithStack (lua_State * L, lua_CFunction func, int nresults)
	{
		lua_pushcfunction(L, func);	// ..., func
		lua_insert(L, 1);	// func, ...
		
		return lua_pcall(L, lua_gettop(L) - 1, nresults, 0) == 0;	// ..., ok[, results / err]
	}

	bool PCallWithStackAndUpvalues (lua_State * L, lua_CFunction func, int nupvalues, int nresults)
	{
		for (int i = 1; i <= nupvalues; ++i) lua_pushvalue(L, lua_upvalueindex(i));	// ..., upvalues

		lua_pushcclosure(L, func, nupvalues);	// ..., func
		lua_insert(L, 1);	// func, ...
		
		return lua_pcall(L, lua_gettop(L) - 1, nresults, 0) == 0;	// ..., ok[, results / err]
	}
CEU_CLOSE_NAMESPACE()
