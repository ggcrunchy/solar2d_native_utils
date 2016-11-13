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

namespace LuaXS {
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

		if (!bOK) printf("luaproc.is_main_state() failed\n");

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

		if (n <= 0) luaL_error(L, "%i upvalues supplied to closures", n);

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

	void AttachGC (lua_State * L, lua_CFunction gc)
	{
		lua_newtable(L);// ..., ud, mt
		lua_pushcfunction(L, gc);	// ..., ud, mt, gc
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

	Options::Options (lua_State * L, int arg) : mL(L), mArg(0)
	{
		if (lua_istable(L, arg)) mArg = CoronaLuaNormalize(L, arg);
	}

	Options & Options::Add (const char * name, const char *& opt)
	{
		if (mArg)
		{
			lua_getfield(mL, mArg, name);	// ..., str

			if (!lua_isnil(mL, -1)) opt = luaL_checkstring(mL, -1);

			lua_pop(mL, 1);	// ...
		}

		return *this;
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
}