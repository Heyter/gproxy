#include <map>
#include <string>
#include <utility>
#include "lua.hpp"
#include "luainterface.h"

namespace luajit_stuff {
	GarrysMod::Lua::ILuaInterface *client_state;
};

lua_State *GetClientState() {
	return luajit_stuff::client_state->GetState();
}