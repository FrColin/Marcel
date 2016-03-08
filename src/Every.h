/* Every.h
 * 	Definitions related to Every task
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 07/09/2015	- LF - First version
 */

#ifndef EVERY_H
#define EVERY_H

#include <libconfig.h>
#include "Marcel.h"

extern Module_t* config_Every(config_setting_t *cfg);
extern void *process_Every(void *);
#endif
