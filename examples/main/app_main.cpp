#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "W25N01GV.h"

constexpr static uint8_t sleep_seconds = 2;

static const char *TAG = "w25_memory";

RTC_DATA_ATTR static bool write;  

extern "C" void app_main(){

	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

	if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER){
		
		if (write){

			ESP_LOGI(TAG, "WRITE!");
			write = false;

		}else{

			ESP_LOGI(TAG, "READ!");
			write = true;
		}

	}else{
		write = true; //Initializes the variable the very first time the esp is powered on
	}

	esp_deep_sleep(sleep_seconds*1000000); //Sleeps for 2 seconds

}