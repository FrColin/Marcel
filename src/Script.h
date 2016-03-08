/* Script.h
 * 	Definitions related to Lua Script handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */

#ifndef SCRIPT_H
#define SCRIPT_H

#ifdef LUA
#include <libconfig.h>

extern int config_Script(config_setting_t *cfg);
extern void *process_Script(void *);
extern const char*getLuaUserFuncScript();
#endif
#endif
