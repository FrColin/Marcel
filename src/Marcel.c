/*
 * Marcel
 *	A daemon to publish smart home data to MQTT broker and rise alert
 *	if needed.
 *
 * Additional options :
 *	-DFREEBOX : enable Freebox statistics
 *	-DUPS : enable UPS statistics (NUT needed)
 *
 *	Copyright 2015 Laurent Faillie
 *
 *		Marcel is covered by
 *		Creative Commons Attribution-NonCommercial 3.0 License
 *      (http://creativecommons.org/licenses/by-nc/3.0/) 
 *      Consequently, you're free to use if for personal or non-profit usage,
 *      professional or commercial usage REQUIRES a commercial licence.
 *  
 *      Marcel is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	18/05/2015	- LF start of development (inspired from TeleInfod)
 *	20/05/2015	- LF - v1.0 - "file float value" working
 *	25/05/2015	- LF - v1.1 - Adding "Freebox"
 *	28/05/2015	- LF - v1.2 - Adding UPS
 *				-------
 *	08/07/2015	- LF - start v2.0 - make source modular
 *	21/07/2015	- LF - v2.1 - secure non-NULL MQTT payload
 *	26/07/2015	- LF - v2.2 - Add ConnectionLostIsFatal
 *	27/07/2015	- LF - v2.3 - Add ClientID to avoid connection loss during my tests
 *				-------
 *	07/07/2015	- LF - switch v3.0 - Add Lua user function in DPD
 *	09/08/2015	- LF - 3.1 - Add mutex to avoid parallel subscription which seems
 *					trashing broker connection
 */
#include "Marcel.h"
#include "Freebox.h"
#include "UPS.h"
#include "DeadPublisherDetection.h"
#include "MQTT_tools.h"
#include "Alerting.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>	/* uname */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int verbose = 0;
pthread_mutex_t onefunc;	/* Only one func can run at a time */

	/*
	 * Helpers
	 */
char *removeLF(char *s){
	size_t l=strlen(s);
	if(l && s[--l] == '\n')
		s[l] = 0;
	return s;
}

char *striKWcmp( char *s, const char *kw ){
/* compare string s against kw
 * Return :
 * 	- remaining string if the keyword matches
 * 	- NULL if the keyword is not found
 */
	size_t klen = strlen(kw);
	if( strncasecmp(s,kw,klen) )
		return NULL;
	else
		return s+klen;
}

char *mystrdup(const char *as){
	/* as strdup() is missing within C99, grrr ! */
	char *s;
	assert(as);
	assert(s = malloc(strlen(as)+1));
	strcpy(s, as);
	return s;
}

size_t socketreadline( int fd, char *l, size_t sz){
/* read a line :
 * -> 	fd : file descriptor to read
 *		l : buffer to store the result
 *		sz : max size of the result
 * <- size read, -1 if error or EoF
 */
	int s=0;
	char *p = l, c;

	for(;;){
		int r = read( fd, &c, 1);

		if(r == -1)
			return -1;
		else if(!r){	/* EOF */
			return -1;
		} else if(c == '\n')
			break;
		else {
			*p++ = c;
			s++;
		}
	}
	*p = '\0';

	return s;
}

	/*
	 * Configuration
	 */
static void read_configuration( const char *fch){
	FILE *f;
	char l[MAXLINE];
	char *arg;
	union CSection *last_section=NULL;

	cfg.sections = NULL;
	cfg.Broker = "tcp://localhost:1883";
	cfg.ClientID = "Marcel";
	cfg.client = NULL;
	cfg.DPDlast = 0;
	cfg.ConLostFatal = 0;
	cfg.first_DPD = NULL;

	cfg.SMSurl = NULL;

	cfg.luascript = NULL;

	if(verbose)
		printf("Reading configuration file '%s'\n", fch);

	if(!(f=fopen(fch, "r"))){
		perror(fch);
		exit(EXIT_FAILURE);
	}

	while(fgets(l, MAXLINE, f)){
		if(*l == '#' || *l == '\n')
			continue;

		if((arg = striKWcmp(l,"Broker="))){
			assert( cfg.Broker = strdup( removeLF(arg) ) );
			if(verbose)
				printf("Broker : '%s'\n", cfg.Broker);
		} else if((arg = striKWcmp(l,"ClientID="))){
			assert( cfg.ClientID = strdup( removeLF(arg) ) );
			if(verbose)
				printf("MQTT Client ID : '%s'\n", cfg.ClientID);
		} else if((arg = striKWcmp(l,"SMSUrl="))){
			assert( cfg.SMSurl = strdup( removeLF(arg) ) );
			if(verbose)
				printf("SMS Url : '%s'\n", cfg.SMSurl);
#ifdef LUA
		} else if((arg = striKWcmp(l,"UserFuncScript="))){
			assert( cfg.luascript = strdup( removeLF(arg) ) );
			if(verbose)
				printf("User functions definition script : '%s'\n", cfg.luascript);
#endif
		} else if((arg = striKWcmp(l,"*FFV="))){
			union CSection *n = malloc( sizeof(struct _FFV) );
			assert(n);
			memset(n, 0, sizeof(struct _FFV));
			n->common.section_type = MSEC_FFV;

			if(last_section)
				last_section->common.next = n;
			else	/* First section */
				cfg.sections = n;
			last_section = n;
			if(verbose)
				printf("Entering section '%s'\n", removeLF(arg));
		} else if((arg = striKWcmp(l,"*Freebox"))){
			union CSection *n = malloc( sizeof(struct _FreeBox) );
			assert(n);
			memset(n, 0, sizeof(struct _FreeBox));
			n->common.section_type = MSEC_FREEBOX;

			if(last_section)
				last_section->common.next = n;
			else	/* First section */
				cfg.sections = n;
			last_section = n;
			if(verbose)
				puts("Entering section 'Freebox'");
		} else if((arg = striKWcmp(l,"*UPS="))){
			union CSection *n = malloc( sizeof(struct _UPS) );
			assert(n);
			memset(n, 0, sizeof(struct _UPS));
			n->common.section_type = MSEC_UPS;

			assert( n->Ups.section_name = strdup( removeLF(arg) ) );

			if(last_section)
				last_section->common.next = n;
			else	/* First section */
				cfg.sections = n;
			last_section = n;
			if(verbose)
				printf("Entering section 'UPS/%s'\n", n->Ups.section_name);
		} else if((arg = striKWcmp(l,"*DPD="))){
			union CSection *n = malloc( sizeof(struct _DeadPublisher) );
			assert(n);
			memset(n, 0, sizeof(struct _DeadPublisher));
			n->common.section_type = MSEC_DEADPUBLISHER;

			assert( n->DeadPublisher.errorid = strdup( removeLF(arg) ) );

			if(last_section)
				last_section->common.next = n;
			else	/* First section */
				cfg.sections = n;
			last_section = n;
			if(!cfg.first_DPD)
				cfg.first_DPD = n;

			if(verbose)
				printf("Entering section 'DeadPublisher/%s'\n", n->DeadPublisher.errorid);
		} else if(!strcmp(l,"DPDLast\n")){	/* DPD grouped at the end of the configuration file */
			cfg.DPDlast = 1;
			if(verbose)
				puts("Dead Publisher Detect (DPD) sections are grouped at the end of the configuration");
		} else if(!strcmp(l,"ConnectionLostIsFatal\n")){	/* Crash if the broker connection is lost */
			cfg.ConLostFatal = 1;
			if(verbose)
				puts("Crash if the broker connection is lost");
		} else if((arg = striKWcmp(l,"File="))){
			if(!last_section || last_section->common.section_type != MSEC_FFV){
				fputs("*F* Configuration issue : File directive outside a FFV section\n", stderr);
					exit(EXIT_FAILURE);
			}
			assert( last_section->FFV.file = strdup( removeLF(arg) ));
			if(verbose)
				printf("\tFile : '%s'\n", last_section->FFV.file);
		} else if((arg = striKWcmp(l,"Host="))){
			if(!last_section || last_section->common.section_type != MSEC_UPS){
				fputs("*F* Configuration issue : Host directive outside a UPS section\n", stderr);
					exit(EXIT_FAILURE);
			}
			assert( last_section->Ups.host = strdup( removeLF(arg) ));
			if(verbose)
				printf("\tHost : '%s'\n", last_section->Ups.host);
		} else if((arg = striKWcmp(l,"Port="))){
			if(!last_section || last_section->common.section_type != MSEC_UPS){
				fputs("*F* Configuration issue : Port directive outside a UPS section\n", stderr);
				exit(EXIT_FAILURE);
			}
			if(!(last_section->Ups.port = atoi(arg))){
				fputs("*F* Configurstruct _DeadPublisher *ation issue : Port is null (or is not a number)\n", stderr);
				exit(EXIT_FAILURE);
			}
			if(verbose)
				printf("\tPort : %d\n", last_section->Ups.port);
		} else if((arg = striKWcmp(l,"Var="))){
			if(!last_section || last_section->common.section_type != MSEC_UPS){
				fputs("*F* Configuration issue : Var directive outside a UPS section\n", stderr);
				exit(EXIT_FAILURE);
			}
			struct var *v = malloc(sizeof(struct var));
			assert(v);
			assert( v->name = strdup( removeLF(arg) ));
			v->next = last_section->Ups.var_list;
			last_section->Ups.var_list = v;
			if(verbose)
				printf("\tVar : '%s'\n", v->name);
		} else if((arg = striKWcmp(l,"Func="))){
			if(!last_section || last_section->common.section_type != MSEC_DEADPUBLISHER){
				fputs("*F* Configuration issue : Func directive outside a DPD section\n", stderr);
				exit(EXIT_FAILURE);
			}
#ifndef LUA
			fputs("*F* User functions can only be used when compiled with Lua support\n", stderr);
			exit(EXIT_FAILURE);
#endif
			assert( last_section->DeadPublisher.funcname = strdup( removeLF(arg) ));
			if(verbose)
				printf("\tFunction : '%s'\n", last_section->DeadPublisher.funcname);
		} else if((arg = striKWcmp(l,"Sample="))){
			if(!last_section){
				fputs("*F* Configuration issue : Sample directive outside a section\n", stderr);
				exit(EXIT_FAILURE);
			}
			last_section->common.sample = atoi( arg );
			if(verbose)
				printf("\tDelay between samples : %ds\n", last_section->common.sample);
		} else if((arg = striKWcmp(l,"Topic="))){
			if(!last_section){
				fputs("*F* Configuration issue : Topic directive outside a section\n", stderr);
				exit(EXIT_FAILURE);
			}
			assert( last_section->common.topic = strdup( removeLF(arg) ));
			if(verbose)
				printf("\tTopic : '%s'\n", last_section->common.topic);
		}
	}

	fclose(f);
}

	/*
	 * Broker related functions
	 */
static int msgarrived(void *actx, char *topic, int tlen, MQTTClient_message *msg){
	union CSection *DPD = cfg.DPDlast ? cfg.first_DPD : cfg.sections;
	const char *aid;
	char payload[msg->payloadlen + 1];

	memcpy(payload, msg->payload, msg->payloadlen);
	payload[msg->payloadlen] = 0;

	if(verbose)
		printf("*I* message arrival (topic : '%s', msg : '%s')\n", topic, payload);

	if((aid = striKWcmp(topic,"Alert/")))
		rcv_alert( aid, payload );
	else for(; DPD; DPD = DPD->common.next){
		if(DPD->common.section_type != MSEC_DEADPUBLISHER)
			continue;
		if(!mqtttokcmp(DPD->DeadPublisher.topic, topic)){	/* Topic found */
			uint64_t v = 1;
			write( DPD->DeadPublisher.rcv, &v, sizeof(v) );	/* Signal it */

#ifdef LUA
			execUserFunc( &(DPD->DeadPublisher), topic, payload );
#endif
		}
	}

	MQTTClient_freeMessage(&msg);
	MQTTClient_free(topic);
	return 1;
}

static void connlost(void *ctx, char *cause){
	printf("*W* Broker connection lost due to %s\n", cause);
	if(cfg.ConLostFatal)
		exit(EXIT_FAILURE);
}

static void brkcleaning(void){	/* Clean broker stuffs */
	MQTTClient_disconnect(cfg.client, 10000);	/* 10s for the grace period */
	MQTTClient_destroy(&cfg.client);
}

	/*
	 * Processing
	 */
static void *process_FFV(void *actx){
	struct _FFV *ctx = actx;	/* Only to avoid zillions of cast */
	FILE *f;
	char l[MAXLINE];

		/* Sanity checks */
	if(!ctx->topic){
		fputs("*E* configuration error : no topic specified, ignoring this section\n", stderr);
		pthread_exit(0);
	}
	if(!ctx->file){
		fprintf(stderr, "*E* configuration error : no file specified for '%s', ignoring this section\n", ctx->topic);
		pthread_exit(0);
	}

	if(verbose)
		printf("Launching a processing flow for FFV '%s'\n", ctx->topic);

	for(;;){	/* Infinite loop to process messages */
		ctx = actx;	/* Back to the 1st one */
		for(;;){
			if(!(f = fopen( ctx->file, "r" ))){
				if(verbose)
					perror( ctx->file );
				if(strlen(ctx->topic) + 7 < MAXLINE){  /* "/Alarm" +1 */
					int msg;
					char *emsg;
					strcpy(l, "Alarm/");
					strcat(l, ctx->topic);
					msg = strlen(l) + 2;

					if(strlen(ctx->file) + strlen(emsg = strerror(errno)) + 5 < MAXLINE - msg){ /* S + " : " + 0 */
						*(l + msg) = 'S';
						strcpy(l + msg + 1, ctx->file);
						strcat(l + msg, " : ");
						strcat(l + msg, emsg);

						mqttpublish(cfg.client, l, strlen(l + msg), l + msg, 0);
					} else if( strlen(emsg) + 2 < MAXLINE - msg ){	/* S + error message */
						*(l + msg) = 'S';
						strcpy(l + msg + 1, emsg);

						mqttpublish(cfg.client, l, strlen(l + msg), l + msg, 0);
					} else {
						char *msg = "Can't open file (and not enough space for the error)";
						mqttpublish(cfg.client, l, strlen(msg), msg, 0);
					}
				}
			} else {
				float val;
				fscanf(f, "%f", &val);	/* Only to normalize the response */
				sprintf(l,"%.1f", val);

				mqttpublish(cfg.client, ctx->topic, strlen(l), l, 0 );
				if(verbose)
					printf("FFV : %s -> %f\n", ctx->topic, val);
				fclose(f);
			}

			if(!(ctx = (struct _FFV *)ctx->next))	/* It was the last entry */
				break;
			if(ctx->section_type != MSEC_FFV || ctx->sample)	/* Not the same kind or new thread requested */
				break;
		}

		sleep( ((struct _FFV *)actx)->sample );
	}

	pthread_exit(0);
}

static void handleInt(int na){
	exit(EXIT_SUCCESS);
}

int main(int ac, char **av){
	const char *conf_file = DEFAULT_CONFIGURATION_FILE;
	pthread_attr_t thread_attr;
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

	if(ac > 0){
		int i;
		for(i=1; i<ac; i++){
			if(!strcmp(av[i], "-h")){
				fprintf(stderr, "%s (%s)\n"
					"Publish Smart Home figures to an MQTT broker\n"
					"Known options are :\n"
					"\t-h : this online help\n"
					"\t-v : enable verbose messages\n"
					"\t-f<file> : read <file> for configuration\n"
					"\t\t(default is '%s')\n",
					basename(av[0]), VERSION, DEFAULT_CONFIGURATION_FILE
				);
				exit(EXIT_FAILURE);
			} else if(!strcmp(av[i], "-v")){
				verbose = 1;
				puts("Marcel (c) L.Faillie 2015");
				printf("%s v%s starting ...\n", basename(av[0]), VERSION);
			} else if(!strncmp(av[i], "-f", 2))
				conf_file = av[i] + 2;
			else {
				fprintf(stderr, "Unknown option '%s'\n%s -h\n\tfor some help\n", av[i], av[0]);
				exit(EXIT_FAILURE);
			}
		}
	}
	read_configuration( conf_file );

	if(!cfg.sections){
		fputs("*F* No section defined : giving up ...\n", stderr);
		exit(EXIT_FAILURE);
	}

		/* Connecting to the broker */
	conn_opts.reliable = 0;
	MQTTClient_create( &cfg.client, cfg.Broker, cfg.ClientID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks( cfg.client, NULL, connlost, msgarrived, NULL);

	switch( MQTTClient_connect( cfg.client, &conn_opts) ){
	case MQTTCLIENT_SUCCESS : 
		break;
	case 1 : fputs("Unable to connect : Unacceptable protocol version\n", stderr);
		exit(EXIT_FAILURE);
	case 2 : fputs("Unable to connect : Identifier rejected\n", stderr);
		exit(EXIT_FAILURE);
	case 3 : fputs("Unable to connect : Server unavailable\n", stderr);
		exit(EXIT_FAILURE);
	case 4 : fputs("Unable to connect : Bad user name or password\n", stderr);
		exit(EXIT_FAILURE);
	case 5 : fputs("Unable to connect : Not authorized\n", stderr);
		exit(EXIT_FAILURE);
	default :
		fputs("Unable to connect\n", stderr);
		exit(EXIT_FAILURE);
	}
	atexit(brkcleaning);

	pthread_mutex_init( &onefunc, NULL);
	init_alerting();
#ifdef LUA
	init_Lua( conf_file );
#endif

		/* Creating childs */
	if(verbose)
		puts("\nCreating childs processes\n"
			   "---------------------------");

	assert(!pthread_attr_init (&thread_attr));
	assert(!pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED));
	for(union CSection *s = cfg.sections; s; s = s->common.next){
		switch(s->common.section_type){
		case MSEC_FFV:
			if(s->common.sample){	/* Creates only if sample is set */
				if(pthread_create( &(s->common.thread), &thread_attr, process_FFV, s) < 0){
					fputs("*F* Can't create a processing thread\n", stderr);
					exit(EXIT_FAILURE);
				}
			}
			break;
#ifdef FREEBOX
		case MSEC_FREEBOX:
			if(!s->common.sample){
				fputs("*E* Freebox section without sample time : ignoring ...\n", stderr);
			} else if(pthread_create( &(s->common.thread), &thread_attr, process_Freebox, s) < 0){
				fputs("*F* Can't create a processing thread\n", stderr);
				exit(EXIT_FAILURE);
			}
			break;
#endif
#ifdef UPS
		case MSEC_UPS:
			if(!s->common.sample){ /* we won't group UPS to prevent too many DNS lookup */
				fputs("*E* UPS section without sample time : ignoring ...\n", stderr);
			} else if(pthread_create( &(s->common.thread), &thread_attr, process_UPS, s) < 0){
				fputs("*F* Can't create a processing thread\n", stderr);
				exit(EXIT_FAILURE);
			}			
			break;
#endif
		case MSEC_DEADPUBLISHER:
			if(!s->common.topic){
				fputs("*E* configuration error : no topic specified, ignoring this section\n", stderr);
			} else if(!*s->DeadPublisher.errorid){
				fprintf(stderr, "*E* configuration error : no errorid specified for DPD '%s', ignoring this section\n", s->common.topic);
			} else if(!s->common.sample && !s->DeadPublisher.funcname){
				fputs("*E* DeadPublisher section without sample time or user function defined : ignoring ...\n", stderr);
			} else {
				if(MQTTClient_subscribe( cfg.client, s->common.topic, 0 ) != MQTTCLIENT_SUCCESS ){
					fprintf(stderr, "Can't subscribe to '%s'\n", s->common.topic );
				} else if(pthread_create( &(s->common.thread), &thread_attr, process_DPD, s) < 0){
					fputs("*F* Can't create a processing thread\n", stderr);
					exit(EXIT_FAILURE);
				}
			}
			break;
		default :	/* Ignore unsupported type */
			break;
		}
	}

	signal(SIGINT, handleInt);
	pause();

	exit(EXIT_SUCCESS);
}

