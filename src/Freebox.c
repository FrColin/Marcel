/* Freebox.c
 * 	Definitions related to Freebox figures handling
 *
 * This file is part of Marcel project and is following the same
 * license rules (see LICENSE file)
 *
 * 08/07/2015	- LF - start v2.0 - make source modular
 */
#ifdef FREEBOX

#include "Marcel.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <curl/curl.h>
#include <stdlib.h>             /* memory */
#include <string.h>             /* memcpy() */
#include <assert.h>
#include <unistd.h>             /* sleep() */
#include <limits.h>
#include <json-c/json.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "Freebox.h"
#include "MQTT_tools.h"

#define FREEBOX_URL "http://mafreebox.freebox.fr"

#define APP_ID "MarcelFbx"
#define APP_NAME "Marcel Freebox"
#define APP_VERSION "1.0"

static char _API_BASE_URL[100];
static char _API_VERSION;
static char _SESSION_HEADER[256];
static char _APP_TOKEN[100];

#define FBX_VERSION FREEBOX_URL"/api_version"

/* Curl's
 * Storing downloaded information in memory
 * From http://curl.haxx.se/libcurl/c/getinmemory.html example
 */

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

static struct json_object * _check_success(const char*url, struct json_object *jobj) {
	struct json_object *value = NULL;
	struct json_object *result = NULL;
	json_object_object_get_ex( jobj, "success", &value);
	if ( !value || json_object_get_boolean(value) != true)  {
		struct json_object *msg = NULL;
		struct json_object *error_code = NULL;
		json_object_object_get_ex( jobj, "msg", &msg);
		json_object_object_get_ex( jobj, "error_code", &error_code);
		fprintf(stderr, "*E* Freebox %s Failed : %s %s\n", url, json_object_get_string(msg), json_object_get_string(error_code) );
		return 0;
	}
	json_object_object_get_ex( jobj, "result", &result);
	return result;
}
static json_object *  _freebox_api_request(const char * url, json_object *data) {
	CURL *curl;
	CURLcode res;
	enum json_tokener_error jerr = json_tokener_success;

	struct MemoryStruct chunk;

	chunk.memory = malloc(1);
	chunk.size = 0;

	curl = curl_easy_init();

	if( !curl ){
		fprintf(stderr, "*E* Can't init curl : %s\n", strerror( errno ));
		fputs("*E* Stopping this thread\n", stderr);
		pthread_exit(0);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	if ( strlen(_SESSION_HEADER)) {
	/* Add a custom header */ 
	struct curl_slist *chunk = NULL;
	chunk = curl_slist_append(chunk, _SESSION_HEADER);
	if ( data ) {
		chunk = curl_slist_append(chunk, "Accept: application/json");
		chunk = curl_slist_append(chunk, "Content-Type: application/json");
	}
	/* set our custom set of headers */ 
	res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	}
	if ( data ) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_object_to_json_string(data));
	}
	if((res = curl_easy_perform(curl)) != CURLE_OK) {
		fprintf(stderr, "*E* Querying Freebox %s : %s\n", url, curl_easy_strerror(res));
		return NULL;
	}
	json_object * jobj = json_tokener_parse_verbose(chunk.memory, &jerr);
	if(jerr != json_tokener_success)
		fprintf(stderr, "*E* Querying Freebox %s Parse JSON error : %s\n", url, json_tokener_error_desc(jerr));
	else {
		return jobj;
	}
	

	curl_easy_cleanup(curl);
	return NULL;
}

static void  _check_freebox_api() {
		json_object * answer = _freebox_api_request(FBX_VERSION, NULL);

		if(answer) {
				struct json_object *api_version = NULL;
				struct json_object *api_base_url = NULL;
				json_object_object_get_ex( answer, "api_version", &api_version);
				json_object_object_get_ex( answer, "api_base_url", &api_base_url);
				_API_VERSION  = json_object_get_string(api_version )[0];
				strcpy(_API_BASE_URL,json_object_get_string(api_base_url ));
				if(verbose) {
					printf("Freebox : api: %s url:%s\n", json_object_get_string(api_version ), json_object_get_string(api_base_url ));
				}
			}

}

static json_object * call_freebox_api(const char* api_url, json_object *data) {
	char url[MAXLINE];
 
	sprintf(url, "%s%sv%c/%s", FREEBOX_URL, _API_BASE_URL,_API_VERSION, api_url);
	json_object * answer = _freebox_api_request(url,data);
	json_object *result = _check_success(url,answer);
	if (!result ) return NULL;
	return result;
}

static int login_freebox(char *app_id, char *app_version, char * app_token) {
	json_object *data;
	struct json_object *challenge = NULL;
	const char *strchallenge = NULL;
	unsigned char digest[20];
	char password[40];
	int md_len = 20;
	json_object * answer = call_freebox_api("login", NULL);
	if (!answer) return 0;
	json_object_object_get_ex( answer, "challenge", &challenge);
	strchallenge = json_object_get_string(challenge); 
	
	// Using sha1 hash engine here.
	// You may use other hash engines. e.g EVP_md5(), EVP_sha224, EVP_sha512, etc
	HMAC(EVP_sha1(), app_token, strlen(app_token), (unsigned char*)strchallenge, strlen(strchallenge), digest, &md_len);    
	
	// Be careful of the length of string with the choosen hash engine. SHA1 produces a 20-byte hash value which rendered as 40 characters.
	// Change the length accordingly with your choosen hash engine
	for(int i = 0; i < md_len; i++)
		sprintf(&password[i*2], "%02x", (unsigned int)digest[i]);

	data = json_object_new_object();
	json_object_object_add(data, "app_id", json_object_new_string(app_id));
	//json_object_object_add(data, "app_version", json_object_new_string(app_version));
	json_object_object_add(data, "password", json_object_new_string(password));
 
	answer=call_freebox_api ("login/session/",  data );
	if (!answer) return 0;
	struct json_object *session_token = NULL;
	json_object_object_get_ex( answer, "session_token", &session_token);
	strcpy(_SESSION_HEADER, "X-Fbx-App-Auth: ");
	strcat(_SESSION_HEADER, json_object_get_string(session_token));
	return 1;
}
static int authorize_application (const char *app_id ,const char *app_name,const char *app_version, const char *device_name) {
    char url[MAXLINE];
    json_object *answer;
	json_object *data;
	struct json_object *app_token = NULL;
	struct json_object *track_id = NULL;
	struct json_object *status = NULL;
	data = json_object_new_object();
	json_object_object_add(data, "app_id", json_object_new_string(app_id));
	json_object_object_add(data, "app_name", json_object_new_string(app_name));
	json_object_object_add(data, "app_version", json_object_new_string(app_version));
	json_object_object_add(data, "device_name", json_object_new_string(device_name));

	answer=call_freebox_api("login/authorize", data );

	json_object_object_get_ex( answer, "app_token", &app_token);
	json_object_object_get_ex( answer, "track_id", &track_id);
	sprintf(url, "login/authorize/%d",json_object_get_int(track_id));
	printf("Freebox : Please grant/deny access to the application on the Freebox LCD...\n");
	if ( verbose ) {
		printf("Freebox : app_token='%s', track_id=%d\n", json_object_get_string(app_token), json_object_get_int(track_id));
	}
	
	const char * current_status = "pending";
	int count = 30;
	while( strcmp("pending", current_status )==0  && count-- > 0) {
		sleep(5);
		answer=call_freebox_api(url, NULL);
		if ( !answer ) {
			fprintf(stderr, "*E* login/authorize error : no answer (%d)\n", count);
			continue;
		}
		json_object_object_get_ex( answer, "status", &status);
		if ( status ){
			current_status = json_object_get_string(status);
		}
		printf("Freebox : wait %s status %s (%d)...\n", url, current_status, count);
	} 
	if ( verbose ) printf( "Authorization %s\n", current_status);
	int success = strcmp("granted", current_status)==0;
	if (success) {
		strcpy(_APP_TOKEN, json_object_get_string(app_token) );
	}
	return success;

}
int loadFreeBoxApptoken( const char *file ){
   FILE *fp;
   char str[80];

   /* opening file for reading */
   fp = fopen(file , "r");
   if(fp == NULL) 
   {
      perror("Error opening file");
      return(-1);
   }
   if( fgets (str, sizeof(str), fp)!=NULL ) 
   {
      /* writing content to APP_TOKEN */
	  char* last_char = &str[strlen(str)-1];
	  if ( *last_char == '\n')
		*last_char = '\0'; // remove '\n'
      strcpy(_APP_TOKEN, str);
   }
   fclose(fp);
   
   return(0);
}
int saveFreeBoxApptoken( const char *file ){
	FILE *fp;
	char str[80];

	/* opening file for reading */
	fp = fopen(file , "w");
	if(fp == NULL) 
	{
	  perror("Error opening file");
	  return(-1);
	}
	fputs(_APP_TOKEN, fp);
	fclose(fp);

	return(0);
}

void *process_Freebox(void *actx){
	char l[MAXLINE];
	char hostname[HOST_NAME_MAX];
    struct _FreeBox *ctx = actx;	/* Only to avoid zillions of cast */
	
	gethostname(hostname, HOST_NAME_MAX);

	_SESSION_HEADER[0] = '\0';
	_APP_TOKEN[0] = '\0';
	/* Sanity checks */
	if(!ctx->topic){
		fputs("*E* configuration error : no topic specified, ignoring this section\n", stderr);
		pthread_exit(0);
	}
	/* read saved app_token */
	if( ctx->file ) {
		int ok = loadFreeBoxApptoken( ctx->file );
		if ( ok < 0 ) 
			fprintf(stderr,"*E* Freebox : reading App-token in '%s'\n", ctx->file);
	}
	if(verbose)
		printf("Launching a processing flow for Freebox\n");
	_check_freebox_api();
	
	if ( !strlen(_APP_TOKEN ) ) {
		if(verbose)
			printf("Freebox  : Authorization...\n");
		int auth = authorize_application (APP_ID, APP_NAME, APP_VERSION, hostname);
		if(!auth){
			fputs("*E* Freebox : App-Auth denied\n", stderr);
			pthread_exit(0);
		} else {
			saveFreeBoxApptoken( ctx->file );
		}
	}
	if(verbose)
		printf("Freebox : Login...\n");
	int login = login_freebox(APP_ID, APP_VERSION, _APP_TOKEN);
	if(!login){
		fputs("*E* Freebox : Login denied\n", stderr);
		pthread_exit(0);
	} else {
		if(verbose)
			printf("Freebox : Login successfull!\n");
	}
	
	for(;;){	/* Infinite loop to process data */
		json_object *answer;
		struct json_object *entry = NULL;
		const char * value;
		int lm;
		answer=call_freebox_api("connection/", NULL);
		if ( !answer ) {
			fprintf(stderr, "*E* connection/ error : no answer\n");
			continue;
		}
		json_object_object_foreach(answer, key, val) {
			
			value = json_object_get_string(val);
			lm = sprintf(l, "%s/%s/%s", ctx->topic, "connection", key) + 2;
			assert( lm+1 < MAXLINE-10 );
            strcpy( l+lm, value);
			mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
		}
		

#ifdef OLD
		if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
/*AF : Send error topic */
			perror("*E* Connecting");
		} else if( send(sockfd, FBX_REQ, strlen(FBX_REQ), 0) == -1 ){
/*AF : Send error topic */
			perror("*E* Sending");
		} else while( socketreadline(sockfd, l, sizeof(l)) != -1 ){
			if(strstr(l, "ATM")){
				int u, d, lm;
				if(sscanf(l+25,"%d", &d) != 1) d=-1;
				if(sscanf(l+44,"%d", &u) != 1) u=-1;

				lm = sprintf(l, "%s/DownloadATM", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", d );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);

				lm = sprintf(l, "%s/UploadATM", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", u );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);
			} else if(striKWcmp(l, "  Marge de bruit")){
				float u, d; 
				int lm;

				if(sscanf(l+25,"%f", &d) != 1) d = -1;
				if(sscanf(l+44,"%f", &u) != 1) u = -1;

				lm = sprintf(l, "%s/DownloadMarge", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%.2f", d );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);

				lm = sprintf(l, "%s/UploadMarge", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%.2f", u );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);
			} else if(striKWcmp(l, "  WAN")){
				int u, d, lm;

				if(sscanf(l+40,"%d", &d) != 1) d = -1;
				if(sscanf(l+55,"%d", &u) != 1) u = -1;

				lm = sprintf(l, "%s/DownloadWAN", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", d );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);

				lm = sprintf(l, "%s/UploadWAN", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", u );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);
			} else if(striKWcmp(l, "  Ethernet")){
				int u, d, lm;

				if(sscanf(l+40,"%d", &d) != 1) d = -1;
				if(sscanf(l+55,"%d", &u) != 1) u = -1;

				lm = sprintf(l, "%s/DownloadTV", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", d );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);

				lm = sprintf(l, "%s/UploadTV", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", u );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);
			} else if(striKWcmp(l, "  USB")){
				int u, d, lm;

				if(sscanf(l+40,"%d", &d) != 1) d = -1;
				if(sscanf(l+55,"%d", &u) != 1) u = -1;

				lm = sprintf(l, "%s/DownloadUSB", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", d );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);

				lm = sprintf(l, "%s/UploadUSB", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", u );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);
			} else if(striKWcmp(l, "  Switch")){
				int u, d, lm;

				if(sscanf(l+40,"%d", &d) != 1) d = -1;
				if(sscanf(l+55,"%d", &u) != 1) u = -1;

				lm = sprintf(l, "%s/DownloadLan", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", d );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);

				lm = sprintf(l, "%s/UploadLan", ctx->topic) + 2;
				assert( lm+1 < MAXLINE-10 );	/* Enough space for the response ? */
				sprintf( l+lm, "%d", u );
				mqttpublish( cfg.client, l, strlen(l+lm), l+lm, 0 );
				if(verbose)
					printf("Freebox : %s -> %s\n", l, l+lm);
			}
		}

		close(sockfd);
#endif
		sleep( ctx->sample );
	}

	pthread_exit(0);
}

#endif
