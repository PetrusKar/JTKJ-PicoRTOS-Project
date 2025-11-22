#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"


//määritellään napeille bufferit("säilytyspaikka"), jonne merkit ns. "puskuroidaan" ennen tulostamista. PK
static volatile char morse_buffer[64];
static volatile uint8_t morse_index = 0;

int position = 0;// määritellään globaali muuttuja asennolle: 0=ei määritetty, 1=pöydällä, 2=pystyssä PK

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX      1

 
//tilakone
enum state { WAITING=1};
enum state programState = WAITING;


//funktio napille 1 PK
static void btn_fxn(uint gpio, uint32_t eventMask) {
     //napinn painallus lisää pisteen morse_bufferiin
    if (morse_index < sizeof(morse_buffer) - 1) {
        morse_buffer[morse_index++] = '.';

    }
}

// Funktio napille 2 PK
static void btn_fxn2(uint gpio, uint32_t eventMask) {

    //napin painallus lisää viivan morse_bufferiin
    if (morse_index < sizeof(morse_buffer) - 1) {
        morse_buffer[morse_index++] = '-';

       
    }     
}
 //funktio napille 1, kun pystyssä PK
static void btn_fxn3(uint gpio, uint32_t eventMask) {

    //napin painallus lisää tyhjän(space) morse_bufferiin
    if (morse_index < sizeof(morse_buffer) - 1) {
        morse_buffer[morse_index++] = ' ';
    }
}
//funktio napille 2, kun pystyssä PK 
static void btn_fxn4(uint gpio, uint32_t eventMask) {

    //napin painallus lisää lopetusmerkin(/) morse_bufferiin
    if (morse_index < sizeof(morse_buffer) - 1) {
        morse_buffer[morse_index++] = '/';
    }
}

//funktio kummallekin napille, kun asento ei määritetty PK
static void btn_fxn5(uint gpio, uint32_t eventMask) {

    //tässä tapauksessa lisätään kysymysmerkki, koska on väärä asento.
    if (morse_index < sizeof(morse_buffer) - 1) {
        morse_buffer[morse_index++] = '?';
    }
}
// Sisältää käsittelijän kummallekin napille----> kutsuu tarvittavaa funktiota. PK
static void btn_handler(uint gpio, uint32_t eventMask) {
     

    // tarkistaa kumpaa nappia painetttiin sekä laitteen asennon ja kutsuu sitä vastaavaa funktiota.
    if (gpio == BUTTON1 && position == 1) {
        return btn_fxn(gpio, eventMask);
    }
    else if (gpio == BUTTON2 && position == 1) {
        return btn_fxn2(gpio, eventMask);
    }
    else if (gpio == BUTTON1 && position == 2) {
        return btn_fxn3(gpio, eventMask);
    }
    else if (gpio == BUTTON2 && position == 2) {
        return btn_fxn4(gpio, eventMask);
    }
    else if (gpio == BUTTON1 || BUTTON2 && position == 3){
        return btn_fxn5(gpio, eventMask);
    }
}


static void sensor_task(void *arg){
    (void)arg;
     
    //muuutujat anturin lukemista varten SP
    float ax, ay, az;
    float gx, gy, gz;

    // alustaa anturin SP
    init_ICM42670();

    //asettaa anturin oletusarvoihin SP
    ICM42670_start_with_default_values();        
            
    for(;;){

        if (programState == WAITING) {
            ICM42670_read_sensor_data(&ax, &ay, &az, NULL, NULL, NULL, NULL);
            if(az > 0.8f){ //ax > -0.02f && ay > -0.1f && az < 1.1f
                position = 1; //pöydällä
            }
            else if(ay < -0.8f){ //ax > 0.08f && ay > -1.0f && az > 0.03f
                position = 2;//pystyssä
            }
            else if(az < -0.8f || ax > 0.8f){
                position = 3;//ei määritelty asento
            }
        }    
        tight_loop_contents(); 

        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//tämä funktio tarkistaa onko morse-bufferissa jotain ja tulostaa sen. Sekä nollaa sen, jotta se ei täyty jatkuvasti.
//sitten odottaa sekunnin, jotta vältytään tuplakirjoitukselta. SP/PK
static void print_task(void *arg){
    (void)arg;
    
    while(1){

        if(morse_index > 0) {
            for (int i = 0; i < morse_index; i++) {
                printf("%c", morse_buffer[0]);
            }//tämä print-on sen takia, että saadaan uusi rivi, muuten koko bufferi tulostuu yhteen riviin.
            printf("\n");

            sleep_ms(500);
            
            morse_index = 0;
        }

        
    }
      
        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(1000));
    
}


int main() {

    stdio_init_all();

    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    init_button1();
        // Initialize the button pin as an input with a pull-up resisto
     
    init_button2();
        // Initialize the button pin as an input with a pull-up resistor    
     
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_handler);
        // keskeytyskäsittelijä napille 1 PK

    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_FALL, true);
        // keskeytyskäsittelijä napille 2  PK  
 
    
    TaskHandle_t hSensorTask, hPrintTask, hUSB = NULL;

    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensor_task, // (en) Task function
                "sensor",                        // (en) Name of the task 
                DEFAULT_STACK_SIZE,              // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                            // (en) Arguments of the task 
                2,                               // (en) Priority of this task
                &hSensorTask);                   // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(print_task,  // (en) Task function
                "print",              // (en) Name of the task 
                DEFAULT_STACK_SIZE,   // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                NULL,                 // (en) Arguments of the task 
                2,                    // (en) Priority of this task
                &hPrintTask);         // (en) A handle to control the execution of this task

    if(result != pdPASS) {
        printf("Print Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();
    
    // Never reach this line.
    return 0;
}

