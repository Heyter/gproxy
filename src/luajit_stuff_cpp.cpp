#include <map>
#include <string>
#include <utility>
#include "lua.hpp"
#include "luainterface.h"

std::map<int, std::string> MetaTableTypes;

const char *GetMetaTableType(int type) {
	return MetaTableTypes.at(type).c_str();
}

namespace luajit_stuff {
	GarrysMod::Lua::ILuaInterface *client_state;
};

lua_State *GetClientState() {
	return luajit_stuff::client_state->GetState();
}