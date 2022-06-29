#include "main.h"
#include "vosal/vosal.h"
#include "lang/lang.h"



static int mainthread(void *args){
    platform_configure();

    zdebugf("Platform init OK\n");
    int res = vm_init();
    if (res) {
        zerrorf("Can't Initialize OS from bytecode, reason %d\n", res);
        return res;
    }
    PThread *pymain = vm_start();
    vm_run(pymain);
    while(1){
        vosThSleep(TIME_U(30000, MILLIS));
    }
    return 0;
}

//Main for zerynth, called by platform entrypoint
int zerynth_main(void) {
    //initialize platform: can't use malloc, can't use printf/debugf, can't use threads
    platform_init();
    //initialize heap: after this we can call malloc, still no threads, no printf
    gc_init();
    //initialize vosal: after this threads are available
    vosInit(NULL);
    //for some obscure reasons, the esp32 main thread behaves badly with priorities
    //(i.e it gets the priority of wifi and event threads when interacting with them)
    //taking "inspiration" from the espressif examples that always create a thread for the jobs
    //and exit the main thread, we did the same and got it working...go figure...
    VThread vmain = vosThCreate(VM_DEFAULT_THREAD_SIZE, VOS_PRIO_NORMAL, mainthread, NULL, NULL);
    vosThResume(vmain);

    // platform_configure();

    // zdebugf("Platform init OK\n");
    // int res = vm_init();
    // if (res) {
    //     zerrorf("Can't Initialize OS from bytecode, reason %d\n", res);
    //     return res;
    // }
    // PThread *pymain = vm_start();
    // vm_run(pymain);
    // Get __init_system__ function
    // Call __init_system__ function
    // vosInit()
    // Get __init_device__ function
    // Call __init_device__ function
    // Run VM
    return 0;
}



// #include "vbl.h"
// #include "port.h"
// #include "lang.h"
// #include "md5.h"
// #ifdef ESP8266_RTOS
// #undef printf
// #endif

// int check_vm(void);
// void main_function(void *data);

// void _init_clock(void);


// //static VM
// VM vm;
// uint16_t woken_up=0;


// #ifdef VM_UPLOADED_BYTECODE
// uint8_t *codemem;// = codearea;
// #else
// //#include "../../../tests/testcode.h"
// uint8_t *codemem = NULL;//codearea;
// #endif

// #if defined(VM_OTA)
//     VMOTA ota_precord;
//     VMOTA *ota_record = &ota_precord;
// #endif

// int check_vm(){

// #if defined(VM_UNPROTECTED) && VM_UNPROTECTED==1
// return 0;
// #else
//     ztracem(VM_TRACING_STARTUP,"Performing Check");
//     MD5_CTX ctx;
//     uint8_t *uid;
//     uint8_t res[16];
//     uint8_t check[16];
//     //uint32_t magic = 0x44474747;

//     int uidlen = vhalNfoGetUIDLen()*2;
//     uid = vhalNfoGetUIDStr();

//     __memcpy(check,(VMSTORE_ADDR())+1024-16,16);

//     MD5_Init(&ctx);
//     //UID is in string form!!!!!!!!
//     MD5_Update(&ctx,uid,uidlen);
//     //MD5_Update(&ctx,((uint8_t*)&__flash_start__),((uint8_t*)&__vmstore__)-((uint8_t*)&__flash_start__)+1024-16);
//     //for(uidlen=0;uidlen<4;uidlen++) MD5_Update(&ctx,&magic,4); //add 16 bytes of magic number
//     MD5_Final(res,&ctx);
//     return __memcmp(res,check,16);
// #endif
// }


// #if defined(VM_KICK_IWDG) && VM_KICK_IWDG!=0
// extern void iwdg_reset(void *data);
// void iwdg_debug(void *data){
//     iwdg_reset(data);
// }
// #endif

// #if defined(ESP8266_RTOS)
// void user_init(void){
// #elif defined(ESP32_RTOS)
// #undef printf
// void app_main(void){
// #elif defined(SPRESENSE_RTOS)
// void zerynth_main(void) {
// #else
// // #   if defined(RTOS_chibi2)
// // #   include "hal.h"
// // #   endif
// //#include "esp_system.h"


// void BUTTON1_pressed(void)
// {
//     //ioport_pin_t btnNumber = IOPORT_CUSTOM_CREATE_PIN2(PIN_PORT_NUMBER(BTN0), PIN_PAD(BTN0));
//     vhalPinToggle(LED0);
// }

// void BUTTON2_pressed(void)
// {
//     //ioport_pin_t btnNumber = IOPORT_CUSTOM_CREATE_PIN2(PIN_PORT_NUMBER(BTN0), PIN_PAD(BTN0));
//     vhalPinToggle(LED0);
// }


// int main(void) {
// #endif


// #if defined(VM_OTA)
//     // ztracem(VM_TRACING_STARTUP,"Start OTA check");
//     ota_record = ota_record_read(&ota_precord);
//     //check ota record
//     if(is_ota_record_valid(ota_record) && VM_OTA_MAX_VM>1){
//         //printf("wm %i, cm %i, vi %i\n",ota_record->working_vm,ota_record->current_vm,VM_OTA_INDEX);
//         if(ota_record->working_vm != ota_record->current_vm) {
//             //an ota has been performed, jump to new vm
//             //change the ota_record before
//             //it will be the duty of the new vm to write the new ota record
//             uint32_t current_vm = ota_record->current_vm;
//             uint32_t magic = ota_record->magic;
//             ota_record->current_vm = ota_record->working_vm;
//             ota_record_write(ota_record);
//             ota_record->current_vm = current_vm;
//             ota_record->magic = magic;
//             if(ota_record->current_vm!=VM_OTA_INDEX) {
//                 //this is not the working vm, jump to the correct one
//                 ota_jump_to_vm(VM_OTA_VM_ADDR(ota_record->current_vm));
//                 //should never return
//             }

//         }
//         else {
//             //working vm is the same as current vm
//             //check if jump is needed
//             if (ota_record->working_vm !=VM_OTA_INDEX && VM_OTA_INDEX==0) {
//                 ota_jump_to_vm(VM_OTA_VM_ADDR(ota_record->working_vm));
//             }
//             //should never return
//         }
//     }
// #endif


//     //Initialize RTOS
//     // ztracem(VM_TRACING_STARTUP,"Init VOS");
//     vosInit(NULL);

//     //Pure FreeRTOS never reaches this point, vTaskStartScheduler then calls main_function
//     main_function(NULL);

// #   if !defined(ESP8266_RTOS) && !defined(SPRESENSE_RTOS)
//     return 0;
// #   endif


// }

// #if !defined(VM_SECUREFW) && defined(ESP32_RTOS)
// void vmDisableWatchdog(void);
// #endif

// void main_function(void *data){
//     (void)data;

//     vm_tracing_init();
//     ztracem(VM_TRACING_STARTUP,"Main");


// #if !defined(VM_SECUREFW) && defined(ESP32_RTOS)
//     //disable RTC WDT that is set by the bootloader
//     ztracem(VM_TRACING_STARTUP,"Disable ESP32 Watchdog");
//     vmDisableWatchdog();
// #endif

// #if defined(VM_IS_CUSTOM)
//     //load up the custom vm scheme
//     ztracem(VM_TRACING_STARTUP,"Init Custom VM");
//     init_custom_vm();
// #endif

//     //Initialize VHAL
//     ztracem(VM_TRACING_STARTUP,"Init VHAL");
//     vhalInit(NULL);

//  #if 0
//     uint32_t drv = VM_UPLOAD_INTERFACE;
//     //vhalPinSetMode(LED0,PINMODE_OUTPUT_PUSHPULL);
//     vhalPinWrite(LED0,1);
//     vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,0,0), VM_RXPIN(drv), VM_TXPIN(drv));
//     vhalSerialWrite(drv, "!\n", 2);
//     uint32_t st_addr = 0x22000;
//     uint32_t end_addr = 0xA400;
//     uint32_t b;
//     uint32_t bb[2]={0xDEADBEEF,0xDEADBEEF};
//     uint8_t err_check=0;
//     vhalFlashTest();
//     err_check = vhalFlashErase(st_addr,8192);
//     vhalSerialWrite(drv, "CHECK#\n", 7);
//     for(b=0;b<8192;b+=8){
//         if (*((uint32_t*)(st_addr+b))!=0xffffffff){
//             vhalSerialWrite(drv, "#", 1);
//         }
//     }
//     vhalSerialWrite(drv, "WRITE#\n", 7);
//     for(b=0;b<8192;b+=8){
//         int rd = vhalFlashWrite( (void*)(st_addr+b),&bb,8);
//         //vhalSerialWrite(drv, "1\n", 2);
//     }

//     vhalSerialWrite(drv, "CHECKX\n", 7);
//     for(b=0;b<8192;b+=4){
//         if (*((uint32_t*)(st_addr+b))!=0xDEADBEEF){
//             vhalSerialWrite(drv, "X", 1);
//         }
//     }

//     while(1){
//         vosThSleep(TIME_U(1000, MILLIS));
//         vhalSerialWrite(drv, "!\n", 2);
//         vhalPinToggle(LED0);
//     }
// #endif


// #if defined(VM_POWERSAVING)
//     ztracem(VM_TRACING_STARTUP,"Init Powersave");
//     woken_up = vhalInitPowersave();
//     ztracemv(VM_TRACING_RESET,"RESET Cause",woken_up);
// #endif

// #if defined(VM_SECUREFW)
//     ztracem(VM_TRACING_STARTUP,"Init SFW");
//     vhalInitSecureFW();
// #endif

//     vosThSetData(vosThCurrent(), NULL);

// #if defined(VM_UPLOADED_BYTECODE)
//     //Initialize codemem if in bytecode upload mode
//     #if defined(VM_OTA)
//         ztracem(VM_TRACING_STARTUP,"Init VBL (OTA)");
//         codemem = vbl_init(ota_record);
//         ota_record->bcaddr = VM_OTA_UNMAP_ADDRESS(((uint32_t)codemem));
//         ota_record->vmaddr = VM_OTA_VM_ADDR(VM_OTA_INDEX);
//         ztracemv(VM_TRACING_STARTUP,"Init VBL (OTA) codemem",codemem);
//     #else
//         ztracem(VM_TRACING_STARTUP,"Init VBL");
//         codemem = vbl_init(NULL);
//         ztracemv(VM_TRACING_STARTUP,"Init VBL codemem",codemem);
//     #endif

// #endif


// #if defined(VM_KICK_IWDG) && VM_KICK_IWDG!=0
//     VSysTimer iwdgtm;
//     iwdgtm = vosTimerCreate();
//     vosSysLock();
//     vosTimerRecurrent(iwdgtm,TIME_U(1000,MILLIS),iwdg_debug,NULL);
//     vosSysUnlock();
// #endif


// #if defined(VM_REGISTER) && VM_REGISTER==1 //REGISTER VM
//     uint32_t drv = VM_UPLOAD_INTERFACE;
//     uint8_t *uidstr = vhalNfoGetUIDStr();
//     vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,VM_HWFC(drv),0), VM_RXPIN(drv), VM_TXPIN(drv));
//     // vhalSerialWrite(drv, (uint8_t *)cvminfo->target, cvminfo->target_size);
//     PORT_LED_CONFIG();
//     while (1) {
//         vosThSleep(TIME_U(50, MILLIS));
//         PORT_LED_ON();
//         vosThSleep(TIME_U(50, MILLIS));
//         PORT_LED_OFF();
//         vhalSerialWrite(drv, uidstr, vhalNfoGetUIDLen() * 2);
//         vhalSerialWrite(drv, (uint8_t *)"\n", 1);
//     }


// #else //NORMAL VM

// ztracem(VM_TRACING_STARTUP,"Check VM");
// if (check_vm()){
//     uint32_t drv = VM_UPLOAD_INTERFACE;
//     //serial port is already opened by vm_upload
//     // vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,0,0), VM_RXPIN(drv), VM_TXPIN(drv));
//     while(1){
//         vosThSleep(TIME_U(500, MILLIS));
//         vhalSerialWrite(drv, (uint8_t*)"Tampered bytecode!!\n", 20);
//     }
// }

// #if defined(HAS_PRE_VM_HOOK)
//     ztracem(VM_TRACING_STARTUP,"Call pre-vm hook");
//     pre_vm_hook(PRE_VM_HOOK_ARGS);
// #endif

// #ifdef VM_UPLOADED_BYTECODE

//     ztracem(VM_TRACING_STARTUP,"Create VM");
//     memset(&vm, 0, sizeof(VM));
//     #ifdef BYTECODE_ACCESS_ALIGNED_4
//     //allocate space for _vm.program
//     //it must be done here, because vm_upload() expects vm_init
//     //to allocate only memory for .data and .bss of native C
//     //printf("before alloc ch %x\n",vm.program);
//     ztracem(VM_TRACING_STARTUP,"Program alloc");
//     vm.program = gc_malloc(sizeof(CodeHeader));
//     //printf("after alloc ch %x\n",vm.program);
//     #endif
//     //Let's check for incoming bytecode if not woken up.
//     ztracem(VM_TRACING_STARTUP,"Call VM upload");
//     vm_upload(&vm);
// #endif

// #if defined(HAS_PRE_VM_NOALLOC_HOOK)
//     pre_vm_noalloc_hook();
// #endif

//     ztracem(VM_TRACING_STARTUP,"Call VM Init");
//     if (vm_init(&vm)) {
//         ztracem(VM_TRACING_STARTUP,"Call VM run");
//         vm_run(_vm.thlist);
//     } else {
//         ztracem(VM_TRACING_STARTUP,"VM Init failed");
//     }

//     ztracem(VM_TRACING_STARTUP,"Main thread exited");

//     vosThSetPriority(VOS_PRIO_IDLE);
//     while(1){
//         vosThSleep(TIME_U(1000,MILLIS));
//         //asm volatile ("wfi" : : : "memory");
//     }

// #endif //VM_REGISTER

// }

//     /*
//     uint32_t drv = VM_UPLOAD_INTERFACE;
//     vhalPinSetMode(LED0,PINMODE_OUTPUT_PUSHPULL);
//     vhalPinWrite(LED0,1);
//     vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,0,0), VM_RXPIN(drv), VM_TXPIN(drv));
//     vhalSerialWrite(drv, "!\n", 2);
//     uint32_t st_addr = 0x30000;
//     uint32_t end_addr = 0x40000;
//     uint32_t b;
//     uint32_t bb[2]={0xDEADBEEF,0xDEADBEEF};
//     vhalFlashErase(st_addr,8192);

//     vhalSerialWrite(drv, "CHECK#\n", 7);
//     for(b=0;b<8192;b+=8){
//         if (*((uint32_t*)(st_addr+b))!=0xffffffff){
//             vhalSerialWrite(drv, "#", 1);
//         }
//     }
//     vhalSerialWrite(drv, "WRITE#\n", 7);
//     for(b=0;b<8192;b+=8){
//         int rd = vhalFlashWrite( (void*)(st_addr+b),&bb,8);
//         uint8_t bf[2];
//         bf[0]='0'+rd;
//         bf[1]='\n';
//         vhalSerialWrite(drv, bf, 2);
//     }
//     vhalSerialWrite(drv, "CHECKX\n", 7);
//     for(b=0;b<8192;b+=4){
//         if (*((uint32_t*)(st_addr+b))!=0xDEADBEEF){
//             vhalSerialWrite(drv, "X", 1);
//         }
//     }
//     while(1){
//         vosThSleep(TIME_U(1000, MILLIS));
//         vhalSerialWrite(drv, "!\n", 2);
//         vhalPinToggle(LED0);
//     }
//     */
// /*
// PORT_LED_CONFIG();
//     while(1){
//         vosThSleep(TIME_U(1000, MILLIS));
//         vhalPinToggle(LED0);
//     }
// */
// /*
// PORT_LED_CONFIG();
// uint32_t drv = VM_UPLOAD_INTERFACE;
//     vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,0,0), VM_RXPIN(drv), VM_TXPIN(drv));
//     void *prt = vhalPinGetPort(LED0);
//     int pad = vhalPinGetPad(LED0);
//     int st= 0;
//     while(1){
//         st = !st;
//         if (st) vhalPinFastSet(prt,pad);
//         else vhalPinFastClear(prt,pad);
//         //vhalPinToggle(LED0);
//         vhalSerialWrite(drv, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", 73);
//         vosThSleep(TIME_U(100, MILLIS));

//     }


//         uint32_t drv = VM_UPLOAD_INTERFACE;
//     uint8_t ch='!';
//     vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,0,0), VM_RXPIN(drv), VM_TXPIN(drv));
//     vhalPinSetMode(LED0,PINMODE_OUTPUT_PUSHPULL);
//     while(1){
//         vhalSerialWrite(drv, &ch, 1);
//         vhalSerialRead(drv,&ch,1);
//         vosThSleep(TIME_U(100, MILLIS));
//         vhalPinToggle(LED0);
//     }

// */


//     // // /* TEST EXT */
//     // vhalPinSetMode(LED0,PINMODE_OUTPUT_PUSHPULL);
//     // vhalPinAttachInterrupt(BTN0,PINMODE_EXT_FALLING,bbb,TIME_U(1000,MILLIS));

//     // while (1){
//     //     //vhalPinToggle(LED0);
//     //     //vhalGoToSleep(TIME_U(5000,MILLIS),POWERSAVE_STOP);
//     // }


//     // /* TEST PWR */
//     // vhalPinSetMode(LED0,PINMODE_OUTPUT_PUSHPULL);
//     // int ii;
//     // for(ii=0;ii<10*(woken_up==2 ? 5:1);ii++){
//     //     vhalPinToggle(LED0);
//     //     vosThSleep(TIME_U((woken_up) ? 100:200,MILLIS));
//     // }
//     // while (1){
//     //     vhalPinToggle(LED0);
//     //     vhalGoToSleep(TIME_U(5000,MILLIS),POWERSAVE_STOP);
//     // }


//     // /* TEST SERIAL */
//     // uint32_t drv = VM_UPLOAD_INTERFACE;
//     // uint8_t ch='!';
//     // vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,0,0), VM_RXPIN(drv), VM_TXPIN(drv));
//     // vhalPinSetMode(LED0,PINMODE_OUTPUT_PUSHPULL);
//     // while(1){
//     //     vhalSerialWrite(drv, &ch, 1);
//     //     vhalSerialRead(drv,&ch,1);
//     //     vosThSleep(TIME_U(100, MILLIS));
//     //     vhalPinToggle(LED0);
//     // }
//     //

//     //     uint32_t drv = VM_UPLOAD_INTERFACE;
//     // uint8_t ch='!';
//     // vhalSerialInit(drv, VM_SERIAL_BAUD, SERIAL_CFG(SERIAL_PARITY_NONE,SERIAL_STOP_ONE, SERIAL_BITS_8,0,0), VM_RXPIN(drv), VM_TXPIN(drv));
//     // while(1){
//     //     vhalSerialWrite(drv, &ch, 1);
//     //     vhalSerialRead(drv,&ch,1);
//     //     vosThSleep(TIME_U(100, MILLIS));
//     //     printf("available %i\n",vhalSerialAvailable(drv));
//     // }
