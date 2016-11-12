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

struct LuaAddParams {
	int mFirstPos;
	int mTablePos;
	bool mLeaveTableAtTop;

	LuaAddParams (int tpos = 0, int first = 0) : mFirstPos(first), mTablePos(tpos), mLeaveTableAtTop(true)
	{
	}
};

bool IsMainState (lua_State * L);
bool IsType (lua_State * L, const char * name, int index = -1);
bool IsType (lua_State * L, const char * name, const char * alt, int index = -1);

void AddClosures (lua_State * L, luaL_Reg closures[], int n, const LuaAddParams & params = LuaAddParams());
void AttachGC (lua_State * L, lua_CFunction gc);
void AttachGC (lua_State * L, const char * type, lua_CFunction gc);
void AttachMethods (lua_State * L, const char * type, void (*populate)(lua_State *));
void LoadClosureLibs (lua_State * L, luaL_Reg closures[], int n, const LuaAddParams & params = LuaAddParams());
void LoadFunctionLibs (lua_State * L, luaL_Reg funcs[], const LuaAddParams & params = LuaAddParams());
void NewWeakKeyedTable (lua_State * L);

template<typename T> int ArrayN (lua_State * L, int arg = 1)
{
	return int(lua_objlen(L, arg) / sizeof(T));
}

template<typename T> void AttachTypedGC (lua_State * L, const char * type)
{
	AttachGC(L, type, [](lua_State * L)
	{
		((T *)lua_touserdata(L, 1))->~T();

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

struct LuaOptions {
	lua_State * mL;	// Current state
	int mArg;	// Stack position

	LuaOptions (lua_State * L, int arg);

	LuaOptions & Add (const char * name, int & opt);
	LuaOptions & Add (const char * name, float & opt);
	LuaOptions & Add (const char * name, double & opt);
	LuaOptions & Add (const char * name, bool & opt);
	LuaOptions & Add (const char * name, size_t & opt);
	LuaOptions & Add (const char * name, int & opt, int def);
	LuaOptions & Add (const char * name, float & opt, float def);
	LuaOptions & Add (const char * name, double & opt, double def);
	LuaOptions & Add (const char * name, bool & opt, bool def);
	LuaOptions & Add (const char * name, size_t & opt, size_t def);
};