#include <math.h>
#include <string.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "SPIbus.hpp"
#include "../include/W25N01GV.h"
#include "sdkconfig.h"

constexpr int MOSI = 13;
constexpr int MISO = 12;
constexpr int SCLK = 14;
constexpr int CS = 15;

constexpr uint32_t CLOCK_SPEED = 1200000; // up to 1MHz for all registers

struct winbond{
	winbond() : nota(0){

	}
	int nota;
};	


void vspi_start(void){

	spi_device_handle_t vspi_handle; //Handle(identificador) for a device on a SPI bus "spi_master.h"

    esp_err_t err = vspi.begin(MOSI, MISO, SCLK,0);
    printf("Error number 1: %s\n",esp_err_to_name(err)); //cppcheck-suppress misra-c2012-17.7
    esp_err_t err2 = vspi.addDevice(0, CLOCK_SPEED, CS, &vspi_handle);
    printf("Error number 2: %s\n",esp_err_to_name(err2));//cppcheck-suppress misra-c2012-17.7

}

winbond_t *init_sensor(void){
	winbond_t *sensor = new winbond_t();
	return sensor;
}

int getSensorNota(winbond_t *sensor){
	return sensor->nota;
}