/*
 * Marcel
 *	A daemon to publish some smart home data to MQTT broker
 *
 *	Compilation
gcc -std=c99 -lpthread -lpaho-mqtt3c -Wall Marcel.c -o Marcel
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
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

	/* PAHO library needed */ 
#include <MQTTClient.h>

#define VERSION "0.1"
#define DEFAULT_CONFIGURATION_FILE "/usr/local/etc/Marcel.conf"
#define MAXLINE 1024	/* Maximum length of a line to be read */
#define BRK_KEEPALIVE 60	/* Keep alive signal to the broker */

int debug = 0;

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
#define strdup(s) mystrdup(s)

char *extr_arg(char *s, int l){ 
/* Extract an argument from TéléInfo trame 
 *	s : argument string just after the token
 *	l : length of the argument
 */
	s++;	/* Skip the leading space */
	s[l]=0;
	return s;
}

	/*
	 * Configuration
	 */
struct Config {
	const char *Broker;
	MQTTClient client;
} cfg;

void read_configuration( const char *fch){
	FILE *f;
	char l[MAXLINE];
	char *arg;

	cfg.Broker = "tcp://localhost:1883";
	cfg.client = NULL;

	if(debug)
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
			if(debug)
				printf("Broker : '%s'\n", cfg.Broker);
		}
	}

	fclose(f);
}

int main(int ac, char **av){
	const char *conf_file = DEFAULT_CONFIGURATION_FILE;
	pthread_attr_t thread_attr;

	if(ac > 0){
		int i;
		for(i=1; i<ac; i++){
			if(!strcmp(av[i], "-h")){
				fprintf(stderr, "%s (%s)\n"
					"Publish Smart Home figures to an MQTT broker\n"
					"Known options are :\n"
					"\t-h : this online help\n"
					"\t-d : enable debug messages\n"
					"\t-f<file> : read <file> for configuration\n"
					"\t\t(default is '%s')\n",
					basename(av[0]), VERSION, DEFAULT_CONFIGURATION_FILE
				);
				exit(EXIT_FAILURE);
			} else if(!strcmp(av[i], "-d")){
				debug = 1;
				puts("Marcel (c) L.Faillie 2015");
				printf("%s (%s) starting ...\n", basename(av[0]), VERSION);
			} else if(!strncmp(av[i], "-f", 2))
				conf_file = av[i] + 2;
			else {
				fprintf(stderr, "Unknown option '%s'\n%s -h\n\tfor some help\n", av[i], av[0]);
				exit(EXIT_FAILURE);
			}
		}
	}
	read_configuration( conf_file );

	exit(EXIT_SUCCESS);
}

