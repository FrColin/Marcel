/* MQTT_tools.c
 *
 * Tools useful for MQTT processing
 * (this file is shared by several projects)
 *
 * 13/07/2015 LF : First version
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <mosquitto.h>
#include "MQTT_tools.h"
#include "Alerting.h"
#include "Marcel.h"


int mqtttokcmp(register const char *s, register const char *t){
	char last = 0;
	if(!s || !t)
		return -1;

	for(; ; s++, t++){
		if(!*s){ /* End of string */
			return(*s - *t);
		} else if(*s == '#') /* ignore remaining of the string */
			return (!*++s && (!last || last=='/')) ? 0 : -1;
		else if(*s == '+'){	/* ignore current level of the hierarchy */
			s++;
			if((last !='/' && last ) || (*s != '/' && *s )) /* has to be enclosed by '/' */
				return -1;
			while(*t != '/'){	/* looking for the closing '/' */
				if(!*t)
					return 0;
				t++;
			}
			if(*t != *s)	/* ensure the s isn't finished */
				return -1;
		} else if(*s != *t)
			break;
		last = *s;
	}
	return(*s - *t);
}


typedef struct _Broker {
	const char *Host;		/* Broker's host */
	int Port;				/* Broker's port */
	const char *ClientID;	/* Marcel client id : must be unique among a broker clients */
	int ConLostFatal;		/* Die if broker connection is lost */
	struct mosquitto * client;
	
} Broker_t;

static Broker_t broker;

const char * mqttgetclientID() {
	return broker.ClientID;
}
char *striKWcmp( char *s, const char *kw ){
/* compare string s against kw
 * Return :
 *     - remaining string if the keyword matches
 *     - NULL if the keyword is not found
 */
       size_t klen = strlen(kw);
       if( strncasecmp(s,kw,klen) )
               return NULL;
       else
               return s+klen;
}

/*
 * Broker related functions
 */
static void msgarrived(struct mosquitto *mosq, void *obj,  const struct mosquitto_message *msg){
	const char *aid;
	char payload[msg->payloadlen + 1];

	memcpy(payload, msg->payload, msg->payloadlen);
	payload[msg->payloadlen] = 0;

	if(verbose)
		printf("*I* message arrival (topic : '%s', msg : '%s')\n", msg->topic, payload);

	if((aid = striKWcmp(msg->topic,"Alert/")))
		rcv_alert( aid, payload );
#ifdef TODO
	else for(; DPD; DPD = DPD->common.next){
		
		if(!mqtttokcmp(DPD->DeadPublisher.topic, msg->topic)){	/* Topic found */
			uint64_t v = 1;
			if(write( DPD->DeadPublisher.rcv, &v, sizeof(v) ) == -1)	/* Signal it */
				perror("eventfd to signal message reception");

#ifdef LUA
			execUserFuncDeadPublisher( DPD->DeadPublisher.funcname, DPD->DeadPublisher.funcid, msg->topic, payload );
#endif
		}
	}
#endif

}

static void connlost(struct mosquitto *mosq, void *obj, int cause){
	printf("*W* Broker connection lost due to %d\n", cause);
	if(broker.ConLostFatal)
		exit(EXIT_FAILURE);
}
static void brkcleaning(void){	/* Clean broker stuffs */
	mosquitto_disconnect(broker.client);	/* 10s for the grace period */
	mosquitto_destroy(broker.client);
}
int mqttpublish( const char *topic, size_t length, void *payload, int retained ){ /* Custom wrapper to publish */
	int mid;
	return mosquitto_publish( broker.client, &mid, topic,  length, payload, 0, retained);
}
int mqttsubscribe( const char *topic )
{
	return mosquitto_subscribe( broker.client, NULL, topic, 0);
}

void configure_Broker(config_setting_t *cfg){
	
	if (config_setting_lookup_string(cfg, "Host", &broker.Host ) == CONFIG_TRUE)
		broker.Host = strdup(broker.Host);
	config_setting_lookup_int(cfg, "Port", &broker.Port);
	if (config_setting_lookup_string(cfg, "ClientID", &broker.ClientID )== CONFIG_TRUE)
		broker.ClientID = strdup(broker.ClientID);
	config_setting_lookup_bool(cfg, "ConnectionLostIsFatal", &broker.ConLostFatal);
	
}
void init_Broker()
{
	/* Connecting to the broker */
	mosquitto_lib_init();
	broker.client = mosquitto_new( broker.ClientID, true, NULL);
	mosquitto_message_callback_set( broker.client,  msgarrived);
	mosquitto_disconnect_callback_set( broker.client, connlost);
	switch( mosquitto_connect(broker.client, broker.Host, broker.Port, 60) ){
	case MOSQ_ERR_SUCCESS : 
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
}


