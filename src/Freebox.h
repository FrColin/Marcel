/* Freebox.h
 * 	Definitions related to Freebox figures handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */

#ifndef FREEBOX_H
#define FREEBOX_H

#ifdef FREEBOX
#include <libconfig.h>
#include <json-c/json.h>

#include "Marcel.h"

extern Module_t* config_FreeboxOS(config_setting_t *cfg);
extern void *process_FreeboxOS(void *);
extern json_object * call_freebox_api(const char* api_url, json_object *data);
#endif
#endif
