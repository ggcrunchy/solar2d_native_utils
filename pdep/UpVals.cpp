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

#include "UpVals.h"

// This and much of the code that follows come from the Pluto project, which is licensed
// under MIT in the Lua-derived parts and public domain otherwise
extern "C" {
	#include "pdep.h"
}

#define LIF(prefix, name) pdep ## _ ## name

/* A simple reimplementation of the unfortunately static function luaA_index.
 * Does not support the global table, registry, or upvalues. */
static StkId getobject(lua_State *L, int stackpos)
{
	if(stackpos > 0) {
		lua_assert(L->base+stackpos-1 < L->top);
		return L->base+stackpos-1;
	} else {
		lua_assert(L->top-stackpos >= L->base);
		return L->top+stackpos;
	}
}

static UpVal *toupval (lua_State *L, int stackpos)
{
	lua_assert(ttype(getobject(L, stackpos)) == LUA_TUPVAL);
	return gco2uv(getobject(L, stackpos)->value.gc);
}

#define setuvvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUPVAL; \
    checkliveness(G(L),i_o); }

static void pushupval (lua_State *L, UpVal *upval)
{
	TValue o;
	setuvvalue(L, &o, upval);
	LIF(A,pushobject)(L, &o);
}

static void pushclosure (lua_State *L, Closure *closure)
{
	TValue o;
	setclvalue(L, &o, closure);
	LIF(A,pushobject)(L, &o);
}

extern "C" void AssignDummyUpvalues (lua_State * L, int arg, int nups)
{
	arg = CoronaLuaNormalize(L, arg);

	Closure * cl = clvalue(getobject(L, arg));
	Proto * p = cl->l.p;

	if (p->sizeupvalues == 0)
	{
		p->sizeupvalues = nups;

		LIF(M,reallocvector)(L, p->upvalues, 0, p->sizeupvalues, TString *);

		lua_pushliteral(L, "");	// ..., ""

		for (int i = 0; i < nups; ++i) p->upvalues[i] = pdep_newlstr(L, "", 0);

		lua_pop(L, 1);	// ...
	}
}

extern "C" int CountUpvalues (lua_State * L, int arg)
{
	Closure * cl = clvalue(getobject(L, arg));

	return (int)cl->l.nupvalues;
}

extern "C" int GetUpvalue (lua_State * L, int arg, int upvalue)
{
	Closure * cl = clvalue(getobject(L, arg));

	if (upvalue > cl->l.nupvalues) return 0;
#if 0
	pushupval(L, cl->l.upvals[upvalue - 1]);

	UpVal * uv = toupval(L, -1);

	lua_checkstack(L, 1);

	/* We can't permit the upval to linger around on the stack, as Lua
	* will bail if its GC finds it. */

	lua_pop(L, 1);
					/* perms reftbl ... */
#endif
	LIF(A,pushobject)(L, cl->l.upvals[upvalue - 1]->v);//uv->v);

	return 1;
}

static UpVal *makeupval (lua_State *L, int stackpos)
{
	UpVal *uv = pdep_new(L, UpVal);
	pdep_link(L, (GCObject*)uv, LUA_TUPVAL);
	uv->tt = LUA_TUPVAL;
	uv->v = &uv->u.value;
	uv->u.l.prev = NULL;
	uv->u.l.next = NULL;
	setobj(L, uv->v, getobject(L, stackpos));
	return uv;
}

static Proto *makefakeproto (lua_State *L, lu_byte nups)
{
	Proto *p = pdep_newproto(L);
	p->sizelineinfo = 1;
	p->lineinfo = pdep_newvector(L, 1, int);
	p->lineinfo[0] = 1;
	p->sizecode = 1;
	p->code = pdep_newvector(L, 1, Instruction);
	p->code[0] = CREATE_ABC(OP_RETURN, 0, 1, 0);
	p->source = pdep_newlstr(L, "", 0);
	p->maxstacksize = 2;
	p->nups = nups;
	p->sizek = 0;
	p->sizep = 0;

	return p;
}

/* The GC is not fond of finding upvalues in tables. We get around this
 * during persistence using a weakly keyed table, so that the GC doesn't
 * bother to mark them. This won't work in unpersisting, however, since
 * if we make the values weak they'll be collected (since nothing else
 * references them). Our solution, during unpersisting, is to represent
 * upvalues as dummy functions, each with one upvalue. */
static void boxupval_start(lua_State *L)
{
	LClosure *lcl;
	lcl = (LClosure*)pdep_newLclosure(L, 1, hvalue(&L->l_gt));
	pushclosure(L, (Closure*)lcl);
					/* ... func */
	lcl->p = makefakeproto(L, 1);

	/* Temporarily initialize the upvalue to nil */

	lua_pushnil(L);
	lcl->upvals[0] = makeupval(L, -1);
	lua_pop(L, 1);
}

static void boxupval_finish(lua_State *L)
{
					/* ... func obj */
	LClosure *lcl = (LClosure *) clvalue(getobject(L, -2));

	lcl->upvals[0]->u.value = *getobject(L, -1);
	lua_pop(L, 1);
}

static void unboxupval (lua_State *L)
{
					/* ... func */
	LClosure *lcl;
	UpVal *uv;

	lcl = (LClosure*)clvalue(getobject(L, -1));
	uv = lcl->upvals[0];
	lua_pop(L, 1);
					/* ... */
	pushupval(L, uv);
					/* ... upval */
}

extern "C" int SetUpvalue (lua_State * L, int arg, int upvalue)
{
	LClosure * lcl = (LClosure*)clvalue(getobject(L, arg));

	if (upvalue > lcl->nupvalues) return 0;

	int v = CoronaLuaNormalize(L, -1);

	lua_checkstack(L, 2);

	boxupval_start(L);
					/* perms reftbl ... func */

	lua_pushvalue(L, v);

					/* perms reftbl ... func obj */
	boxupval_finish(L);

	unboxupval(L);
					/* perms reftbl ... func upval */
	lcl->upvals[upvalue - 1] = toupval(L, -1);

	lua_pop(L, 2); // one from Pluto, one for object

	return 1;
}