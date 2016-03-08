/* Marcel.h
 * 	Shared definition
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */

#ifndef MARCEL_H
#define MARCEL_H

#include <pthread.h>
#include <libconfig.h>

#define VERSION "4.4"	/* Need to stay numerique as exposed to Lua */

#define DEFAULT_CONFIGURATION_FILE "/usr/local/etc/Marcel.conf"
#define MAXLINE 1024	/* Maximum length of a line to be read */
#define BRK_KEEPALIVE 60	/* Keep alive signal to the broker */


typedef void * (*process_f) (void*);


typedef struct _Module {
	struct _Module *next;
	const char *name;	/* module name */
	int sample;			/* Delay b/w 2 queries */
	pthread_t thread;	/* Child to handle this section */
	const char *topic;	/* Root of the topics to publish to */
	process_f process;
	} Module_t;


typedef struct _Var {	/* Storage for var list */
	struct _Var *next;
	const char *name;
} Var_t;
/* The functions we will find in the plugin */
typedef  Module_t * (*config_f) (config_setting_t *cfg);


	/* Helper functions */
extern int verbose;
extern int config_Module(config_setting_t *cfg, Module_t *module);


	/* Broker related */
extern int papub( const char *, int, void *, int);

#endif

