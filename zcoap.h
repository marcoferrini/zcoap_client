
#include "stdint.h"
#include "stddef.h"
#include <sys/time.h>

#include "vosal/vosal.h"
//#include "coap_dtls.h"
#include "coap.h"


typedef struct _zcoap_cfg_create {   //input create
    int timeout_s;
    unsigned int ping_seconds;
    char *srvr_uri;

    void *cn_call_back_arg;
    char *ca_file;
    char *key_file;
    char *cert_file;
    coap_pki_key_t key_type;
    coap_asn1_privatekey_type_t private_key_type; 

} zcoap_client_cfg_create_t;

typedef struct _coap_request
{
    //coap_optlist_t *optlist;
    //coap_pdu_t *req;
    uint8_t type;
    uint8_t code;
    
    //NEWCODE
    size_t block_wise_transfer_size;
   
    char* data;
    char* path;
    char* query; 
}zcoap_request_t; 

typedef struct _coap_response
{

    //coap_pdu_t *res;
    VSemaphore data_lock;   /*useful while doing observation */ 
    unsigned char* data; 
    unsigned int data_len; 
}zcoap_response_t;


int zcoap_client_create (zcoap_client_cfg_create_t *cfg);    
int zcoap_client_request (zcoap_request_t *request, int zc, zcoap_response_t *response, int observe);
int zcoap_client_destroy (int zc);
int zcoap_get_error(int zc);
int zcoap_client_finish (int zc);
int zcoap_client_observe(int zc,int obs_timeout_s ,char* topic, zcoap_response_t* response,size_t block_wise_transfer_size);
int zcoap_client_publish(int zc, char* topic, size_t block_wise_transfer_size);
