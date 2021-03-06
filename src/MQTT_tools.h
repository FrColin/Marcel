/* MQTT_tools.h
 *
 * Definition for tools useful for MQTT processing
 *
 * 13/07/2015 LF : First version
 */

#ifndef MQTT_TOOL_H
#define MQTT_TOOL_H

#include <libconfig.h>

/* Compare 2 strings like strcmp() but s can contain MQTT wildcards
 * '#' : replace remaining of the line
 * '+' : a sub topic and must be enclosed by '/'
 *
 *  
 * Wildcards are checked as per mosquitto's source code rules
 * (comment in http://git.eclipse.org/c/mosquitto/org.eclipse.mosquitto.git/tree/src/subs.c)
 *
 * <- 0 : strings match
 * <- -1 : wildcard error
 * <- others : strings are different
 */
extern int mqtttokcmp(const char *s,  const char *t);

extern void configure_Broker(config_setting_t *cfg);
extern void init_Broker(void);
extern const char * mqttgetclientID();
/* Publish an MQTT message */
extern int mqttpublish( const char *topic, size_t length, void *payload, int retained );
/* Subscribe an MQTT message */
extern int mqttsubscribe( const char *topic );
#endif
