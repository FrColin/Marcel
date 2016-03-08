/* UPS.h
 * 	Definitions related to UPS figures handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */

#ifndef ONEWIRE_H
#define ONEWIRE_H

#ifdef ONEWIRE

#include "Marcel.h"

extern Module_t* config_OneWire(config_setting_t *cfg);
extern void *process_OneWire(void *);

#endif
#endif
