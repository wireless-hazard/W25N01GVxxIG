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

RTC_DATA_ATTR static bool write;  

extern "C" void app_main(){

	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

	if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER){

		//MEMORY STARTING ROUTINE - START%%%%%%%%%%%%%%%%%%%
    	winbond_t *flash_memory = init_w25_struct(sizeof(float)*N_SAMPLES+4);
    
    	ESP_ERROR_CHECK(vspi_w25_alloc_bus(flash_memory));
    
    	w25_Initialize(flash_memory);
    	//MEMORY STARTING ROUTINE - END%%%%%%%%%%%%%%%%%%%%

		if (write){

			ESP_LOGE(TAG, "WRITE!");

			float payload[N_SAMPLES] = {1.4,4.7,6.0,26.6597,17.12,97,0.23,7.8,12.85,0}; //Array that will be written on the memory
			uint8_t *payloadUINT8_T = (uint8_t *)&payload[0]; //Since the memory accepts uint8_t converts it without changing the values
			//if the payload was a uint8_t, the conversion wouldn't be necessary

    		ESP_ERROR_CHECK(w25_BlockErase(flash_memory, 0x0000)); //Memory needs to be erased before writing in the address in a new block
    		ESP_ERROR_CHECK(w25_WriteMemory(flash_memory, 0x0000, 0x0000, payloadUINT8_T, sizeof(float)*N_SAMPLES));

			write = false;

		}else{
			
			ESP_LOGE(TAG, "READ!");

			uint8_t payloadUINT8_T[sizeof(float)*N_SAMPLES]{0};

			w25_ReadMemory(flash_memory, 0x0000, 0x0000, &payloadUINT8_T[0], sizeof(float)*N_SAMPLES);

			float *payload = (float *)&payloadUINT8_T[0];

			for (int i = 0; i < N_SAMPLES; i++)
			{		
				ESP_LOGW(TAG,"%f\n", payload[i]);
			}

			write = true;
		}

	}else{
		write = true; //Initializes the variable the very first time the esp is powered on
	}

	esp_deep_sleep(sleep_seconds*1000000); //Sleeps for 2 seconds

}