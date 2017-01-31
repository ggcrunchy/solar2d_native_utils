#pragma once

#include "CoronaLua.h"

#ifdef __cplusplus
extern "C" {
#endif
	void GetUpvalue (lua_State * L, int arg, int upvalue);
	void SetUpvalue (lua_State * L, int arg, int upvalue);
#ifdef __cplusplus
}
#endif