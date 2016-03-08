/*
 * Marcel
 *	A daemon to publish smart home data to MQTT broker and rise alert
 *	if needed.
 *
 * Additional options :
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
 *	09/08/2015	- LF - 3.2 - all subscriptions are done in the main thread as it seems 
 *					paho is not thread safe.
 *	07/09/2015	- LF - 3.3 - Adding Every tasks.
 *				-------
 *	06/10/2015	- LF - switch to v4.0 - curl can be used in several "section"
 *	29/10/2015	- LF - 4.1 - Add meteo forcast
 *	29/11/2015	- LF - 4.2 - Correct meteo forcast icon
 *	31/01/2016	- LF - 4.3 - Add AlertCommand
 *	04/02/2016	- LF - 4.4 - Alert can send only a mail
 */
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
#include <curl/curl.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ownetapi.h>
#include <libconfig.h>

#include "Marcel.h"

#include "MQTT_tools.h"
#include "Alerting.h"

int verbose = 0;

Module_t *modules_list = 0;
Module_t *last_module = 0;

int loadPlugins( const char * plugin_name, config_f *conf, process_f *process )
{
	char * result;
	char buffer[80];
	void *plugin;
	sprintf(buffer,"plugins/%s.so", plugin_name );
	plugin = dlopen(buffer, RTLD_NOW|RTLD_GLOBAL);
	if (!plugin)
	{
	 fprintf(stderr,"Cannot load %s\n", dlerror ());
	 return 0;
	}
	sprintf(buffer,"config_%s", plugin_name );
	*conf = dlsym(plugin, buffer);
	result = dlerror();
	if (result)
	{
	   fprintf(stderr,"Cannot find %s in %s: %s\n", buffer, plugin_name, result);
	   return 0;
	}
	sprintf(buffer,"process_%s", plugin_name );
	*process = dlsym (plugin, buffer);
	result = dlerror();
	if (result)
	{
		fprintf(stderr,"Cannot find %s in %s: %s\n", buffer, plugin_name, result);
		return 0;
	}
	return 1;
}
	/*
	 * Configuration
	 */
int config_Module(config_setting_t *cfg, Module_t *module){
	if ( config_setting_lookup_string(cfg, "Topic", &module->topic )== CONFIG_TRUE)
		module->topic = strdup(module->topic);
	config_setting_lookup_int(cfg, "Sample", &module->sample);
	if(verbose) {
		printf("\tTopic : '%s'\n", module->topic);
		printf("\tDelay between samples : %ds\n", module->sample);
	}
	return 1;	
}

static int read_configuration( const char *fch){
	int len = 0;
	config_t config;
	config_init (&config);
	
	if(verbose)
		printf("Reading configuration file '%s'\n", fch);

	if (!config_read_file (&config, fch) )
	{
		fprintf(stderr, "%s:%d - %s\n", 
				config_error_file(&config),
                config_error_line(&config), 
				config_error_text(&config));
        config_destroy(&config);
		exit(EXIT_FAILURE);
	}
	config_setting_t *broker_cfg = config_lookup(&config, "Broker");
	configure_Broker(broker_cfg);
	config_setting_t *alerting_cfg = config_lookup(&config, "Alert");
	configure_Alerting(alerting_cfg);
	config_setting_t *modules_cfg = config_lookup(&config, "plugins");
	if ( modules_cfg ) {
		len = config_setting_length (modules_cfg);
		if(verbose)
				 fprintf(stderr,"%d plugins\n",len);
		for ( int index =0; index < len ; index ++ ){
			const config_setting_t * setting = config_setting_get_elem(modules_cfg, index );
			const char *name = config_setting_name(setting);
			config_setting_t *module_cfg = config_lookup_from(modules_cfg,name);
			if ( module_cfg ) {
				config_f conf;
				process_f process;
				if(verbose)
				 fprintf(stderr,"Loading plugin %s\n",name);
			
				int loaded  = loadPlugins( name, &conf, &process );
				if ( loaded ) {
					if(verbose)
						fprintf(stderr,"Configuring plugin %s\n",name);
					/* keep order of confile */
					Module_t *module =  conf(module_cfg);
					if ( last_module ) {
						last_module->next = module;
					}
					last_module = module;
					module->name = strdup( name );
					module->process = process;
					if ( modules_list == 0 )
						modules_list = module;
				}
			}
		}
	} 
	config_destroy(&config); // strdup all config string
	return len;
}




static void handleInt(int na){
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv){
	const char *conf_file = DEFAULT_CONFIGURATION_FILE;
	pthread_attr_t thread_attr;
	int c;

	while ((c = getopt (argc, argv, "vhf:")) != -1)
		switch (c)
			{
			  case 'v':
				verbose = 1;
				puts("Marcel (c) L.Faillie 2015");
				puts("       (c) Fr.Colin  2016");
				printf("%s v%s starting ...\n", basename(argv[0]), VERSION);
				break;
			  case 'h':
				fprintf(stderr, "%s (%s)\n"
							"Publish Smart Home figures to an MQTT broker\n"
							"Known options are :\n"
							"\t-h : this online help\n"
							"\t-v : enable verbose messages\n"
							"\t-f <file> : read <file> for configuration\n"
							"\t\t(default is '%s')\n",
							basename(argv[0]), VERSION, DEFAULT_CONFIGURATION_FILE
						);
				exit(EXIT_FAILURE);
				break;
			  case 'f':
				conf_file = optarg;
				break;
			  default:
				fprintf(stderr, "Unknown option '%c'\n%s -h\n\tfor some help\n", c, argv[0]);
				exit(EXIT_FAILURE);
		  }

	char *copy_cf;

	assert( copy_cf = strdup(conf_file) );
	if(chdir( dirname( copy_cf )) == -1){
		perror("chdir() : ");
		exit(EXIT_FAILURE);
	}
	free( copy_cf );
	int nbmodules = read_configuration( conf_file );
	if ( !nbmodules ) {
		fprintf(stderr, "No modules found exiting..\n");
		exit(EXIT_FAILURE);
	}
	if(verbose)
		puts("\n End reading configuration\n");
	/* Curl related */
	curl_global_init(CURL_GLOBAL_ALL);
	atexit( curl_global_cleanup );
	/* broker */
	init_Broker();
	/* alerting */
	init_Alerting();

		/* Creating childs */
	if(verbose)
		puts("\nCreating childs processes\n"
			   "---------------------------");

	assert(!pthread_attr_init (&thread_attr));
	assert(!pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED));
	for(Module_t *mod = modules_list; mod; mod = mod->next){
		if ( verbose ) fprintf(stdout,"Start Thread %s\n",mod->name);
		if(pthread_create( &(mod->thread), &thread_attr, mod->process, mod) < 0){
				fprintf(stderr,"*F* Can't create a %s processing thread\n", mod->name);
				exit(EXIT_FAILURE);
			}
	}

	signal(SIGINT, handleInt);
	pause();

	exit(EXIT_SUCCESS);
}

#ifdef OLD

		case MSEC_DEADPUBLISHER:
			if(!s->common.topic){
				fputs("*E* configuration error : no topic specified, ignoring this section\n", stderr);
			} else if(!*s->DeadPublisher.errorid){
				fprintf(stderr, "*E* configuration error : no errorid specified for DPD '%s', ignoring this section\n", s->common.topic);
			} else if(!s->common.sample && !s->DeadPublisher.funcname){
				fputs("*E* DeadPublisher section without sample time or user function defined : ignoring ...\n", stderr);
			} else {
				if(mosquitto_subscribe( cfg.client, NULL, s->common.topic, 0 ) != MOSQ_ERR_SUCCESS ){
					fprintf(stderr, "Can't subscribe to '%s'\n", s->common.topic );
				} else if(pthread_create( &(s->common.thread), &thread_attr, process_DPD, s) < 0){
					fputs("*F* Can't create a processing thread\n", stderr);
					exit(EXIT_FAILURE);
				}
			}
			break;


#endif

