/* Every.c
 * 	Repeating task
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 07/09/2015	- LF - First version
 */
#ifdef LUA	/* Only useful with Lua support */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <lauxlib.h>
#include "Lua.h"
#include "Every.h"
#include "Marcel.h"


typedef struct _Every {
		Module_t module;
		const char *name;	/* Name of the section, passed to Lua function */
		const char *funcname;	/* Function to be called */
		int funcid;			/* Function id in Lua registry */
	} Every_t;


Module_t* config_Every(config_setting_t *cfg)
{
	Every_t* ctx = malloc(sizeof(Every_t));
	if(verbose) printf("Entering section 'Every'\n");
	config_Module(cfg, &ctx->module);
	if (config_setting_lookup_string(cfg, "Title", &ctx->name  ) == CONFIG_TRUE )
		ctx->name = strdup(ctx->name);
	if (config_setting_lookup_string(cfg, "Func", &ctx->funcname  ) == CONFIG_TRUE )
		ctx->funcname = strdup(ctx->funcname);

#ifndef LUA
	fputs("*F* Every section is only available when compiled with Lua support\n", stderr);
	exit(EXIT_FAILURE);
#endif

	if(verbose) {
		printf("Entering section 'Every/%s'\n", ctx->name );
		printf("\tFunction : '%s'\n", ctx->funcname);
	}
	return &ctx->module;
}
void *process_Every(void * data){
	Every_t* ctx = (Every_t*)data;
	if(ctx->funcname){
		if( (ctx->funcid = findUserFunc( ctx->funcname )) == LUA_REFNIL ){
			fprintf(stderr, "*F* configuration error : user function \"%s\" is not defined\n", ctx->funcname);
			exit(EXIT_FAILURE);
		}
	}

	if(verbose)
		printf("Launching a processing flow for '%s' Every task\n", ctx->funcname);


	for(;;){
		execUserFuncEvery( ctx->name, ctx->funcid );
		sleep( ctx->module.sample );
	}

	pthread_exit(0);
	return 0;
}

#endif /* LUA */
