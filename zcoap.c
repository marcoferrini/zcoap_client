
#include "libcoap.h"
#include "coap_dtls.h"
#include "coap.h"

#include "stdlib.h"


#include <netdb.h>
#include <sys/param.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "nvs_flash.h"

#include "vosal/vosal.h"


#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


#include "platform/zcoap.h"

const static char *TAG = "CoAP_client";

#define BUFSIZE 40
#define COAP_DEFAULT_TIME_SEC 5
#define ZCOAP_CLIENT_NUM 2

typedef struct _zcoap_config  
{

    void *cn_call_back_arg;
    char* ca_file;   
    char *key_file;
    char *cert_file;

    coap_pki_key_t key_type;
    coap_asn1_privatekey_type_t private_key_type; 

    int timeout_s; 
    unsigned int ping_seconds;   
    char *server_uri;
    
    
}zcoap_client_config_t;

typedef struct _zcoap_client
{
    int used;

    int resp_wait;
    
    int doing_observe;
    int obs_started; 

    int timeout_ms;
    int timeout_s;
    
    int obs_ms; 
    int obs_s;

    int token; 
    int last_observe_token; //last token used to establish a observation relationship 
    int last_used_token; //last token used
    
    coap_uri_t dst_uri;
    zcoap_response_t *response;
    zcoap_request_t *request;
    coap_session_t *session;
    coap_context_t * ctx;
    
    uint8_t *cert_mem;
    uint8_t *key_mem; 
    uint8_t *ca_mem;


}zcoap_client_t;

static zcoap_client_t coap_clients[ZCOAP_CLIENT_NUM];

coap_pdu_t* zcoap_new_request(int zc, int observe, coap_block_t *block_wise_transfer){

    zcoap_client_t *zh = &coap_clients[zc];

    uint8_t token[4]; 
    
    coap_pdu_t* pdu_to_be_sent = coap_new_pdu(zh->session); 
    coap_optlist_t* optlist; 

    
    int res; 
    unsigned char _buf[BUFSIZE];
    unsigned char *buf;
    size_t buflen;

    
    uint16_t opt = 0; /*it could be interpreted as COAP_MESSAGE_CON but it should not create any problem */
    unsigned int opt_length; 

    size_t size = 0;
    char* temp;

    
    pdu_to_be_sent->tid = coap_new_message_id(zh->session);
    pdu_to_be_sent->code = zh->request->code;
    pdu_to_be_sent->type = zh->request->type;
   
   /***********************************token**********************************************/

    switch (observe)
    {
    case -1:  //clear observe 
        zh->token = zh->last_observe_token;
        break;
    case 1: //set observe 
        zh->last_observe_token = ++zh->token; 
        zh->last_used_token = zh->token; 
        break;

    default:
        zh->token = zh->last_used_token + 1;
        zh->last_used_token = zh->token; 
        break;
    }
    

    token[0] = (zh->token >> 24) & 0xFF;
    token[1] = (zh->token >> 16) & 0xFF;
    token[2] = (zh->token >> 8) & 0xFF;
    token[3] = (zh->token) & 0xFF;


    if(!coap_add_token(pdu_to_be_sent, sizeof(zh->token),(uint8_t*)token)) {
        ESP_LOGE(TAG, "zcoap_new_request, coap_add_tocken() has failed");
        return NULL;
    }
    
    /***********************************option**********************************************/

    //creating a new optlist and adding host
    optlist = coap_new_optlist(COAP_OPTION_URI_HOST, zh->dst_uri.host.length, zh->dst_uri.host.s);
    
    if(!optlist){
        ESP_LOGE(TAG, "zcoap_new_request, coap_new_optilist has failed ");
    }
    
    //adding path 
    if(zh->dst_uri.path.length){  
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_path(zh->dst_uri.path.s, zh->dst_uri.path.length, buf, &buflen);

        while (res--)
        {
            if(!coap_insert_optlist(&optlist, coap_new_optlist(COAP_OPTION_URI_PATH,
                                                     coap_opt_length(buf),
                                                     coap_opt_value(buf)))){
                                                         ESP_LOGE(TAG, "zcoap_new_request, coap_insert_optlist has failed, path"); 
                                                     }
            buf += coap_opt_size(buf);

        }
        
    }

    if(zh->dst_uri.query.length){   
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_query(zh->dst_uri.query.s, zh->dst_uri.query.length, buf, &buflen);

        while (res--) {
                if(!coap_insert_optlist(&optlist,
                                    coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                     coap_opt_length(buf),
                                                     coap_opt_value(buf)))){
                                                        ESP_LOGE(TAG, "zcoap_client_request, coap_insert_optlist has failed, path");
                                                     }

                buf += coap_opt_size(buf);
            }
    }

    //eventually observe option 

    if ((pdu_to_be_sent->code == COAP_REQUEST_GET))
    {
        if(observe >0){
            buflen = BUFSIZE;
            buf = _buf;
            if (! coap_insert_optlist(&optlist, coap_new_optlist(COAP_OPTION_OBSERVE,
                                    coap_encode_var_safe(buf, sizeof(buf), COAP_OBSERVE_ESTABLISH), buf)))
            {
                ESP_LOGE(TAG, "zcoap_new_request(), coap_insert_optlist() has failed");
                return NULL;
            }

            zh->doing_observe = 1;
        }else if(observe < 0){
            buflen = BUFSIZE;
            buf = _buf;
            if (! coap_insert_optlist(&optlist, coap_new_optlist(COAP_OPTION_OBSERVE,
                                    coap_encode_var_safe(buf, sizeof(buf), COAP_OBSERVE_CANCEL), buf)))
            {
                ESP_LOGE(TAG, "zcoap_new_request(), coap_insert_optlist() has failed");
                return NULL;
            }
        }
        
    }

    /*evaluating size of the payload*/
    size = 0;
    temp = zh->request->data;
    if(temp){
        while (*temp++){
            ++size;
        }
    }

    /*adding block1 and block2 tranfer, for the block1 option we have to know the size of the payload, stored at this point in the variable size */
    if(block_wise_transfer->szx){
        
        buf = _buf;

        if(zh->request->code != COAP_REQUEST_DELETE){
            opt = zh->request->code == COAP_REQUEST_GET ? COAP_OPTION_BLOCK2 : COAP_OPTION_BLOCK1;
            block_wise_transfer->m = (opt == COAP_OPTION_BLOCK1) && (coap_more_blocks(size, block_wise_transfer->num , block_wise_transfer->szx));

            opt_length = coap_encode_var_safe(buf, sizeof(buf),
                                (block_wise_transfer->num << 4 | block_wise_transfer->m << 3 | block_wise_transfer->szx));

            coap_insert_optlist(&optlist, 
                                coap_new_optlist(opt,opt_length, buf));


            /*adding size2 in case of block2 transfer to receive from the server a size estimate of the resource*/
            if(opt == COAP_OPTION_BLOCK2){
                opt = COAP_OPTION_SIZE2; 
                opt_length = coap_encode_var_safe(buf, sizeof(buf), 0); /*the value of the option in set to zero according to RFC7959 4.0*/
                coap_insert_optlist(&optlist, 
                                    coap_new_optlist(opt,opt_length, buf)); 
            } else{   
                opt_length = coap_encode_var_safe(buf, sizeof(buf), size); 
                coap_insert_optlist(&optlist, 
                                    coap_new_optlist(COAP_OPTION_SIZE1,opt_length, buf)); 

            }
        }
    }
    
    coap_add_optlist_pdu(pdu_to_be_sent, &optlist);

    /***********************************payload**********************************************/
    
    
    if(zh->request->data){
        if(opt == COAP_OPTION_BLOCK1){
            if (!coap_add_block(pdu_to_be_sent, size,(uint8_t*)zh->request->data, block_wise_transfer->num, block_wise_transfer->szx))
            {
               ESP_LOGE(TAG, "zcoap_new_request(), coap_add_block() has failed");
                return NULL; 
            }
            
        }else{
            if(!coap_add_data(pdu_to_be_sent, size,(uint8_t*)zh->request->data)){
            ESP_LOGE(TAG, "zcoap_new_request(), coap_add_data() has failed");
            return NULL; 
            }
        }
        
    }
    coap_delete_optlist(optlist); 
    return pdu_to_be_sent; 

}

/*return the coap_address corresponding to the input coap_uri*/
void zcoap_get_coapaddr(coap_uri_t* coap_uri, coap_address_t* coap_addr){ 

    //coap_address_t coap_addr;
    char* hostname = NULL; 
    struct hostent *hp;
    
    //from the hostname to the IP address
    hostname = (char*)calloc(1,coap_uri->host.length+1);
        if(hostname == NULL){
            ESP_LOGE(TAG, "zcoap_get_coapaddr(), calloc failed");
            coap_addr->size = 0;
            return;
        
        }
    

    memcpy(hostname, coap_uri->host.s, coap_uri->host.length);
    hp = gethostbyname(hostname);
    free(hostname);

    if (hp == NULL) {
        ESP_LOGE(TAG, "zcoap_get_coapaddr(), DNS lookup failed, error = %i",h_errno);
        coap_addr->size = 0;
        return;
    }
    char tmpbuf[INET6_ADDRSTRLEN];
    coap_address_init(coap_addr);
    switch (hp->h_addrtype) {
        case AF_INET:
            coap_addr->addr.sin.sin_family      = AF_INET;
            coap_addr->addr.sin.sin_port        = htons(coap_uri->port);
            memcpy(&coap_addr->addr.sin.sin_addr, hp->h_addr, sizeof(coap_addr->addr.sin.sin_addr));
            inet_ntop(AF_INET, &coap_addr->addr.sin.sin_addr, tmpbuf, sizeof(tmpbuf));
            ESP_LOGI(TAG, "zcoap_get_coapaddr, DNS lookup succeeded. IP=%s", tmpbuf);
            break;
        case AF_INET6:  
            coap_addr->addr.sin6.sin6_family      = AF_INET6;
            coap_addr->addr.sin6.sin6_port        = htons(coap_uri->port);
            memcpy(&coap_addr->addr.sin6.sin6_addr, hp->h_addr, sizeof(coap_addr->addr.sin6.sin6_addr));
            inet_ntop(AF_INET6, &coap_addr->addr.sin6.sin6_addr, tmpbuf, sizeof(tmpbuf));
            ESP_LOGI(TAG, "zcoap_get_coapaddr, DNS lookup succeeded. IP=%s", tmpbuf);
            break;
        default:
            ESP_LOGE(TAG, "zcoap_get_coapaddr, DNS lookup response failed");
            coap_addr->size = 0;
            return;
    }
    return;
        
}

static int
verify_cn_callback(const char *cn,
                   const uint8_t *asn1_public_cert,
                   size_t asn1_length,
                   coap_session_t *session,
                   unsigned depth,
                   int validated,
                   void *arg
                  )
{
    coap_log(LOG_INFO, "CN '%s' presented by server (%s)\n",
             cn, depth ? "CA" : "Certificate");
    return 1;
}

static uint8_t *read_file_mem(const char* filename, size_t *length) {
  FILE *f;
  uint8_t *buf;
  struct stat statbuf;

  *length = 0;
  if (!filename || !(f = fopen(filename, "r")))
    return NULL;

  if (fstat(fileno(f), &statbuf) == -1) {
    fclose(f);
    return NULL;
  }

  buf = coap_malloc(statbuf.st_size+1);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  if (fread(buf, 1, statbuf.st_size, f) != (size_t)statbuf.st_size) {
    fclose(f);
    coap_free(buf);
    return NULL;
  }
  buf[statbuf.st_size] = '\000';
  *length = (size_t)(statbuf.st_size + 1);
  fclose(f);
  return buf;
}

static void nack_handler(coap_context_t* context, coap_session_t *session, coap_pdu_t *sent,
                            coap_nack_reason_t reason,const coap_tid_t id){
    
    zcoap_client_t* zh = NULL;
    for (int i = 0; i < ZCOAP_CLIENT_NUM; i++)
    {
        if (coap_clients[i].used && coap_clients[i].ctx == context && coap_clients[i].session == session)
        {
            zh = &coap_clients[i];
            break;
        
        }
        
    }
    switch (reason)
    {
        case COAP_NACK_TOO_MANY_RETRIES:
        case COAP_NACK_NOT_DELIVERABLE:
        case COAP_NACK_RST:
        case COAP_NACK_TLS_FAILED:
            zh->resp_wait = 0;
            break;
        default:
            ;
        
    }
    return;
}

static void response_handler(coap_context_t *ctx, coap_session_t *session,  coap_pdu_t *sent, 
                                coap_pdu_t *received, const coap_tid_t id)
{   


    unsigned char *data = NULL;
    size_t data_len = 0;

    unsigned char *temp_data_pointer= NULL; 
    unsigned int block_num; 

    int block2_flag; 
    
    coap_pdu_t *pdu = NULL;
    coap_opt_t *block_opt = NULL;

    coap_opt_t* opt = NULL;
    coap_opt_iterator_t opt_iter;

    coap_tid_t tid;

    coap_block_t block;


    //printing for debugging purposes 
    ESP_LOGD(TAG, "response_handler() has started");

    zcoap_client_t* zh = NULL;
    int slot; 
    for (slot = 0; slot < ZCOAP_CLIENT_NUM; slot++)
    {
        if (coap_clients[slot].used && coap_clients[slot].ctx == ctx && coap_clients[slot].session == session)
        {
            zh = &coap_clients[slot];
            break;
        
        }
        
    }
  /***********************************token check**********************************************/

   //WARNING: check token, works only when the tokens are generated sequentially, there must be security layer
    if(*received->token > (uint8_t)zh->token){
        ESP_LOGE(TAG, "unscheduled message");
        //drop if this was just some message, or send RST in case of notification
        if (!sent && (received->type == COAP_MESSAGE_CON ||
                received->type == COAP_MESSAGE_NON))
            coap_send_rst(session, received);
        return;
    }


    if(received->type == COAP_MESSAGE_RST)
    {
        ESP_LOGI(TAG, "got a reset \n");
        zh->resp_wait = 0; 
        return; 
    } 


    /***********************************response computing**********************************************/
    if (COAP_RESPONSE_CLASS(received->code) == 2)
    {
    //success
        /* set obs timer if we have successfully subscribed a resource */
        if (zh->doing_observe && !zh->obs_started &&
            coap_check_option(received, COAP_OPTION_OBSERVE, &opt_iter)){
                ESP_LOGI(TAG,"observation relationship established, set timeout to %d (s) \n",zh->obs_s);
                zh->obs_started = 1;
                zh->obs_ms = zh->obs_s*1000;
                

            }

        /* Need to see if blocked response */
        block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);
        block2_flag = 1; 
        if (!block_opt)
        {
            block2_flag = 0; 
            block_opt = coap_check_option(received, COAP_OPTION_BLOCK1, &opt_iter);
        }
        /*if there is no block 1 or block 2 option block2_flag will be set to 1 but not used*/
        if (block_opt){
            uint16_t blktype = opt_iter.type;

            if (coap_opt_block_num(block_opt) == 0) {
                zh->resp_wait = zh->doing_observe ? !coap_check_option(received,
                                  COAP_OPTION_OBSERVE, &opt_iter) == NULL : 0;
            }

            /*retriving the data*/
            if (coap_get_data(received, &data_len, &data)) {
                printf("Received: %.*s\n", (int)data_len, data);
            }
            
            if(block2_flag){
                    block_num = coap_opt_block_num(block_opt);

                    if(!block_num ){ /*check if it is the first block of the sequence*/
                        if(COAP_OPT_BLOCK_MORE(block_opt)){
                            /*first but not last*/
                            opt = coap_check_option(received, COAP_OPTION_SIZE2, &opt_iter);
                            if(!opt){
                                ESP_LOGE(TAG, "no size2 option in the received message");
                            }
                            size_t opt_length = coap_opt_length(opt); 
                            uint32_t size2; 

                            switch (opt_length)
                            {
                            case 1:
                                size2 = *coap_opt_value(opt);
                                break;
                            case 2:
                                size2 = *coap_opt_value(opt) << 8 | *(coap_opt_value(opt) + 1); 
                                break;
                            case 3: 
                                size2 = *coap_opt_value(opt)<<16|*(coap_opt_value(opt)+1) << 8 | *(coap_opt_value(opt) + 2); 
                                break;
                            case 4: 
                                size2 = *coap_opt_value(opt)<< 24|*(coap_opt_value(opt)+1)<<16 | *(coap_opt_value(opt)+2) << 8 | *(coap_opt_value(opt) + 3);
                            default:
                                size2 = 0; 
                                break;
                            }
                        
                            if(zh->obs_started){
                                vosSemWait(zh->response->data_lock);
                                zh->response->readable = 0; 
                            }
                            
                            zh->response->data = realloc(zh->response->data, size2);
                            memcpy(zh->response->data, data, data_len); 

                            if(zh->obs_started)
                                vosSemSignal(zh->response->data_lock); 
                            zh->response->data_len = size2;

                            
                            /*ESP_LOGD(TAG, "size2 = %u", size2);
                            ESP_LOGD(TAG, "lenght = %u", opt_length);*/
                            
                        }else{  /*first and last*/
                            if(zh->obs_started)
                                vosSemWait(zh->response->data_lock);
                            zh->response->data = realloc(zh->response->data, data_len);
                            memcpy(zh->response->data, data, data_len);
                            zh->response->data_len = data_len;
                            if(zh->obs_started)
                                vosSemSignal(zh->response->data_lock);
                        }
                        

                    }else{ /*it is not the first block of the sequence so zh->response->data is already pointing to a dinamic array of the right size*/
                        if(zh->obs_started)
                                vosSemWait(zh->response->data_lock);
                        temp_data_pointer = zh->response->data + (COAP_OPT_BLOCK_SZX(block_opt)<<5)*block_num; 
                        /*now temp_data_pointer should point to the correct slot of the array*/
                        memcpy(temp_data_pointer, data, data_len); 
                        if(zh->obs_started)
                                vosSemSignal(zh->response->data_lock);
                    }
                
            }

            /*check if there is more*/
            if (COAP_OPT_BLOCK_MORE(block_opt)) {

                //input of zcoap_new_request
                 
                block.szx = COAP_OPT_BLOCK_SZX(block_opt);
                block.num = coap_opt_block_num(block_opt);
                if(!block2_flag){
                    if(block.szx != zh->request->block_wise_transfer_size){
                        unsigned int bytes_sent = ((block.num +1)<<(zh->request->block_wise_transfer_size + 4));
                        if(bytes_sent % (1<< (block.szx+4))==0){
                            /* Recompute the block number of the previous packet given the new block size */
                            block.num = (bytes_sent >> (block.szx+4)) - 1; 
                            zh->request->block_wise_transfer_size = block.szx; 
                        }else{
                            ESP_LOGD(TAG,"ignoring request to increase Block1 size, "
                                "next block is not aligned on requested block size boundary. " );
                        }
                    }
                } 
                block.num++; 

                pdu = zcoap_new_request(slot, 0, &block); 
                if(!pdu){
                    ESP_LOGE(TAG, "zcoap_new_request_has_failed"); 
                    goto end; 
                }
            
                /*printing for debugging purposes*/
                //coap_show_pdu(LOG_EMERG, pdu);

                tid = coap_send(session, pdu);

                if (tid != COAP_INVALID_TID) {
                    zh->resp_wait = 1;
                    zh->timeout_ms = zh->timeout_s * 1000;
                }

            }else{
                zh->response->readable = 1;
                zh->resp_wait = zh->doing_observe ? zh->obs_started : 0;
            }

            return;
            printf("\n");
        }else{
            if (coap_get_data(received, &data_len, &data)) {
                printf("Received: %.*s\n", (int)data_len, data);
            }
            if(zh->obs_started){
                zh->response->readable = 1;
                vosSemWait(zh->response->data_lock);
            }
            zh->response->data = realloc(zh->response->data, data_len);
            memcpy(zh->response->data, data, data_len); 
            zh->response->data_len = data_len;
            if(zh->obs_started)
                vosSemSignal(zh->response->data_lock);
        }
        
         

    
    }else if (COAP_RESPONSE_CLASS(received->code)== 4){
        ESP_LOGE(TAG, "client error");

    }else if (COAP_RESPONSE_CLASS(received->code)== 5){
        ESP_LOGE(TAG, "server error");
    }

end: 
    zh->resp_wait = zh->doing_observe ? zh->obs_started : 0;
    return;
}

int zcoap_client_create (zcoap_client_cfg_create_t *cfg)
{
    int i, slot = -1;     
    for(i = 0; i < ZCOAP_CLIENT_NUM; i++) {
        if (!coap_clients[i].used) {
            slot = i;
            break;
        }
    }


    if(slot < 0){  
        return -1;
    }
    
    
    coap_context_t* context = NULL; 
    coap_session_t*  coap_session = NULL; 
    
    zcoap_client_config_t config = {0};
    coap_uri_t uri;  

    coap_dtls_pki_t dtls_pki;
    char client_sni[256];

    uint8_t *cert_mem = NULL;
    uint8_t *key_mem = NULL; 
    uint8_t *ca_mem = NULL;
    size_t cert_mem_len = 0;
    size_t key_mem_len = 0;
    size_t ca_mem_len = 0; 


    
    

   
    config.ping_seconds = cfg->ping_seconds; 
    config.timeout_s = cfg->timeout_s;
    config.server_uri = cfg->srvr_uri;


    config.cn_call_back_arg = cfg->cn_call_back_arg;
    config.ca_file = cfg->ca_file;
    config.key_type = cfg->key_type; 
    config.key_file = cfg->key_file;
    config.private_key_type = cfg->private_key_type;
    config.cert_file = cfg->cert_file;   

    
    /***********************************server address setting**********************************************/
     if(coap_split_uri((uint8_t*)config.server_uri, strlen(config.server_uri), &uri) == -1){
        ESP_LOGE(TAG, "zcoap_client_create(), CoAP server uri error");
        return -1;
    }

    //ESP_LOGI(TAG, "zcoap_client_create(), coap_split_uri() success");
    //from coap_uri to coap_address

    coap_address_t server_address;
    
    zcoap_get_coapaddr(&uri, &server_address);

    if (!server_address.size)
    {
        ESP_LOGE(TAG, "zcoap_client_create(), zcoap_get_coapaddr() has failed");
        return -1;
    }
    
    
    /***********************************new context**********************************************/ 
    context= coap_new_context(NULL);
    if (!context) {
            ESP_LOGE(TAG, "zcoap_client_create(), coap_new_context() has failed");
            goto clean_up;
    }

    coap_context_set_keepalive(context, config.ping_seconds);

    /***********************************new session**********************************************/

    if(uri.scheme == COAP_URI_SCHEME_COAPS){
        if (!coap_dtls_is_supported()){
            ESP_LOGE(TAG,"scheme not supported");
            slot = -1; 
            goto clean_up;
        } 

        memset(client_sni, 0, sizeof(client_sni)); 
        memset(&dtls_pki, 0, sizeof(dtls_pki));

        dtls_pki.version= COAP_DTLS_PKI_SETUP_VERSION;

        dtls_pki.verify_peer_cert        = 0; //1
        dtls_pki.require_peer_cert       = 1;
        dtls_pki.allow_self_signed       = 1;
        dtls_pki.allow_expired_certs     = 1;

        dtls_pki.cert_chain_validation   = 0; //1
        dtls_pki.cert_chain_verify_depth = 1; //should be 3

        dtls_pki.check_cert_revocation   = 1;
        dtls_pki.allow_no_crl            = 1;
        dtls_pki.allow_expired_crl       = 1;

        dtls_pki.validate_cn_call_back = verify_cn_callback;
        dtls_pki.cn_call_back_arg = config.cn_call_back_arg;

        dtls_pki.validate_sni_call_back = NULL; 
        dtls_pki.sni_call_back_arg = NULL;
        

        dtls_pki.additional_tls_setup_call_back = NULL;
        
        if(uri.host.length){
            memcpy(client_sni, uri.host.s, MIN(uri.host.length, sizeof(client_sni)));
        } else{
            memcpy(client_sni, "localhost", 9); 
        }
        dtls_pki.client_sni = client_sni; 

        dtls_pki.pki_key.key_type = config.key_type;

        if(config.key_type == COAP_PKI_KEY_PEM){
            dtls_pki.pki_key.key.pem.ca_file = config.ca_file; 
            dtls_pki.pki_key.key.pem.public_cert = config.cert_file;
            dtls_pki.pki_key.key.pem.private_key = config.key_file?config.key_file:config.cert_file; 
        }else{
            if (ca_mem == 0 && cert_mem == 0 && key_mem == 0) {
                ca_mem = read_file_mem(config.ca_file, &ca_mem_len);
                cert_mem = read_file_mem(config.cert_file, &cert_mem_len);
                key_mem = read_file_mem(config.key_file, &key_mem_len);
            }
            
            dtls_pki.pki_key.key.asn1.ca_cert = ca_mem;
            dtls_pki.pki_key.key.asn1.public_cert = cert_mem;
            dtls_pki.pki_key.key.asn1.private_key = key_mem ? key_mem : cert_mem;
            dtls_pki.pki_key.key.asn1.ca_cert_len = ca_mem_len;
            dtls_pki.pki_key.key.asn1.public_cert_len = cert_mem_len;
            dtls_pki.pki_key.key.asn1.private_key_len = key_mem ?
                                                            key_mem_len : cert_mem_len; 
            dtls_pki.pki_key.key.asn1.private_key_type = config.private_key_type;     
        }
        
        coap_session = coap_new_client_session_pki(context, NULL, &server_address,
                                                  uri.scheme == COAP_URI_SCHEME_COAPS ? COAP_PROTO_DTLS : COAP_PROTO_TLS,
                                                  &dtls_pki);
    }else{
    
        coap_session = coap_new_client_session(context, NULL, &server_address, COAP_PROTO_UDP);
    }
                                                  
                                              
    if (!coap_session) {
        ESP_LOGE(TAG, "zcoap_client_create(), coap_new_client_session() has failed");
        goto clean_up;
    }
    

    coap_register_response_handler(context, response_handler);
    coap_register_nack_handler(context, nack_handler);

    coap_register_option(context, COAP_OPTION_BLOCK2);


    memset(&coap_clients[slot], 0, sizeof(zcoap_client_t));

    coap_clients[slot].timeout_s = config.timeout_s; 
    coap_clients[slot].timeout_ms = config.timeout_s*1000;
    coap_clients[slot].obs_s = 0;
    coap_clients[slot].obs_ms = 0; 
     
    coap_clients[slot].used = 1;
    coap_clients[slot].session = coap_session;
    coap_clients[slot].ctx = context;
    coap_clients[slot].dst_uri = uri;
    coap_clients[slot].request = NULL;
    coap_clients[slot].response = NULL; 

    coap_clients[slot].token = 0;
    coap_clients[slot].last_observe_token = 0; 
    coap_clients[slot].last_used_token=0; 
    
    coap_clients[slot].doing_observe = 0; 
    coap_clients[slot].obs_started = 0; 


    coap_clients[slot].ca_mem = ca_mem; 
    coap_clients[slot].cert_mem = cert_mem; 
    coap_clients[slot].key_mem = key_mem;  
      
    return slot;    

    clean_up:
        if (coap_session) {
            coap_session_release(coap_session);
        }
        if (context) {
            coap_free_context(context);
        }
        if(cert_mem)
            coap_free(cert_mem); 
        if(ca_mem)
            coap_free(ca_mem); 
        if(key_mem)
            coap_free(key_mem); 
        return -1;


}

/*send a request, after the call you should free response->data*/
int zcoap_client_request (zcoap_request_t *request, int zc, zcoap_response_t *response, 
                            int observe){
                                
    zcoap_client_t *zh = &coap_clients[zc];

    int result = 0; 
    

    coap_block_t block_wise_transfer; 

    if(!zh->used){
        ESP_LOGE(TAG, "zcoap_client_request(), selected client unused");
        return 0;
    }

    zh->request = request;
    zh->response = response; 

    zh->response->data = NULL; 
    
    
    
    /*****************************path setting*******************************/

    size_t size = 0;
    char* temp = zh->request->path;
    
    if(temp){
        while (*temp++){
            ++size;
        }
    }

    //ESP_LOGD(TAG, "path size = %i", size);
    zh->dst_uri.path.s =(uint8_t*) zh->request->path;
    zh->dst_uri.path.length = size;

    //ESP_LOGD(TAG, "query =%s ", zh->request->query);

    /*************************query setting******************************/
    size = 0;
    temp = zh->request->query;
    
    if(temp){
        while (*temp++){
            ++size;
        }
    }

    zh->dst_uri.query.s =(uint8_t*) zh->request->query;
    zh->dst_uri.query.length = size;



    block_wise_transfer.szx = zh->request->block_wise_transfer_size;
    block_wise_transfer.num = 0; 
    block_wise_transfer.m = 0; 

    coap_pdu_t* pdu_to_be_sent = zcoap_new_request(zc, observe, &block_wise_transfer);
    if(!pdu_to_be_sent){
        ESP_LOGE(TAG, "zcoap_new_request has failed"); 
        return 0; 
    }

    //printing the pdu for debugging purposes
    //coap_show_pdu(LOG_EMERG, pdu_to_be_sent);
   
    zh->resp_wait = 1; 
    if(coap_send(zh->session, pdu_to_be_sent) == COAP_INVALID_TID)
        ESP_LOGE(TAG, "zcoap_client_request(), coap_send() has failed");
    
    ESP_LOGD(TAG, "zcoap_client_request, coap_send() succeded");
    
    zh->timeout_ms = zh->timeout_s*1000; 

    /******************************************response wait************************************/
    while(zh->resp_wait){
        uint32_t wait_ms; 

        if(zh->obs_ms){
            wait_ms = (zh->timeout_ms > zh->obs_ms) ? zh->obs_ms : zh->timeout_ms;
        } else {
            wait_ms = zh->timeout_ms;
        }

        result = coap_run_once(zh->ctx, wait_ms);
        ESP_LOGD(TAG, "coap_run_once result = %i", result);

        
        if(result >= 0) {
            if (zh->timeout_ms > 0)
            {
            
               if (result >= zh->timeout_ms){
                   ESP_LOGI(TAG, "timeout ");
                   break;
               } else{
                   zh->timeout_ms -= result;

               }

            
            }
            if (zh->obs_ms > 0){
                if(result >= zh->obs_ms){
                    ESP_LOGI(TAG, "clear observation relationship\n");
                    zh->obs_ms = 0; 
                    zh->obs_s = 0;
                    zh->doing_observe = 0;
                    zcoap_client_request(zh->request, zc, zh->response, -1); /*clear the observarion relationship*/
                    zh->obs_started = 0; 
                    vosSemDestroy(zh->response->data_lock);
                    break; 
                }else{
                    zh->obs_ms -= result;

                }
            }

            
        }
        
       
    }
        
    return 1;
}


int zcoap_client_observe(int zc,int obs_timeout_s, char* topic, zcoap_response_t* response, 
                            size_t block_wise_transfer_size){
                                
    zcoap_client_t *zh = &coap_clients[zc];
    zcoap_request_t req;

    if(!zh->used){
        ESP_LOGE(TAG, "zcoap_client_observe(), selected client unused");
        return 0;
    }

    //setting the request code and type as indicated by the protocol 
    
    zh->request = &req; 
    zh->request->code = COAP_REQUEST_GET;
    zh->request->type = COAP_MESSAGE_CON;


    //setting the path of the wanted resource
    zh->request->path = topic;
    zh->request->query = NULL;
    zh->request->data = NULL; 

    zh->request->block_wise_transfer_size = block_wise_transfer_size; 

    response->data_lock = vosSemCreate(1); /*the corresponding destroy is done in zcoap_client_request when we clear the observation relationship*/
    response->readable = 0;
    //setting the observation relationship timeout
    zh->obs_s = obs_timeout_s;

    //sending the request
    if (!zcoap_client_request(zh->request, zc, response, 1))
    {
        ESP_LOGE(TAG, "zcoap_client_observe(), zcoap_client_request() has failed");
        return 0; 
    }
    
    return 1; 
}


int zcoap_client_publish(int zc, char* topic, size_t block_wise_transfer_size){
    zcoap_client_t *zh = &coap_clients[zc];

    if(!zh->used){
        ESP_LOGE(TAG, "zcoap_client_publish(), selected client unused");
        return 0;
    }
    zcoap_request_t req;
    zh->request = &req; 

    zh->request->code = COAP_REQUEST_PUT;
    zh->request->type = COAP_MESSAGE_CON;
    
    //sending the path of the wanted resource
    zh->request->path = topic;
    zh->request->query = NULL;

    zh->request->block_wise_transfer_size = block_wise_transfer_size; 

    //sending the request
    if (!zcoap_client_request(zh->request, zc, zh->response, 0))
    {
        ESP_LOGE(TAG, "zcoap_client_publish(), zcoap_client_request() has failed");
        return 0; 
    }

    return 1;

}


int zcoap_client_destroy(int zc){  

    zcoap_client_t *zh = &coap_clients[zc];

    if(!zh->used){
        ESP_LOGE(TAG, "zcoap_client_destroy(), selected client unused");
        return 0;
    }
    if(zh->ca_mem)
        coap_free(zh->ca_mem);
    if(zh->cert_mem)
        coap_free(zh->cert_mem);
    if(zh->key_mem)
        coap_free(zh->key_mem);

    coap_session_release(zh->session);
    coap_free_context(zh->ctx);


    /**/
    //forse andrebbe distrutta anche la optlist associata alla struttura request 
    zh->used = 0; 
    
    return 1;  
}
