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

// https://www.lua.org/source/5.1/lobject.h.html
#define iscollectable(o)        (ttype(o) >= LUA_TSTRING)

// https://www.lua.org/source/5.1/lstate.h.html
/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)      (cast(GCObject *, (v)))

// https://www.lua.org/source/5.1/lgc.h.html

#define maskmarks       cast_byte(~(bitmask(BLACKBIT)|WHITEBITS))

#define makewhite(g,x)  \
   ((x)->gch.marked = cast_byte(((x)->gch.marked & maskmarks) | luaC_white(g)))

#define iswhite(x)      test2bits((x)->gch.marked, WHITE0BIT, WHITE1BIT)
#define isblack(x)      testbit((x)->gch.marked, BLACKBIT)
#define isgray(x)       (!isblack(x) && !iswhite(x))

#define otherwhite(g)   (g->currentwhite ^ WHITEBITS)
#define isdead(g,v)     ((v)->gch.marked & otherwhite(g) & WHITEBITS)

#define changewhite(x)  ((x)->gch.marked ^= WHITEBITS)
#define gray2black(x)   l_setbit((x)->gch.marked, BLACKBIT)

#define valiswhite(x)   (iscollectable(x) && iswhite(gcvalue(x)))

#define luaC_white(g)   cast(lu_byte, (g)->currentwhite & WHITEBITS)

#define luaC_barrier(L,p,v) { if (valiswhite(v) && isblack(obj2gco(p)))  \
        luaC_barrierf(L,obj2gco(p),gcvalue(v)); }

// https://www.lua.org/source/5.1/lgc.c.html

#define white2gray(x)   reset2bits((x)->gch.marked, WHITE0BIT, WHITE1BIT)

#define markvalue(g,o) { checkconsistency(o); \
  if (iscollectable(o) && iswhite(gcvalue(o))) reallymarkobject(g,gcvalue(o)); }

#define markobject(g,t) { if (iswhite(obj2gco(t))) \
                reallymarkobject(g, obj2gco(t)); }


#define setthreshold(g)  (g->GCthreshold = (g->estimate/100) * g->gcpause)


static void reallymarkobject (global_State *g, GCObject *o) {
  lua_assert(iswhite(o) && !isdead(g, o));
  white2gray(o);
  switch (o->gch.tt) {
    case LUA_TSTRING: {
      return;
    }
    case LUA_TUSERDATA: {
      Table *mt = gco2u(o)->metatable;
      gray2black(o);  /* udata are never gray */
      if (mt) markobject(g, mt);
      markobject(g, gco2u(o)->env);
      return;
    }
    case LUA_TUPVAL: {
      UpVal *uv = gco2uv(o);
      markvalue(g, uv->v);
      if (uv->v == &uv->u.value)  /* closed? */
        gray2black(o);  /* open upvalues are never black */
      return;
    }
    case LUA_TFUNCTION: {
      gco2cl(o)->c.gclist = g->gray;
      g->gray = o;
      break;
    }
    case LUA_TTABLE: {
      gco2h(o)->gclist = g->gray;
      g->gray = o;
      break;
    }
    case LUA_TTHREAD: {
      gco2th(o)->gclist = g->gray;
      g->gray = o;
      break;
    }
    case LUA_TPROTO: {
      gco2p(o)->gclist = g->gray;
      g->gray = o;
      break;
    }
    default: lua_assert(0);
  }
}

static void luaC_barrierf (lua_State *L, GCObject *o, GCObject *v) {
  global_State *g = G(L);
  lua_assert(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o));
  lua_assert(g->gcstate != GCSfinalize && g->gcstate != GCSpause);
  lua_assert(ttype(&o->gch) != LUA_TTABLE);
  /* must keep invariant? */
  if (g->gcstate == GCSpropagate)
    reallymarkobject(g, v);  /* restore invariant */
  else  /* don't mind */
    makewhite(g, o);  /* mark as white just to avoid other barriers */
}

// https://www.lua.org/source/5.1/lapi.c.html

#define api_checknelems(L, n)   api_check(L, (n) <= (L->top - L->base))

//#define api_checkvalidindex(L, i)       api_check(L, (i) != luaO_nilobject)

#define api_incr_top(L)   {api_check(L, L->top < L->ci->top); L->top++;}



static TValue *index2adr (lua_State *L, int idx) {
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    api_check(L, idx <= L->ci->top - L->base);
// STEVE CHANGE
/*  if (o >= L->top) return cast(TValue *, luaO_nilobject);
    else */return o;
// /STEVE CHANGE
  }
  else if (idx > LUA_REGISTRYINDEX) {
    api_check(L, idx != 0 && -idx <= L->top - L->base);
    return L->top + idx;
  }
  else switch (idx) {  /* pseudo-indices */
    case LUA_REGISTRYINDEX: return registry(L);
    case LUA_ENVIRONINDEX: {
      Closure *func = curr_func(L);
      sethvalue(L, &L->env, func->c.env);
      return &L->env;
    }
    case LUA_GLOBALSINDEX: return gt(L);
    default: {
      Closure *func = curr_func(L);
      idx = LUA_GLOBALSINDEX - idx;
      return (idx <= func->c.nupvalues)
                ? &func->c.upvalue[idx-1]
                : cast(TValue *, 0xDEADBEEF);//luaO_nilobject); <- STEVE CHANGE
    }
  }
}

static const char *aux_upvalue (StkId fi, int n, TValue **val) {
  Closure *f;
  if (!ttisfunction(fi)) return NULL;
  f = clvalue(fi);
  if (f->c.isC) {
    if (!(1 <= n && n <= f->c.nupvalues)) return NULL;
    *val = &f->c.upvalue[n-1];
    return "";
  }
  else {
    Proto *p = f->l.p;
	const char * str; // <- STEVE CHANGE
    if (!(1 <= n && n <= /*p->*sizeupvalues*/f->l.nupvalues)) return NULL; // <- STEVE CHANGE
    *val = f->l.upvals[n-1]->v;
    str = p->sizeupvalues ? getstr(p->upvalues[n-1]) : NULL; // <- STEVE CHANGE
	return str ? str : ""; // <- STEVE CHANGE
  }
}

extern "C" const char * GetUpvalueOK (lua_State * L, int funcindex, int n)
{
  const char *name;
  TValue *val;
  if (funcindex > lua_gettop(L)) return NULL; // <- STEVE CHANGE
  lua_lock(L);
  name = aux_upvalue(index2adr(L, funcindex), n, &val);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}

extern "C" const char * SetUpvalueOK (lua_State * L, int funcindex, int n)
{
  const char *name;
  TValue *val;
  StkId fi;
  if (funcindex > lua_gettop(L)) return NULL; // <- STEVE CHANGE
  lua_lock(L);
  fi = index2adr(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val);
  if (name) {
    L->top--;
    setobj(L, val, L->top);
    luaC_barrier(L, clvalue(fi), L->top);
  }
  lua_unlock(L);
  return name;
}