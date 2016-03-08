/* Lua.c
 *
 * Implementation Lua user functions
 *
 * Cover by Creative Commons Attribution-NonCommercial 3.0 License
 * (http://creativecommons.org/licenses/by-nc/3.0/)
 *
 * 22/07/2015 LF - Creation
 */


#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libgen.h>		/* dirname ... */
#include <unistd.h>		/* chdir ... */
#include <limits.h>
#include <json-c/json.h>

#ifdef LUA

#include <lauxlib.h>	/* auxlib : usable hi-level function */
#include <lualib.h>		/* Functions to open libraries */

#include "Lua.h"

#include "Marcel.h"
#include "Alerting.h"
#include "MQTT_tools.h"
#include "Freebox.h" /* reference Freebox function */


static lua_State *L;
pthread_mutex_t onefunc;	/* Only one func can run at a time */

static void clean_lua(void){
	lua_close(L);
}

int findUserFunc( const char *fn ){
	lua_getglobal(L, fn);
	if( lua_type(L, -1) != LUA_TFUNCTION ){
		if(verbose && lua_type(L, -1) != LUA_TNIL )
			fprintf(stderr, "*E* \"%s\" is not a function, a %s.\n", fn, lua_typename(L, lua_type(L, -1)) );
		lua_pop(L, 1);
		return LUA_REFNIL;
	}

	int ref = luaL_ref(L,LUA_REGISTRYINDEX);	/* Get the reference to the function */

	return ref;
}

void execUserFuncDeadPublisher( const char *name, int funcid, const char *topic, const char *msg){
	if(funcid != LUA_TNIL){	/* A function is defined */
		pthread_mutex_lock( &onefunc );
		lua_rawgeti( L, LUA_REGISTRYINDEX, funcid);	/* retrieves the function */
		lua_pushstring( L, topic);
		lua_pushstring( L, msg);
		if(lua_pcall( L, 2, 0, 0)){
			fprintf(stderr, "DPD / '%s' : %s\n", topic, lua_tostring(L, -1));
			lua_pop(L, 1);  /* pop error message from the stack */
			lua_pop(L, 1);  /* pop NIL from the stack */
		}
		pthread_mutex_unlock( &onefunc );
	}
}

void execUserFuncEvery( const char *name,  int funcid){
	pthread_mutex_lock( &onefunc );
	lua_rawgeti( L, LUA_REGISTRYINDEX, funcid);	/* retrieves the function */
	lua_pushstring( L, name );
	if(lua_pcall( L, 1, 0, 0)){
		fprintf(stderr, "Every / %s : %s\n", name, lua_tostring(L, -1));
		lua_pop(L, 1);  /* pop error message from the stack */
		lua_pop(L, 1);  /* pop NIL from the stack */
	}
	pthread_mutex_unlock( &onefunc );
}


static int lmSendMsg(lua_State *L){
	if(lua_gettop(L) != 2){
		fputs("*E* In your Lua code, SendMessage() requires 2 arguments : title and message\n", stderr);
		return 0;
	}

	const char *topic = luaL_checkstring(L, 1);
	const char *msg = luaL_checkstring(L, 2);
	AlertCmd( topic, msg );
	
	return 0;
}

static int lmRiseAlert(lua_State *L){
	if(lua_gettop(L) != 2){
		fputs("*E* In your Lua code, RiseAlert() requires 2 arguments : topic, message\n", stderr);
		return 0;
	}

	const char *topic = luaL_checkstring(L, 1);
	const char *msg = luaL_checkstring(L, 2);
	RiseAlert( topic, msg, 0 );
	
	return 0;
}

static int lmRiseAlertSMS(lua_State *L){
	if(lua_gettop(L) != 2){
		fputs("*E* In your Lua code, RiseAlert() requires 2 arguments : topic, message\n", stderr);
		return 0;
	}

	const char *topic = luaL_checkstring(L, 1);
	const char *msg = luaL_checkstring(L, 2);
	RiseAlert( topic, msg, -1 );
	
	return 0;
}

static int lmClearAlert(lua_State *L){
	if(lua_gettop(L) != 1){
		fputs("*E* In your Lua code, ClearAlert() requires 1 argument : topic\n", stderr);
		return 0;
	}

	const char *topic = luaL_checkstring(L, 1);
	AlertIsOver( topic );

	return 0;
}

static int lmPublish(lua_State *L){
	if(lua_gettop(L) != 2){
		fputs("*E* In your Lua code, Publish() requires 2 arguments : topic and value.\n", stderr);
		return 0;
	}

	const char *topic = luaL_checkstring(L, 1),
				*val = luaL_checkstring(L, 2);

	mqttpublish( topic, strlen(val), (void *)val, 0 );

	return 0;
}

static int lmHostname(lua_State *L){
	char n[HOST_NAME_MAX];
	gethostname(n, HOST_NAME_MAX);

	lua_pushstring(L, n);
	return 1;
}

static int lmClientID(lua_State *L){
	lua_pushstring(L, mqttgetclientID() );

	return 1;
}

static int lmVersion(lua_State *L){
	lua_pushstring(L, VERSION);

	return 1;
}
#ifdef LUA
static void lua_push_json_object( lua_State *L, json_object *jobj );

static void lua_publish_json_array ( lua_State *L, json_object *jobj )
{
	int arraylen = json_object_array_length(jobj); /*Getting the length of the array*/
	int i;
	json_object * jvalue;
	for (i=0; i< arraylen; i++){
		jvalue = json_object_array_get_idx(jobj, i); /*Getting the array element at position i*/
		lua_push_json_object( L, jvalue);
		lua_rawseti(L, -2, i+1);
	}
}
static void lua_push_json_object( lua_State *L, json_object *jobj ) {
	enum json_type obj_type;
	enum json_type key_type;
	obj_type = json_object_get_type(jobj);
	switch (obj_type) {
		 case json_type_null: lua_pushnil(L);
		 break;
		 case json_type_boolean: lua_pushboolean(L, json_object_get_boolean(jobj));
		 break;
		 case json_type_double: lua_pushnumber(L, json_object_get_double(jobj));
		 break;
		 case json_type_int: lua_pushinteger(L, json_object_get_int(jobj));
		 break;
		 case json_type_string:  lua_pushstring (L, json_object_get_string(jobj));
		 break;
		 case json_type_object: 
			lua_newtable(L);
			json_object_object_foreach(jobj, key, val) {
			key_type = json_object_get_type(val);
			 switch (key_type) {
				case json_type_null: lua_pushnil(L);
				break;
				case json_type_boolean: lua_pushboolean(L, json_object_get_boolean(val));
				break;
				case json_type_double: lua_pushnumber(L, json_object_get_double(val));
				break;
				case json_type_int: lua_pushinteger(L, json_object_get_int(val));
				break;
				case json_type_string:  lua_pushstring (L, json_object_get_string(val));
				break;
				case json_type_object: 
					lua_push_json_object(L, val);
				break;
				case json_type_array: 
					lua_publish_json_array(L,val);
				break;
			 }
			lua_setfield(L, -2, key);
			}
		 break;
		 case json_type_array: 
			lua_newtable(L);
			lua_publish_json_array ( L, jobj );
		 break;
		 }
}

static int lmCallFreeboxApi(lua_State *L){
	json_object *jdata = NULL;
	if(lua_gettop(L) < 1 || lua_gettop(L) > 2 ){
		luaL_argerror (L, 1, "CallFreeboxApi() requires : url [, data ]");
		return 0;
	}
	luaL_checktype(L, 1, LUA_TSTRING);
	const char *url = luaL_checkstring(L, 1);
	if(lua_gettop(L) == 2){
		const char *data = luaL_checkstring(L, 2);
		jdata = json_tokener_parse(data);
	}
	json_object *result = call_freebox_api(url, jdata);
	if ( result ) {
		lua_push_json_object(L, result);
		return 1;
	}
	return 0;
}
#endif
static const struct luaL_Reg MarcelLib [] = {
	{"CallFreeboxApi", lmCallFreeboxApi},
	{"SendMessage", lmSendMsg},
	{"RiseAlert", lmRiseAlert},		/* ... and send only a mail */
	{"RiseAlert", lmRiseAlertSMS},	/* ... and send both a mail and a SMS */
	{"ClearAlert", lmClearAlert},
	{"MQTTPublish", lmPublish},
	{"Hostname", lmHostname},
	{"ClientID", lmClientID},
	{"Version", lmVersion},
	{NULL, NULL}
};

void init_Lua(  const char *luascript ){
	if( luascript ){

		L = luaL_newstate();		/* opens Lua */
		luaL_openlibs(L);	/* and it's libraries */
		atexit(clean_lua);

		luaL_newmetatable(L, "Marcel");
		lua_pushstring(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);	/* metatable.__index = metatable */
		luaL_setfuncs(L,MarcelLib,0);
		lua_setglobal(L, "Marcel");
		int err = luaL_loadfile(L, luascript) || lua_pcall(L, 0, 0, 0);
		if(err){
			fprintf(stderr, "*F* '%s' : %s\n", luascript, lua_tostring(L, -1));
			exit(EXIT_FAILURE);
		}

		pthread_mutex_init( &onefunc, NULL);
	} else if(verbose)
		puts("*W* No FuncScript defined, Lua disabled");
}
#endif
