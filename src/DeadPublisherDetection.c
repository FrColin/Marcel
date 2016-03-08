/* DeadPublisherDetection.c
 * 	DPD processing
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 09/07/2015	- LF - First version
 * 28/07/2015	- LF - Add user function
 */

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/select.h>

#include <sys/eventfd.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef LUA
#include <lauxlib.h>
#endif

#include "DeadPublisherDetection.h"
#include "Lua.h"
#include "Marcel.h"
#include "MQTT_tools.h"

typedef struct _DeadPublisher {
		Module_t module;
		const char *Topic;
		const char *funcname;	/* User function to call on data arrival */
		int funcid;				/* Function id in Lua registry */
		const char *errorid;	/* Error's name */
		int rcv;				/* Event for data receiving */
		int inerror;			/* true if this DPD is in error */
	} DeadPublisher_t;

	
Module_t* config_DPD(config_setting_t *cfg){
	if(verbose)
		printf("Entering section 'DeadPublisher'\n");
	DeadPublisher_t *ctx  = malloc( sizeof(DeadPublisher_t));
	config_Module(cfg, &ctx->module);
	config_setting_lookup_string(cfg, "Topic", &ctx->Topic  );
	ctx->Topic = strdup(ctx->Topic);
	config_setting_lookup_string(cfg, "Func", &ctx->funcname  );
	ctx->funcname = strdup(ctx->funcname);
	config_setting_lookup_string(cfg, "DPD", &ctx->errorid  );
	ctx->errorid = strdup(ctx->errorid);
	if(verbose)
		printf("Entering section 'DeadPublisher/%s'\n", ctx->errorid);
	return &ctx->module;
}

void *process_DPD(void *data){
	DeadPublisher_t *ctx  = (DeadPublisher_t *)data;
	struct timespec ts, *uts;
	if(!ctx->module.topic){
		fputs("*E* configuration error : no topic specified, ignoring this section\n", stderr);
	} else if(!ctx->errorid){
		fprintf(stderr, "*E* configuration error : no errorid specified for DPD '%s', ignoring this section\n", ctx->module.topic);
	} else if(!ctx->module.sample && !ctx->funcname){
			fputs("*E* DeadPublisher section without sample time or user function defined : ignoring ...\n", stderr);
	} else {
		if(mqttsubscribe( ctx->module.topic ) != 0 ){
			fprintf(stderr, "Can't subscribe to '%s'\n",  ctx->module.topic );
		} 
	}
		/* Sanity checks */
#ifdef LUA
	if(ctx->funcname){
		if( (ctx->funcid = findUserFunc( ctx->funcname )) == LUA_REFNIL ){
			fprintf(stderr, "*F* configuration error : user function \"%s\" is not defined\n", ctx->funcname);
			exit(EXIT_FAILURE);
		}
	}
#endif
	if(verbose)
		printf("Launching a processing flow for DeadPublisherDetect (DPD) '%s'\n", ctx->module.topic);

		/* Creating the fd for the notification */
	if(( ctx->rcv = eventfd( 0, 0 )) == -1 ){
		perror("eventfd()");
		pthread_exit(0);
	}

	if(ctx->module.sample){
		ts.tv_sec = (time_t)ctx->module.sample;
		ts.tv_nsec = 0;

		uts = &ts;
	} else
		uts = NULL;

	for(;;){
		fd_set rfds;
		FD_ZERO( &rfds );
		FD_SET( ctx->rcv, &rfds );

		switch( pselect( ctx->rcv+1, &rfds, NULL, NULL, uts, NULL ) ){
		case -1:	/* Error */
			close( ctx->rcv );
			ctx->rcv = 1;
			perror("pselect()");
			pthread_exit(0);
		case 0:	/* timeout */
			if(verbose)
				printf("*I* timeout for DPD '%s'\n", ctx->errorid);
			if( !ctx->inerror ){
				char topic[strlen(ctx->errorid) + 7]; /* "Alert/" + 1 */
				const char *msg_info = "SNo data received after %d seconds";
				char msg[ strlen(msg_info) + 15 ];	/* Some room for number of seconds */
				int msg_len;

				strcpy( topic, "Alert/" );
				strcat( topic, ctx->errorid );

				msg_len = sprintf( msg, msg_info, ctx->module.sample );

				if( mqttpublish( topic, msg_len, msg, 0 ) == 0 ){
					if(verbose)
						printf("*I* Alert raises for DPD '%s'\n", ctx->errorid);
						ctx->inerror = 1;
					}
			}
			break;
		default:{	/* Got some data
					 *
					 * Notez-Bien : Lua's user function is launched in 
					 * Marcel.c/msgarrived() directly
					 */
				uint64_t v;
				if(read(ctx->rcv, &v, sizeof( uint64_t )) == -1)
					perror("eventfd - reading notification");
				if( ctx->inerror ){
					char topic[strlen(ctx->errorid) + 7]; /* "Alert/" + 1 */
					strcpy( topic, "Alert/" );
					strcat( topic, ctx->errorid );
					if( mqttpublish(  topic, 1, "E", 0 ) == 0 ){
						if(verbose)
							printf("*I* Alert corrected for DPD '%s'\n", ctx->errorid);
						ctx->inerror = 0;
					}
				}
			}
			break;
		}
	}

	close( ctx->rcv );
	ctx->rcv = 1;
	
	pthread_exit(0);
}

