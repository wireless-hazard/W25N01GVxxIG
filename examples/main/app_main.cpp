#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "W25N01GV.h"

#define N_SAMPLES 10// Amount of real input samples

constexpr static uint8_t sleep_seconds = 2;

static const char *TAG = "w25_memory";

RTC_DATA_ATTR static bool write; //This variable persists the deep_sleep

extern "C" void app_main(){

	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

	if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER){

		//MEMORY STARTING ROUTINE - START%%%%%%%%%%%%%%%%%%%
    	winbond_t *flash_memory = init_w25_struct(sizeof(float)*N_SAMPLES+4); //This +4 is just a workaround for a current bug
    
    	ESP_ERROR_CHECK(vspi_w25_alloc_bus(flash_memory)); //Allocates the vspi bus
    
    	w25_Initialize(flash_memory); //Initializes the handler
    	//MEMORY STARTING ROUTINE - END%%%%%%%%%%%%%%%%%%%%

		if (write){ //Routine that writes the values into the memory

			ESP_LOGE(TAG, "WRITE!");

			float payload[N_SAMPLES] = {1.4,4.7,6.0,26.6597,17.12,97,0.23,7.8,12.85,0}; //Array that will be written into the memory
			uint8_t *payloadUINT8_T = (uint8_t *)&payload[0]; //Since the memory accepts uint8_t converts it without changing the values
			//if the payload was a uint8_t, the conversion wouldn't be necessary

    		ESP_ERROR_CHECK(w25_BlockErase(flash_memory, 0x0000, 20U)); //The memory's block needs to be erased before writing in the address
    		ESP_ERROR_CHECK(w25_WriteMemory(flash_memory, 0x0000, 0x0000, payloadUINT8_T, sizeof(float)*N_SAMPLES)); 
    		//Since the float has 4x the size of an uint8_t, the samples' number has to be multiplied by sizeof(float)

			write = false; //when the esp32 wakes from the deep sleep, it'll enter the Read path

		}else{
			
			ESP_LOGE(TAG, "READ!");

    		//Since the float has 4x the size of an uint8_t, the samples' number has to be multiplied by sizeof(float)
			uint8_t payloadUINT8_T[sizeof(float)*N_SAMPLES]{0}; 

			w25_ReadMemory(flash_memory, 0x0000, 0x0000, &payloadUINT8_T[0], sizeof(float)*N_SAMPLES);

			float *payload = (float *)&payloadUINT8_T[0]; //Converts back to float without changing the values

			for (int i = 0; i < N_SAMPLES; i++) //Now, payload has N_SAMPLES values again
			{		
				ESP_LOGW(TAG,"%f\n", payload[i]);
			}

			write = true; //when the esp32 wakes from the deep sleep, it'll enter the Write path
		}
		//DEINIT MEMORY - START%%%%%%%%%%%%%%%%%%%%%%%%%%%
    	ESP_LOGW("tag","FREE BUS: %s",esp_err_to_name(vspi_w25_free_bus(flash_memory)));
    	ESP_LOGW("tag","FREE STRUCT: %s",esp_err_to_name(deinit_w25_struct(flash_memory)));
    	//DEINIT MEMORY - END%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	}else{
		write = true; //Initializes the variable the very first time the esp32 is powered on
	}

	esp_deep_sleep(sleep_seconds*1000000); //Sleeps for 2 seconds

}