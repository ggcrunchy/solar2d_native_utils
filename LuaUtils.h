#ifndef LUA_UTILS_H
#define LUA_UTILS_H

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
void AttachGC (lua_State * L, const char * type, lua_CFunction gc);
void AttachMethods (lua_State * L, const char * type, void (*populate)(lua_State *));
void LoadClosureLibs (lua_State * L, luaL_Reg closures[], int n, const LuaAddParams & params = LuaAddParams());
void LoadFunctionLibs (lua_State * L, luaL_Reg funcs[], const LuaAddParams & params = LuaAddParams());

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

#endif