/* UPS.c
 * 	Definitions related to UPS figures handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */
#ifdef UPS

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <upsclient.h>

#include "UPS.h"
#include "Marcel.h"
#include "MQTT_tools.h"

typedef struct _UPS {
		Module_t module;
		const char *name;	/* Name of the UPS as defined in NUT */
		const char *host;	/* NUT's server */
		int port;			/* NUT's port */
		Var_t *var_list;	/* List of variables to read */
	} UPS_t;

static UPS_t ctx;

Module_t* config_Ups(config_setting_t *cfg)
{
	if(verbose) printf("Entering section 'UPS'\n");
	config_Module(cfg, &ctx.module);
	if (config_setting_lookup_string(cfg, "Name", &ctx.name  )== CONFIG_TRUE)
		ctx.name = strdup(ctx.name);
	if (config_setting_lookup_string(cfg, "Host", &ctx.host  )== CONFIG_TRUE)
		ctx.host = strdup(ctx.host);
	config_setting_lookup_int(cfg, "Port", &ctx.port  );
	config_setting_t *vars = config_lookup_from(cfg, "variables");
	if ( !(vars && config_setting_is_array(vars) )){
		return 0;
	}
	Var_t *url = 0;
	for( int i = 0; i <config_setting_length(vars); i++ )
	{
		Var_t *v = malloc(sizeof(Var_t));
		assert(v);
		v->next = url;
		v->name = strdup(config_setting_get_string_elem(vars, i ));
		url = v;
	}
	ctx.var_list = url;
	return &ctx.module;
}
void *process_Ups(void *data){
	
	/* Sanity checks */
	if(!ctx.module.topic || !ctx.name){
		fputs("*E* configuration error : section name or topic specified, ignoring this section\n", stderr);
		pthread_exit(0);
	}
	

	if(verbose)
		printf("Launching a processing flow for UPS/%s\n", ctx.name);

	for(;;){	/* Infinite loop to process data */
		UPSCONN_t ups;
		
		if(upscli_connect(&ups, ctx.host, ctx.port, UPSCLI_CONN_TRYSSL) < 0){
/*AF : Send error topic */
			// upscli_strerror(UPSCONN_t *ups);
			perror("*E* Connecting");
		} else {
			for(Var_t *v = ctx.var_list; v; v = v->next){
				char ps[MAXLINE], pe[MAXLINE];
				int	ret;
				unsigned int numq, numa;
				const char *query[4];
				char **answer;

				query[0] = "VAR";
				query[1] = ctx.name;
				query[2] = v->name;
				numq = 3;

				ret = upscli_get(&ups, numq, query, &numa, &answer);
				if ((ret < 0) || (numa < numq)) {
					if(verbose)
						printf("*E* %s/%s : unexpected result '(%d) %s'\n", ctx.name, v->name, ret, upscli_strerror(&ups));
				} else {
					assert( strlen(ctx.module.topic) + strlen(v->name) + 2 < MAXLINE ); /* ensure there is enough place for the topic name */
					sprintf( pe, "%s/%s", ctx.module.topic, v->name );
					sprintf( ps, "%s", answer[3] );
					mqttpublish(  pe, strlen(ps), ps, 0 );
					if(verbose)
						printf("UPS : %s -> '%s'\n", pe, ps);
				}
				
			}
			upscli_disconnect(&ups);
			sleep( ctx.module.sample );
		}
	}
	pthread_exit(0);
}
#endif
