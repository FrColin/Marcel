/* Meteo.h
 *	Publish meteo information.
 *
 *	Based on OpenWeatherMap.org information
 */

#ifndef METEO_H
#define METEO_H
#include <libconfig.h>
#include "Marcel.h"
#ifdef METEO

extern Module_t* config_Meteo3H(config_setting_t *cfg);
extern Module_t* config_MeteoD(config_setting_t *cfg);
extern void *process_Meteo3H(void *);
extern void *process_MeteoD(void *);

#endif
#endif
