#include "userdata.h"

extern "C" {
#include "lj_obj.h"
#include "lauxlib.h"
}


struct ProxyLuaFunctionUserData {
	GCfuncL *fn;
};
struct ProxyUserData {
	GarrysMod::Lua::UserData ud;
	GCudata *proxy_data;
};

lua_State *GetClientState();

#define api_checknelems(L, n)		api_check(L, (n) <= (L->top - L->base))
#define api_checkvalidindex(L, i)	api_check(L, (i) != niltv(L))

static TValue *index2adr(lua_State *L, int idx)
{
	if (idx > 0) {
		TValue *o = L->base + (idx - 1);
		return o < L->top ? o : niltv(L);
	}
	else if (idx > LUA_REGISTRYINDEX) {
		api_check(L, idx != 0 && -idx <= L->top - L->base);
		return L->top + idx;
	}
	else if (idx == LUA_GLOBALSINDEX) {
		TValue *o = &G(L)->tmptv;
		settabV(L, o, tabref(L->env));
		return o;
	}
	else if (idx == LUA_REGISTRYINDEX) {
		return registry(L);
	}
	else {
		GCfunc *fn = curr_func(L);
		api_check(L, fn->c.gct == ~LJ_TFUNC && !isluafunc(fn));
		if (idx == LUA_ENVIRONINDEX) {
			TValue *o = &G(L)->tmptv;
			settabV(L, o, tabref(fn->c.env));
			return o;
		}
		else {
			idx = LUA_GLOBALSINDEX - idx;
			return idx <= fn->c.nupvalues ? &fn->c.upvalue[idx - 1] : niltv(L);
		}
	}
}

namespace LFuncs {
	int lua_pushto(lua_State *from, lua_State *to, int stack, bool first = true);
};

namespace luajit_stuff {
	lua_State *proxy_state = 0;
};

bool __cdecl ShouldOverrideGC(const GCobj * const obj) {
	switch (obj->gch.gct) {
	case ~LJ_TFUNC: {
		GCfunc *fn = (GCfunc *)obj;
		if (fn->l.ffid != FF_LUA) {
			return false;
		}
		GCproto *proto = funcproto(fn);
		return proto->flags & 0x80 ? true : false;
	}
	case ~LJ_TUDATA: {
		GCudata *ud = (GCudata *)obj;
		return ud->unused2 == 0xc9;
	}
	default:
		return false;
	}
}

static int call_function(lua_State *from, lua_State *to) {
	int amt = lua_gettop(from);
	int top = lua_gettop(to) - 1;
	for (int i = 1; i <= amt; i++)
		if (LFuncs::lua_pushto(from, to, i))
			lua_pushnil(to);

	int mask = lua_gethookmask(to), count = lua_gethookcount(to);
	lua_Hook hook = lua_gethook(to);
	lua_sethook(to, NULL, 0, 0);
	lua_call(to, amt, -1);
	lua_sethook(to, hook, mask, count);

	amt = lua_gettop(to) - top;

	for (int i = 1; i <= amt; i++)
		if (LFuncs::lua_pushto(to, from, top + i))
			lua_pushnil(from);

	lua_settop(to, top);
	return amt;
}

static int ProxyFunction(lua_State *from) {
	lua_State *to = GetClientState();

	LFuncs::lua_pushto(from, to, lua_upvalueindex(1));

	return call_function(from, to);
}

static int ProxyLuaFunction (lua_State *from) {
	lua_State *to = GetClientState();
	auto ud = (ProxyLuaFunctionUserData *)lua_touserdata(from, lua_upvalueindex(1));

	TValue *top = to->top;
	lua_pushnil(to);
	setfuncV(to, top, (GCfunc *)ud->fn);

	return call_function(from, to);
}

static int metatable_index;

static int __gc(lua_State *L) {
	auto ud = (ProxyUserData *)lua_touserdata(L, 1);
	ud->proxy_data->unused2 = 0;
	return 0;
}
static int __index(lua_State *L) {
	lua_State *cl = GetClientState();
	LFuncs::lua_pushto(L, cl, 1);
	LFuncs::lua_pushto(L, cl, 2);

	lua_gettable(cl, -2);

	LFuncs::lua_pushto(cl, L, -1);
	lua_pop(cl, 2);
	return 1;
}
static int __newindex(lua_State *L) {
	lua_State *cl = GetClientState();
	LFuncs::lua_pushto(L, cl, 1);
	LFuncs::lua_pushto(L, cl, 2);
	LFuncs::lua_pushto(L, cl, 3);

	lua_settable(cl, -3);

	lua_pop(cl, 1);
	return 0;
}
static int __eq(lua_State *L) {
	lua_State *cl = GetClientState();
	LFuncs::lua_pushto(L, cl, 1);
	LFuncs::lua_pushto(L, cl, 2);

	lua_pushboolean(L, lua_equal(cl, -2, -1));

	lua_pop(cl, 2);

	return 1;
}
static int __tostring(lua_State *L) {
	lua_State *cl = GetClientState();
	LFuncs::lua_pushto(L, cl, 1);

	if (luaL_callmeta(cl, -1, "__tostring")) {
		size_t len;
		const char *str = lua_tolstring(cl, -1, &len);
		lua_pushlstring(L, str, len);
		lua_pop(cl, 1);
	}
	else {
		lua_pushfstring(L, "%s: %p", lua_typename(cl, lua_type(L, -1), lua_gettop(cl)), lua_topointer(cl, -1));
	}
	lua_pop(cl, 1);

	return 1;
}
void InitMetaTable(lua_State *L) {
	lua_newtable(L);
	lua_pushstring(L, "__gc");
	lua_pushcclosure(L, __gc, 0);
	lua_settable(L, -3);
	lua_pushstring(L, "__newindex");
	lua_pushcclosure(L, __newindex, 0);
	lua_settable(L, -3);
	lua_pushstring(L, "__index");
	lua_pushcclosure(L, __index, 0);
	lua_settable(L, -3);
	lua_pushstring(L, "__eq");
	lua_pushcclosure(L, __eq, 0);
	lua_settable(L, -3);
	lua_pushstring(L, "__tostring");
	lua_pushcclosure(L, __tostring, 0);
	lua_settable(L, -3);
	metatable_index = luaL_ref(L, LUA_REGISTRYINDEX);
}

const char *GetMetaTableType(int type);

namespace LFuncs {
	int lua_pushto(lua_State *from, lua_State *to, int stack, bool first)
	{
		if (stack < 0 && stack > -10000) {
			stack = lua_gettop(from) + stack + 1;
		}
		switch (lua_type(from, stack)) {
		case LUA_TNIL:
			lua_pushnil(to);
			break;
		case LUA_TNUMBER:
			lua_pushnumber(to, lua_tonumber(from, stack));
			break;
		case LUA_TBOOLEAN:
			lua_pushboolean(to, lua_toboolean(from, stack));
			break;
		case LUA_TSTRING: {
			size_t slen;
			const char *str = lua_tolstring(from, stack, &slen);
			lua_pushlstring(to, str, slen);
			break;
		}
		case LUA_TUSERDATA: {
			if (luajit_stuff::proxy_state == to) {

				auto ud_from = (GarrysMod::Lua::UserData *)lua_touserdata(from, stack);
				GCudata *udgc = &((GCudata *)ud_from)[-1];
				auto ud = (ProxyUserData *)lua_newuserdata(to, sizeof(ProxyUserData));
				ud->proxy_data = udgc;
				ud->ud.data = ud_from->data;
				ud->ud.type = ud_from->type;

				udgc->unused2 = 0xc9;

				/* metatable */

				lua_rawgeti(to, LUA_REGISTRYINDEX, metatable_index);
				lua_setmetatable(to, -2);
			}
			else if (luajit_stuff::proxy_state == from) {
				auto ud_from = (GarrysMod::Lua::UserData *)lua_touserdata(from, stack);
				/* we need to push the userdata's GCobj pointer to the stack directly, here it goes! */
				TValue *top = to->top;
				lua_pushnil(to);
				setudataV(to, top, *(GCudata **)&ud_from[1]);

			}
			break;
		}
		case LUA_TTABLE:
			if (to == luajit_stuff::proxy_state) {
				// if not R._POINTERS then
				//   R._POINTERS = {}
				// end
				// top = R._POINTERS
				lua_pushlstring(to, "_POINTERS", 9);
				lua_gettable(to, LUA_REGISTRYINDEX);
				if (lua_type(to, -1) == LUA_TNIL) {
					lua_pop(to, 1);
					lua_newtable(to);
					lua_pushlstring(to, "_POINTERS", 9);
					lua_pushvalue(to, -2);
					lua_settable(to, LUA_REGISTRYINDEX);
				}

				// if R._POINTERS[ptr] then
				//   return R._POINTERS[ptr]

				lua_pushnumber(to, (uintptr_t)lua_topointer(from, stack));
				lua_gettable(to, -2);
				if (lua_type(to, -1) == LUA_TTABLE) {
					lua_remove(to, -2);
					break;
				}

				// else 
				//   local ret = {}
				//   R._POINTERS[ptr] = ret
				//   return ret
				// end

				lua_pop(to, 1);
				lua_newtable(to);
				lua_pushnumber(to, (uintptr_t)lua_topointer(from, stack));
				lua_pushvalue(to, -2);
				lua_settable(to, -4);
				lua_remove(to, -2);

				// for k, v in pairs(from_tbl) do
				//   to_tbl[k] = copy(v)
				// end
				lua_pushnil(from);

				while (lua_next(from, stack) != 0) {
					if (lua_pushto(from, to, -2, false)) {
						goto end;
					}
					if (lua_pushto(from, to, -1, false)) {
						lua_pop(to, 1);
						goto end;
					}
					lua_settable(to, -3);
				end:
					lua_pop(from, 1);
				}
				break;
			}
			lua_newtable(to);
			break;

		case LUA_TFUNCTION: {
			auto fn = lua_tocfunction(from, stack);
			if (!fn) { /* lua function */
				if (luajit_stuff::proxy_state != to)
					return 1;
				auto lfn = (GCfuncL *)index2adr(from, stack)->gcr.gcptr32;

				if (lfn->ffid != FF_LUA)
					return 1;

				/* push userdata with the gcfuncl in it */
				auto ud = (ProxyLuaFunctionUserData *)lua_newuserdata(to, sizeof(ProxyLuaFunctionUserData));
				ud->fn = lfn;
				GCproto *proto = funcproto((GCfunc *)ud->fn);
				proto->flags |= 0x80;

				/* bind it to a caller */

				lua_pushcclosure(to, ProxyLuaFunction, 1);
				break;
			}
			lua_pushcclosure(to, fn, 0);
			if (luajit_stuff::proxy_state == to)
				lua_pushcclosure(to, ProxyFunction, 1);
			break;
		}
		default:
			return 1;
		}
		if (first && to == luajit_stuff::proxy_state) {
			lua_pushlstring(to, "_POINTERS", 9);
			lua_pushnil(to);
			lua_settable(to, LUA_REGISTRYINDEX);
		}
		return 0;
	}
}