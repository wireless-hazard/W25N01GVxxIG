#include "unity.h"
#include "esp_system.h"

void app_main(void){

	UNITY_BEGIN();
	unity_run_all_tests();
	UNITY_END();
	// unity_run_menu();
}
