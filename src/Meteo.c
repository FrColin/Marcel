/* Meteo.c
 *	Publish meteo information.
 *
 *	Based on OpenWeatherMap.org information
 *
 * 29/11/2015 - Add /weather/acode as OWM's icon code is not accurate
 */

#ifndef METEO_H

#include <curl/curl.h>
#include <stdlib.h>		/* memory */
#include <string.h>		/* memcpy() */
#include <assert.h>
#include <unistd.h>		/* sleep() */
#include <json-c/json.h>
#include <pthread.h>

	/* This key has been provided by courtesy of OpenWeatherMap
	 * and is it reserved to this project.
	 * Be smart and request another one if you need your own key
	 */
#define URLMETEO3H "http://api.openweathermap.org/data/2.5/forecast?q=%s&mode=json&units=%s&lang=%s&appid=eeec13daf6e332c80ff3b648fbf628aa"
#define URLMETEOD "http://api.openweathermap.org/data/2.5/forecast/daily?q=%s&mode=json&units=%s&lang=%s&appid=eeec13daf6e332c80ff3b648fbf628aa"

	/* Curl's
	 * Storing downloaded information in memory
	 * From http://curl.haxx.se/libcurl/c/getinmemory.html example
	 */
	 
#include "Meteo.h"
#include "MQTT_tools.h"
#include "Marcel.h"

typedef struct _Meteo {
		Module_t module;
		int Daily;
		const char *City;	/* CityName,Country to query */
		const char *Units;	/* Result's units */
		const char *Lang;	/* Result's language */
	} Meteo_t;

	
struct MemoryStruct {
	char *memory;
	size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp){
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL){ /* out of memory! */ 
		fputs("not enough memory (realloc returned NULL)\n", stderr);
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;
 
	return realsize;
}

	/* Convert weather condition to accurate code */
static int convWCode( int code, int dayornight ){
	if( code >=200 && code < 300 )
		return 0;
	else if( code >= 300 && code <= 312 )
		return 9;
	else if( code > 312 && code < 400 )
		return( dayornight ? 39:45 );
	else if( code == 500 || code == 501 )
		return 11;
	else if( code >= 502 && code <= 504 )
		return 12;
	else if( code == 511 )
		return 10;
	else if( code >= 520 && code <= 529 )
		return( dayornight ? 39:45 );
	else if( code == 600)
		return 13;
	else if( code > 600 && code < 610 )
		return 14;
	else if( code == 612 || (code >= 620 && code < 630) )
		return( dayornight ? 41:46 );
	else if( code >= 610 && code < 620 )
		return 5;
	else if( code >= 700 && code < 800 )
		return 21;
	else if( code == 800 )
		return( dayornight ? 32:31 );
	else if( code == 801 )
		return( dayornight ? 34:33 );
	else if( code == 802 )
		return( dayornight ? 30:29 );
	else if( code == 803 || code == 804 )
		return( dayornight ? 28:27 );

	return -1;
}

Module_t* config_Meteo(config_setting_t *cfg)
{
	Meteo_t *mto = malloc(sizeof(Meteo_t));
	config_Module(cfg, &mto->module);
	config_setting_lookup_int(cfg,"Daily", &mto->Daily );
	config_setting_lookup_string(cfg,"City", &mto->City ); 
	mto->City = strdup( mto->City );
	config_setting_lookup_string(cfg,"Units", &mto->Units );
	mto->Units = strdup( mto->Units );
	config_setting_lookup_string(cfg,"Lang", &mto->Lang);
	mto->Lang = strdup( mto->Lang );
	if( strcmp( mto->Units, "metric" ) &&
				strcmp( mto->Units, "imperial" ) &&
				strcmp( mto->Units, "Standard" ) ){
				fputs("*F* Configuration issue : Units can only be only \"metric\" or \"imperial\" or \"Standard\"\n", stderr);
				return 0;
	}
	return &mto->module;
}

static void Meteo3H(Meteo_t *ctx){
	CURL *curl;
	enum json_tokener_error jerr = json_tokener_success;

	if(verbose)
		puts("*D* Querying Meteo 3H");

	if((curl = curl_easy_init())){
		char url[strlen(URLMETEO3H) + strlen(ctx->City) + strlen(ctx->Units) + strlen(ctx->Lang)];	/* Thanks to %s, Some room left for \0 */
		CURLcode res;
		struct MemoryStruct chunk;

		chunk.memory = malloc(1);
		chunk.size = 0;

		sprintf(url, URLMETEO3H, ctx->City, ctx->Units, ctx->Lang);
#if 0
		curl_easy_setopt(curl, CURLOPT_URL, "file:////home/laurent/Projets/Marcel/meteo_tst.json");
#else
		curl_easy_setopt(curl, CURLOPT_URL, url);
#endif
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Marcel/" VERSION);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

		if((res = curl_easy_perform(curl)) == CURLE_OK){	/* Processing data */
			json_object * jobj = json_tokener_parse_verbose(chunk.memory, &jerr);
			if(jerr != json_tokener_success)
				fprintf(stderr, "*E* Querying meteo : %s\n", json_tokener_error_desc(jerr));
			else {
				struct json_object *wlist = NULL;
				json_object_object_get_ex( jobj, "list", &wlist);
				if(wlist){
					int nbre = json_object_array_length(wlist);
					char l[MAXLINE];
					for(int i=0; i<nbre; i++){
						struct json_object *wo = NULL;
						struct json_object *wod = NULL;
						struct json_object *swod = NULL;
						wo = json_object_array_get_idx( wlist, i );	/* Weather's object */
						json_object_object_get_ex( wo, "dt" , &wod );	/* Weather's data */

						int lm = sprintf(l, "%s/%d/time", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%u", json_object_get_int(wod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "main", &wod );
						json_object_object_get_ex( wod, "temp", &swod);
						lm = sprintf(l, "%s/%d/temperature", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "pressure", &swod);
						lm = sprintf(l, "%s/%d/pressure", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "humidity", &swod);
						lm = sprintf(l, "%s/%d/humidity", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%d", json_object_get_int(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "weather", &wod );
						wod = json_object_array_get_idx( wod, 0 );
						json_object_object_get_ex( wod, "description", &swod);
						lm = sprintf(l, "%s/%d/weather/description", ctx->module.topic, i) + 2;
						const char *ts = json_object_get_string(swod);
						assert( lm+1 < MAXLINE-strlen(ts) );
						sprintf( l+lm, "%s", ts);
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "icon", &swod);
						lm = sprintf(l, "%s/%d/weather/code", ctx->module.topic, i) + 2;
						ts = json_object_get_string(swod);
						assert( lm+1 < MAXLINE-strlen(ts) );
						sprintf( l+lm, "%s", ts);
						mqttpublish( l, strlen(l+lm), l+lm, 1);
						int dayornight = (ts[2] == 'd');

							/* Accurate weathear icon */
						json_object_object_get_ex( wod, "id", &swod);
						lm = sprintf(l, "%s/%d/weather/acode", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%d", convWCode(json_object_get_int(swod), dayornight) );
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "clouds", &wod );
						json_object_object_get_ex( wod, "all", &swod);
						lm = sprintf(l, "%s/%d/clouds", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%d", json_object_get_int(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "wind", &wod );
						json_object_object_get_ex( wod, "speed", &swod);
						lm = sprintf(l, "%s/%d/wind/speed", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "deg", &swod);
						lm = sprintf(l, "%s/%d/wind/direction", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

					}
				}
			}
			json_object_put(jobj);
		} else
			fprintf(stderr, "*E* Querying meteo : %s\n", curl_easy_strerror(res));

			/* Cleanup */
		curl_easy_cleanup(curl);
		free(chunk.memory);
	}
}


static void MeteoD(Meteo_t *ctx){
	CURL *curl;
	enum json_tokener_error jerr = json_tokener_success;

	if(verbose)
		puts("*D* Querying Meteo daily");

	if((curl = curl_easy_init())){
		char url[strlen(URLMETEOD) + strlen(ctx->City) + strlen(ctx->Units) + strlen(ctx->Lang)];	/* Thanks to %s, Some room left for \0 */
		CURLcode res;
		struct MemoryStruct chunk;

		chunk.memory = malloc(1);
		chunk.size = 0;

		sprintf(url, URLMETEOD, ctx->City, ctx->Units, ctx->Lang);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Marcel/" VERSION);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

		if((res = curl_easy_perform(curl)) == CURLE_OK){	/* Processing data */
			json_object * jobj = json_tokener_parse_verbose(chunk.memory, &jerr);
			if(jerr != json_tokener_success)
				fprintf(stderr, "*E* Querying meteo daily : %s\n", json_tokener_error_desc(jerr));
			else {
				struct json_object *wlist = NULL;
				json_object_object_get_ex( jobj, "list", &wlist);
				if(wlist){
					int nbre = json_object_array_length(wlist);
					char l[MAXLINE];
					for(int i=0; i<nbre; i++){
						struct json_object *wo = NULL;
						struct json_object *wod = NULL;
						struct json_object *swod = NULL;
						wo = json_object_array_get_idx( wlist, i );	/* Weather's object */
						json_object_object_get_ex( wo, "dt", &wod );	/* Weather's data */

						int lm = sprintf(l, "%s/%d/time", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%u", json_object_get_int(wod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "temp", &wod );
						json_object_object_get_ex( wod, "day", &swod);
						lm = sprintf(l, "%s/%d/temperature/day", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "night", &swod);
						lm = sprintf(l, "%s/%d/temperature/night", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "eve", &swod);
						lm = sprintf(l, "%s/%d/temperature/evening", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "morn", &swod);
						lm = sprintf(l, "%s/%d/temperature/morning", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(swod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "weather", &wod );
						wod = json_object_array_get_idx( wod, 0 );
						json_object_object_get_ex( wod, "description", &swod);
						lm = sprintf(l, "%s/%d/weather/description", ctx->module.topic, i) + 2;
						const char *ts = json_object_get_string(swod);
						assert( lm+1 < MAXLINE-strlen(ts) );
						sprintf( l+lm, "%s", ts);
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wod, "icon", &swod);
						lm = sprintf(l, "%s/%d/weather/code", ctx->module.topic, i) + 2;
						ts = json_object_get_string(swod);
						assert( lm+1 < MAXLINE-strlen(ts) );
						sprintf( l+lm, "%s", ts);
						mqttpublish( l, strlen(l+lm), l+lm, 1);
						int dayornight = (ts[2] == 'd');

							/* Accurate weathear icon */
						json_object_object_get_ex( wod, "id", &swod);
						lm = sprintf(l, "%s/%d/weather/acode", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%d", convWCode(json_object_get_int(swod), dayornight) );
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "pressure", &wod );
						lm = sprintf(l, "%s/%d/pressure", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(wod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "clouds", &wod );
						lm = sprintf(l, "%s/%d/clouds", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%d", json_object_get_int(wod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "humidity", &wod );
						lm = sprintf(l, "%s/%d/humidity", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%d", json_object_get_int(wod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "speed", &wod );
						lm = sprintf(l, "%s/%d/wind/speed", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%.2lf", json_object_get_double(wod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);

						json_object_object_get_ex( wo, "deg", &wod );
						lm = sprintf(l, "%s/%d/wind/direction", ctx->module.topic, i) + 2;
						assert( lm+1 < MAXLINE-10 );
						sprintf( l+lm, "%d", json_object_get_int(wod));
						mqttpublish( l, strlen(l+lm), l+lm, 1);
					}
				}
			}
			json_object_put(jobj);
		} else
			fprintf(stderr, "*E* Querying meteo : %s\n", curl_easy_strerror(res));

			/* Cleanup */
		curl_easy_cleanup(curl);
		free(chunk.memory);
	}
}

void *process_Meteo(void *data){
	Meteo_t *ctx = (Meteo_t *)data;
	if(verbose)
		printf("Launching a processing flow for Meteo 3H\n");

	for(;;){
		if ( ctx->Daily )
			MeteoD(ctx);
			else Meteo3H(ctx);
		sleep( ctx->module.sample);
	}

	pthread_exit(0);
}


#endif
