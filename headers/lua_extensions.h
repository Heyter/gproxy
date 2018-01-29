#include "userdata.h"

struct GCfuncL;
union GCobj;
union TValue;
struct GCudata;

struct ProxyLuaFunctionUserData {
	GCfuncL *fn;
};
struct ProxyUserData {
	GarrysMod::Lua::UserData ud;
	GCudata *proxy_data;
};