


/*
Script used to test the client:
    basically it creates a client that creates a resource and gets its content
    with the possibility of establishing a observation relationship.
    Finally the client is destroyed
*/


#include "main.h"
#include "vosal/vosal.h"
#include "lang/heap.h"
#include "vhal/vhal.h"
#include "platform/net.h"
#include "platform/zcoap.h"
#include "libcoap.h"
#include "coap_dtls.h"



#include "esp_log.h"


#define HOME 0
#define EXAMPLE_COAP_LOG_DEFAULT_LEVEL 7

/*1 to establish a observation relationship, 0 otherwise*/
#define OBSERVE_TEST 0


static zcoap_request_t req;
static zcoap_response_t res;

const static char *TAG = "CoAP_client";


void my_fun_ip(znet_interface_info_t * info) {
    uint8_t * ip = info->ip.ip_arr;

    zprintf("ip int = %i\n", info->ip.ip);
    zprintf("ip = %i.%i.%i.%i\n", ip[0], ip[1], ip[2], ip[3]);
}
void my_fun_con(znet_interface_info_t * info) {
    zprintf("connected.\n");
}

void my_fun_disc(znet_interface_info_t * info) {
    zprintf("disconnected.\n");
}

void define_payload(char** payload){
    char* string = "Gallia est omnis divisa in partes tres, quarum unam incolunt Belgae,\
aliam Aquitani, tertiam qui ipsorum lingua Celtae, nostra Galli\
appellantur. Hi omnes lingua, institutis, legibus inter se differunt.";
    size_t size = 0;
    char* temp = string;
    if(temp){
        while (*temp++){
            ++size;
        }
    } 
    *payload = malloc(size+1);
    memcpy(*payload, string, size+1);
    
    return; 
}

int zerynth_main(void){
    //initialize platform
    platform_init();
    //initialize heap
    gc_init();
    vosInit(NULL);
    platform_configure();
    
    zwdt_disable();
        
    coap_set_log_level(EXAMPLE_COAP_LOG_DEFAULT_LEVEL);


    int ret = 0;

    znet_interface_config_t wifi_conf = {
        .type = WIFI_STA,
    };

#if !HOME  
    znet_wifi_context_t ctx = {
        .ssid = "ZerynthTest",
        .pwd = "ZerynthTT",
    };
#elif HOME == 1
    znet_wifi_context_t ctx = {
        .ssid = "Home&Life SuperWiFi-2BC1",
        .pwd = "8QTEN3X73QR438FM",
    };
#else 
    znet_wifi_context_t ctx = {
        .ssid = "piano_sup",
        .pwd = "bangbang",
    };
#endif

    int ifc_wifi = znet_interface_register(&wifi_conf);
    zprintf("iface %i\n", ifc_wifi);
    znet_set_got_ip_cb(ifc_wifi, my_fun_ip);
    znet_set_connected_cb(ifc_wifi, my_fun_con);
    znet_set_disconnected_cb(ifc_wifi, my_fun_disc);
    znet_interface_set_dhcp(ifc_wifi, 1);
    vosThSleep(TIME_U(1, SECONDS));
    
    ret = znet_interface_up(ifc_wifi);
    zprintf("up wifi : %i\n", ret);
    vosThSleep(TIME_U(1, SECONDS));
    ret = znet_interface_connect(ifc_wifi, &ctx);
    zprintf("con wifi : %i\n", ret);

    /*content of the resource*/
    char* payload = NULL;
    define_payload(&payload);
    
    /*number of iteration of the sequence create resource -> get resource -> delete resource*/
    int i = 10;

    /*observe timeout*/
    int obs_timeout_s;

    /*client setting*/
    zcoap_client_cfg_create_t config; 
    config.srvr_uri = "coap://192.168.1.130/"; //server uri, TO BE CHANGED
    config.timeout_s = 10; //random
    config.ping_seconds = 0;
    config.cn_call_back_arg=NULL;
    
    int slot = zcoap_client_create(&config);
    if(slot>= 0)
        ESP_LOGI(TAG, "client created, slot=%i\n", slot);
    else{
        ESP_LOGE(TAG, "error zcoap_client_create()\n");
        goto end; 
    }

    
    while(i--){
    /*destroy*/
    req.code = COAP_REQUEST_DELETE;
    req.data = NULL; 
    req.path = ".ztest_coap";
    req.type = COAP_MESSAGE_NON;
    req.query = NULL;
    req.block_wise_transfer_size = 0;

    if(zcoap_client_request( &req ,slot, &res, 0)){
        ESP_LOGI(TAG, "zcoap_client_request() success\n");
        //printf("Received: %.*s\n", (int)res.data_len, res.data);
    }
    else {
        ESP_LOGE(TAG, "zcoap_client_request() has failed\n");
        goto end;
    }
     
    
    /*create*/
    req.code = COAP_REQUEST_PUT;
    req.data = payload;
    req.path =".ztest_coap";
    req.type = COAP_MESSAGE_NON;
    req.query = NULL;
    req.block_wise_transfer_size = 0;

    if(zcoap_client_request( &req ,slot, &res, 0)){
        ESP_LOGI(TAG, "zcoap_client_request() success\n");
        //printf("Received: %.*s\n", (int)res.data_len, res.data);
    }
    else {
        ESP_LOGE(TAG, "zcoap_client_request() has failed\n");
        goto end;
    }

    /*get*/
    req.code = COAP_REQUEST_GET;
    req.data = NULL;
    req.path = ".ztest_coap";
    req.block_wise_transfer_size = 0;
    
    #if !OBSERVE_TEST

    if(zcoap_client_request( &req ,slot, &res, 0)){
        ESP_LOGI(TAG, "zcoap_client_request() success\n");
        printf("Received: %.*s\n", (int)res.data_len, res.data);
    }
    else {
        ESP_LOGE(TAG, "zcoap_client_request() has failed\n");
        goto end;
    }
    
    free(res.data);
    res.data_len=0; 
     

    #else OBSERVE_TEST
        obs_timeout_s = 7;

        if(zcoap_client_observe(slot, obs_timeout_s, req.path, &res, req.block_wise_transfer_size)){
            ESP_LOGI(TAG, "zcoap_client_observe() success\n");
            printf("Received: %.*s\n", (int)res.data_len, res.data);
        }
        else 
            ESP_LOGE(TAG, "zcoap_client_observe() has failed\n");
        ESP_LOGD(TAG, "readable =%u", res.readable);
        free(res.data);
        res.data_len=0;


        req.code = COAP_REQUEST_PUT;
        req.data = "block_wise_transfer_test_block2_option_0123456789";
        req.path =".ztest_coap";
        

        req.type = COAP_MESSAGE_NON;
        req.query = NULL;
        
        req.block_wise_transfer_size = 0;

        if(zcoap_client_request( &req ,slot, &res, 0)){
            ESP_LOGI(TAG, "zcoap_client_request() success\n");
        }
        else {
            ESP_LOGE(TAG, "zcoap_client_request() has failed\n");
            goto end;
        }
    #endif
    } //while(i--)


    zcoap_client_destroy(slot);

    ESP_LOGD(TAG, "end"); 
    
    end:
    free(payload); 
    while (1){
            vosThSleep(TIME_U(1, SECONDS));
    }
}

