/*
 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>
#include <unistd.h>

#include <ownetapi.h>
#include <pthread.h>
#include "OneWire.h"
#include "Marcel.h"
#include "MQTT_tools.h"

typedef struct _Device {	/* Storage for Device list */
	struct _Device *next;
	const char *Title;
	const char *Device;
    const char *Topic;
    int Sample;
} Device_t;

typedef struct _OneWire {
		Module_t module;
		Device_t *devices;	/* devices to read */
	} OneWire_t;

static OneWire_t ctx;

Module_t* config_OneWire(config_setting_t *cfg)
{
	if(verbose) printf("Entering section 'OneWire'\n");
	
	config_Module(cfg, &ctx.module);
	config_setting_t *devices = config_lookup_from(cfg, "Devices");
	if ( devices && config_setting_is_array(devices) ){
		return 0;
	}
	Device_t *dev = 0;
	for( int i = 0; i <config_setting_length(devices); i++ )
	{
		config_setting_t *elem = config_setting_get_elem(devices,i);
		Device_t *v = malloc(sizeof(Device_t));
		assert(v);
		v->next = dev;
		config_setting_lookup_string(elem, "Title", &(v->Title));
		v->Title = strdup(v->Title);
		config_setting_lookup_string(elem, "Device", &(v->Device));
		v->Device = strdup(v->Device);
        config_setting_lookup_string(elem, "Topic", &(v->Topic));
		v->Topic = strdup(v->Topic);
        config_setting_lookup_int(elem, "Sample", &(v->Sample));
		dev = v;
	}
	ctx.devices = dev;
	return &ctx.module;
}

/*
 * Processing
 */
void *process_OneWire(void *data){
	char l[MAXLINE];
    int owh = OWNET_init( "" );
	OWNET_set_temperature_scale('C');
	
	/* Sanity checks */
	// if(!ctx.module.topic){
		// fputs("*E* configuration error : no topic specified, ignoring this section\n", stderr);
		// pthread_exit(0);
	// }
	if(!ctx.devices){
		fprintf(stderr, "*E* configuration error : no devices specified for '%s', ignoring this section\n", ctx.module.topic);
		pthread_exit(0);
	}

	if(verbose)
		printf("Launching a processing flow for OneWire\n");

	for(;;){	/* Infinite loop to process messages */
		for(Device_t *dev = ctx.devices; dev ; dev = dev->next){
			if(OWNET_present( owh, dev->Device) != 0 ){
				if(verbose)
					perror( dev->Device );
				if(strlen(dev->Topic) + 7 < MAXLINE){  /* "/Alarm" +1 */
					int msg;
					char *emsg;
					strcpy(l, "Alarm/");
					strcat(l, dev->Topic);
					msg = strlen(l) + 2;

					if(strlen(dev->Device) + strlen(emsg = strerror(errno)) + 5 < MAXLINE - msg){ /* S + " : " + 0 */
						*(l + msg) = 'S';
						strcpy(l + msg + 1, dev->Device);
						strcat(l + msg, " : ");
						strcat(l + msg, emsg);

						mqttpublish( l, strlen(l + msg), l + msg, 0);
					} else if( strlen(emsg) + 2 < MAXLINE - msg ){	/* S + error message */
						*(l + msg) = 'S';
						strcpy(l + msg + 1, emsg);

						mqttpublish( l, strlen(l + msg), l + msg, 0);
					} else {
						char *msg = "Can't open file (and not enough space for the error)";
						mqttpublish( l, strlen(msg), msg, 0);
					}
				}
			} else {
				float val;
				char * read_data = NULL ;
    
				int read_length = OWNET_read(owh, dev->Device, &read_data);
				if(read_length < 0 || !sscanf(read_data, "%f", &val)){
					if(verbose)
						printf("OneWire : %s -> Unable to read a float value.\n", dev->Topic);
				} else {	/* Only to normalize the response */
					sprintf(l,"%.1f", val);

					mqttpublish(dev->Topic, strlen(l), l, 0 );
					if(verbose)
						printf("OneWire : %s -> %f\n", dev->Topic, val);
					 free( read_data);
				}
			}

			
		}

		sleep( ctx.module.sample );
	}

	pthread_exit(0);
	return 0;
}

