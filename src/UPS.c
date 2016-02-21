/* UPS.c
 * 	Definitions related to UPS figures handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */
#ifdef UPS

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

#include <upsclient.h>

#include "UPS.h"
#include "MQTT_tools.h"

void *process_UPS(void *actx){
	struct _UPS *ctx = actx;	/* Only to avoid zillions of cast */
	
		/* Sanity checks */
	if(!ctx->topic || !ctx->section_name){
		fputs("*E* configuration error : section name or topic specified, ignoring this section\n", stderr);
		pthread_exit(0);
	}
	

	if(verbose)
		printf("Launching a processing flow for UPS/%s\n", ctx->section_name);

	for(;;){	/* Infinite loop to process data */
		UPSCONN_t ups;
		
		if(upscli_connect(&ups, ctx->host, ctx->port, UPSCLI_CONN_TRYSSL) < 0){
/*AF : Send error topic */
			// upscli_strerror(UPSCONN_t *ups);
			perror("*E* Connecting");
		} else {
			for(struct var *v = ctx->var_list; v; v = v->next){
				char ps[MAXLINE], pe[MAXLINE];
				int	ret;
				unsigned int numq, numa;
				const char *query[4];
				char **answer;

				query[0] = "VAR";
				query[1] = ctx->section_name;
				query[2] = v->name;
				numq = 3;

				ret = upscli_get(&ups, numq, query, &numa, &answer);
				if ((ret < 0) || (numa < numq)) {
					if(verbose)
						printf("*E* %s/%s : unexpected result '(%d) %s'\n", ctx->section_name, v->name, ret, upscli_strerror(&ups));
				} else {
					assert( strlen(ctx->topic) + strlen(v->name) + 2 < MAXLINE ); /* ensure there is enough place for the topic name */
					sprintf( pe, "%s/%s", ctx->topic, v->name );
					sprintf( ps, "%s", answer[3] );
					mqttpublish( cfg.client, pe, strlen(ps), ps, 0 );
					if(verbose)
						printf("UPS : %s -> '%s'\n", pe, ps);
				}
				
			}
			upscli_disconnect(&ups);
			sleep( ctx->sample );
		}
	}
	pthread_exit(0);
}
#endif
