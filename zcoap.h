
#include "stdint.h"
#include "stddef.h"
#include <sys/time.h>

#include "vosal/vosal.h"
#include "coap.h"


/**
 * @defgroup Zcoap_client
 * API functions for interfacing with libcoap
 * 
 */



/**
 * @brief The structure used as input for the create
 * 
 * 
 */
typedef struct _zcoap_cfg_create { 
    
    int timeout_s; /**< client timeout (s) */
    unsigned int ping_seconds;/**< minimum inactivity before sending a ping message  */
    char *srvr_uri;/**< server uri*/

    void *cn_call_back_arg; /**<For the DTLS setup: points to a user defined set of data that will get passed in to the validate_cn_call_back() function */ 
    char *ca_file; /**< For the DTLS setup: points to the CA File location on disk which will be in PEM format, or NULL.*/
    char *key_file; /**< For the DTLS setup: points to the Public Certificate location on disk which will be in PEM format.*/
    char *cert_file;  /**<For the DTLS setup:  points to the Private Key location on disk which will be in PEM format*/
    coap_pki_key_t key_type;/**< For the DTLS setup: defines the format that the certificates / keys are provided in*/
    coap_asn1_privatekey_type_t private_key_type; /**<For the DTLS setup: is the encoding type of the DER encoded ASN.1 definition of the Private Key.*/

} zcoap_client_cfg_create_t;

/**
 * @brief The structure used to define a request
 * 
 * 
 */
typedef struct _coap_request
{
    uint8_t type;/**<message type: CON, NON*/
    uint8_t code;/**<request method*/
    char* data; /**<data to be attached to the request*/
    char* path; /**<location of the requested resource*/
    char* query; 

    size_t block_wise_transfer_size; /**<if !=0 enable block wise transfers and specify the size of each block*/
}zcoap_request_t; 

/**
 * @brief The structure used to store information about the response 
 * 
 * 
 */
typedef struct _coap_response
{
    unsigned char* data; /**<points to a dynamic array containing the data of the response, is up to the user free the allocated memory*/
    unsigned int data_len; /**<size of the dynamic array*/
    VSemaphore data_lock;   /**<Semaphore used to avoid simultaneus data writings and readings: to be used only when doing observation*/ 
    uint8_t readable; /**<flag: 1 the data is valid, 0 is invalid. Useful only when doing observation with block wise tranfers*/
     
}zcoap_response_t;

/**
 * zcoap_client_create()
 * 
 * @brief setup function of the CoAP context and session with the server
 * 
 * @param cfg 
 * @return the slot in the coap_cliets[] array if successful, -1 if fails 
 */
int zcoap_client_create (zcoap_client_cfg_create_t *cfg);    

/**
 * zcoap_client_request
 * 
 * @brief Send a pdu with the specified properties
 * 
 * @param request The request properties
 * @param zc The slot of the to be used client 
 * @param response The structure where the response will be put into 
 * @param observe flag: 1 if an observation relationship need to be established, 0 if it is not
 * @return @c 1 if succesfull, @c 0 if fails
 */
int zcoap_client_request (zcoap_request_t *request, int zc, zcoap_response_t *response, int observe);

/**
 * zcoap_client_observe
 * 
 * @brief Establish a observation relationship with the server. The client will wait until the relationship expires 
 * 
 * @param zc The slot of the to be used client 
 * @param obs_timeout_s The observation timeout(s), it must be shorter than the client timeout
 * @param topic The path of the resource to observe 
 * @param response The structure where the response will be put into 
 * @param block_wise_transfer_size if !=0 enable block wise transfers and specify the size of each block
 * @return @c 1 if succesfull, @c 0 if fails
 */
int zcoap_client_observe(int zc,int obs_timeout_s ,char* topic, zcoap_response_t* response,size_t block_wise_transfer_size);

/**
 * 
 * zcoap_client_publish
 * 
 * @brief Updates the contents of a given resource
 * 
 * @param zc The slot of the to be used client 
 * @param topic The path of the resource to update
 * @param block_wise_transfer_size if !=0 enable block wise transfers and specify the size of each block
 * @return @c 1 if succesfull, @c 0 if fails
 */
int zcoap_client_publish(int zc, char* topic, size_t block_wise_transfer_size);

/**
 * zcoap_client_destroy
 * @brief Free the allocated memory to the given client
 * 
 * @param zc The slot of the to be used client 
 * @return @c 1 if succesfull, @c 0 if fails
 */
int zcoap_client_destroy (int zc);
