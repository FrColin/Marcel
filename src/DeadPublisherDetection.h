/* DeadPublisherDetection.h
 * 	Definitions related to DPD figures handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 09/07/2015	- LF - First version
 */

#ifndef DPD_H
#define DPD_H

#include "Marcel.h"

extern Module_t* config_DPD(config_setting_t *cfg);
extern void *process_DPD(void *);
#endif
