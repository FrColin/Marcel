/* Scrip.c
 * 	Definitions related to Lua Script handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */
#ifdef LUA

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "Lua.h"
#include "Every.h"
#include "DList.h"
#include "DeadPublisherDetection.h"
#include "Script.h"
#include "Marcel.h"
#include "MQTT_tools.h"

typedef struct _Module_Node {
	DLNode_t node;
	Module_t *module;
}Module_Node_t;

typedef struct _Script {
	Module_t module;
	const char *luascript;		/* file containing Lua functions */
	DList_t everys;
	DList_t dpds;
	} Script_t;

static Script_t ctx;

Module_t* config_Scripts(config_setting_t *cfg)
{
	if(verbose) printf("Entering section 'Script'\n");
	DLListInit( &ctx.everys );
	DLListInit( &ctx.dpds );
	
	config_setting_lookup_string(cfg, "UserFuncScript", &ctx.luascript  );
	ctx.luascript = strdup(ctx.luascript);
	config_setting_t *everys_cfg = config_lookup_from(cfg,"Every");
	if ( everys_cfg ) {
		int len = config_setting_length (everys_cfg);
		for ( int index =0; index < len ; index ++ ){
			config_setting_t * every_cfg = config_setting_get_elem(everys_cfg, index );
			if ( every_cfg ){
				Module_t *md = config_Every(every_cfg);
				Module_Node_t *node = malloc(sizeof(Module_Node_t));
				node->module = md;
				DLAdd( &ctx.everys, &node->node );
			}
		}
	}
	config_setting_t *dpds_cfg = config_lookup_from(cfg,"Dpds");
	if ( dpds_cfg ) {
		int len = config_setting_length (dpds_cfg);
		for ( int index =0; index < len ; index ++ ){
			config_setting_t * dpd_cfg = config_setting_get_elem(dpds_cfg, index );
			Module_t *md = config_DPD(dpd_cfg);
			Module_Node_t *node = malloc(sizeof(Module_Node_t));
			node->module = md;
			DLAdd( &ctx.dpds, &node->node );
		}
	}
	if(verbose) {
		printf("User functions definition script : '%s'\n", ctx.luascript);
		} 
	return &ctx.module;
}
void *process_Scripts(void *data){
	pthread_attr_t thread_attr;
	
	init_Lua( ctx.luascript );
	
	assert(!pthread_attr_init (&thread_attr));
	assert(!pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED));
	
	for(DLNode_t *node = ctx.everys.first; node; node = node->next){
		Module_Node_t *mod = (Module_Node_t *) node;
		if ( verbose ) fprintf(stdout,"Start EVERY Thread \n");
		if(pthread_create( &(mod->module->thread), &thread_attr, process_Every, mod->module) < 0){
				fprintf(stderr,"*F* Can't create a %s processing thread\n", "TODO" /*mod->name*/);
				exit(EXIT_FAILURE);
			}
	}
	for(DLNode_t *node = ctx.dpds.first; node; node =  node->next){
		Module_Node_t *mod = (Module_Node_t *) node;
		if ( verbose ) fprintf(stdout,"Start DPDS Thread \n");
		if(pthread_create( &(mod->module->thread), &thread_attr, process_DPD, mod->module) < 0){
				fprintf(stderr,"*F* Can't create a %s processing thread\n", "TODO" /*mod->name*/);
				exit(EXIT_FAILURE);
			}
	}
	return 0;
}
#endif
