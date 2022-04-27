#include <math.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "../include/W25N01GV.h"
#include "sdkconfig.h"
#include "hal/gpio_types.h"
#include <stdbool.h>
#include "driver/gpio.h"
#include <bitset>

#define N_OF_TRIAL 20

namespace{
    //Instruction Set Table
    enum instruction_code {
        W25_DEVICE_RESET   = 0xFF,
        JEDEC_ID           = 0x9F,
        READ_STATUS_REG    = 0x05,
        WRITE_STATUS_REG   = 0x01,
        WRITE_ENABLE       = 0x06,
        WRITE_DISABLE      = 0x04,
        BB_MANAG           = 0xA1, //Swap Blocks
        READ_BBM_LUT       = 0xA5,
        LAST_ECC_FAIL_ADDR = 0xA9,
        BLOCK_ERASE        = 0xD8,
        PROG_DATA_LOAD     = 0x02, //Reset Buffer
        RAND_PROG_LOAD     = 0x84,
        PROG_EXEC          = 0x10,
        PAGE_DATA_READ     = 0x13,
        READ_DATA          = 0x03,
        FAST_READ          = 0x0B
    };
}

constexpr gpio_num_t MOSI = GPIO_NUM_23;
constexpr gpio_num_t MISO = GPIO_NUM_19;
constexpr gpio_num_t SCLK = GPIO_NUM_18;
constexpr gpio_num_t CS = GPIO_NUM_5;
constexpr gpio_num_t HOLD = GPIO_NUM_17;
constexpr gpio_num_t WP = GPIO_NUM_16;

constexpr uint32_t CLOCK_SPEED = 8000000; // up to 1MHz for all registers

constexpr size_t MAX_TRANS_SIZE = 2048+4;
constexpr uint16_t MAX_ALLOWED_ADDR = 2047U;
constexpr uint16_t MAX_ALLOWED_PAGEBLOCK = 65472U; //This might be wrong, CHECK IT LATER

RTC_DATA_ATTR uint16_t current_address_RTC;
RTC_DATA_ATTR uint16_t current_column_RTC;

struct winbond{
	//cppcheck-suppress misra-c2012-2.7 
	//cppcheck-supress misra-c2012-17.8
	explicit winbond(size_t max_trans_size, TickType_t p_timeout = 1000) : dev_config{
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0, //SPI MODE
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans= 0,
        .clock_speed_hz = CLOCK_SPEED,
        .input_delay_ns = 0,
        .spics_io_num = CS,
        .flags = SPI_DEVICE_NO_DUMMY,
        .queue_size = 1,
        .pre_cb = 0,
        .post_cb = 0
    }, buffer_size{max_trans_size}, semaphore_timeout{p_timeout}{
        
        opCode = static_cast<uint8_t *>(heap_caps_malloc(max_trans_size, MALLOC_CAP_DMA)); //creates a DMA-suitable chunk of memory
        (void)memset(opCode, 0, max_trans_size);

        spi_bus_mutex = xSemaphoreCreateBinary();

        gpio_set_direction(HOLD,GPIO_MODE_OUTPUT);
        gpio_set_direction(WP,GPIO_MODE_OUTPUT);
        gpio_set_level(HOLD, 1);
        gpio_set_level(WP, 1);
    }

    spi_device_interface_config_t dev_config;
    spi_device_handle_t handle;
    uint8_t *opCode;
    size_t buffer_size;
    SemaphoreHandle_t spi_bus_mutex;
    TickType_t semaphore_timeout;

    void opCode_free(void);
};	

void winbond::opCode_free(void){
    heap_caps_free(this->opCode);
}

/*INITIALIZING THE BUS */

esp_err_t vspi_w25_alloc_bus(winbond_t *w25){

    spi_bus_config_t vspi_config = {
        .mosi_io_num = MOSI,
        .miso_io_num = MISO,
        .sclk_io_num = SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092,
        .flags = 0,
        .intr_flags = 0
    };

    esp_err_t err = spi_bus_initialize(VSPI_HOST, &vspi_config, 2);
    if (err == ESP_OK){
        err = spi_bus_add_device(VSPI_HOST, &w25->dev_config, &w25->handle);
    }else{
        err = ESP_FAIL;
    }
    xSemaphoreGive(w25->spi_bus_mutex);

    return err;

}
esp_err_t vspi_w25_free_bus(winbond_t *w25){
    esp_err_t err = ESP_FAIL;
    auto sem_timeout = xSemaphoreTake(w25->spi_bus_mutex, w25->semaphore_timeout);
    if (sem_timeout == pdTRUE){
        err = spi_bus_remove_device(w25->handle);
        if (err == ESP_OK){
            err = spi_bus_free(VSPI_HOST);
        }else{
            err = ESP_FAIL;
        }
        xSemaphoreGive(w25->spi_bus_mutex);
    }else{
        err = ESP_ERR_TIMEOUT;
    }
    return err;
}

winbond_t *init_w25_struct(size_t max_trans_size){
    assert(max_trans_size <= MAX_TRANS_SIZE);
	winbond_t *w25 = new winbond_t(max_trans_size);
	return w25;
}

esp_err_t deinit_w25_struct(winbond_t *w25){
    auto sem_timeout = xSemaphoreTake(w25->spi_bus_mutex, w25->semaphore_timeout); //If the bus isn't initialized, 
                                                       //this semaphore won't be never taken (DANGER!!!)
    esp_err_t err = ESP_OK;
    if (sem_timeout == pdTRUE){
        w25->opCode_free();
        delete(w25);
    }else{
        err = ESP_ERR_TIMEOUT;
    }
    return err;
}

uint16_t w25_RecoverCurrentAddr(void){
    return current_address_RTC;
}

esp_err_t w25_CommitCurrentAddr(uint16_t page_addr){
    esp_err_t err = ESP_OK;
    current_address_RTC = page_addr;
    return err;
}

uint16_t w25_RecoverCurrentColumn(void){
    return current_column_RTC;
}

esp_err_t w25_CommitCurrentColumn(uint16_t column_addr){
    esp_err_t err = ESP_OK;
    current_column_RTC = column_addr;
    return err;
}

static esp_err_t vspi_transmission(const uint8_t *opCode, size_t opCode_size, uint8_t *out_buffer, spi_device_handle_t handle, SemaphoreHandle_t spi_bus_mutex, TickType_t timeout){
    spi_transaction_t transaction = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = opCode_size*size_t{8},
        .rxlength = 0,
        .user = nullptr,
        .tx_buffer = opCode,
        .rx_buffer = out_buffer
    };
    auto sem_timeout = xSemaphoreTake(spi_bus_mutex, timeout);
    esp_err_t err = ESP_FAIL;
    if (sem_timeout == pdTRUE){
        err = spi_device_transmit(handle,&transaction);
        xSemaphoreGive(spi_bus_mutex);
    }else{
        err = ESP_ERR_TIMEOUT;
    }
    return err;
}

/* LOW LEVEL DRIVER FUNCTIONS*/

esp_err_t w25_Reset(const winbond_t *w25){
    
    esp_err_t err = ESP_OK;
    
    while(w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),STAT_BUSY)){
        ESP_LOGW("MEMORY IS BUSY","\n");
        vTaskDelay(1);
    }
    uint8_t opCode[] = {instruction_code::W25_DEVICE_RESET};
    err = vspi_transmission(opCode,sizeof(opCode),nullptr,w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);
    
    return err;
}

esp_err_t w25_GetJedecID(const winbond_t *w25, uint8_t *out_buffer, size_t buffer_size){
    assert(buffer_size >= size_t{3});
    uint8_t opCode[5] = {instruction_code::JEDEC_ID, 0x00, 0x00, 0x00, 0x00};
    esp_err_t err = vspi_transmission(opCode,sizeof(opCode),opCode,w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);
    out_buffer[0] = opCode[2];
    out_buffer[1] = opCode[3];
    out_buffer[2] = opCode[4];
    return err;
}

uint8_t w25_ReadStatusRegister(const winbond_t *w25, reg_addr register_address){
    uint8_t opCode[9] = {instruction_code::READ_STATUS_REG, register_address, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t buffer = 0;
    esp_err_t err = vspi_transmission(opCode,sizeof(opCode),opCode,w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);
    assert(err==ESP_OK);
    if (err == ESP_OK){
        buffer = opCode[2];
    }
    return buffer;
}

bool w25_evaluateStatusRegisterBit(uint8_t registerOutput, uint8_t bitValue){
    return ((registerOutput & bitValue) > 0U); //This should the only used to test individual bits!!! (bit masks aren't allowed here)
}

esp_err_t w25_WriteStatusRegister(const winbond_t *w25, reg_addr register_address, uint8_t bitValue){
    uint8_t opCode[3] = {instruction_code::WRITE_STATUS_REG, register_address, bitValue};
    return vspi_transmission(opCode,sizeof(opCode), nullptr, w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);
}

esp_err_t w25_WritePermission(const winbond_t *w25, bool state){
    uint8_t opCode[1];
    if (state){
        opCode[0] = WRITE_ENABLE;
    }else{
        opCode[0] = WRITE_DISABLE;
    }
    return vspi_transmission(opCode,1,nullptr,w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);
}

esp_err_t w25_ReadDataBuffer(const winbond_t *w25, uint16_t column_addr, uint8_t *out_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;
    assert(column_addr<=MAX_ALLOWED_ADDR); //MAXIMUM ALLOWED ADDRESS
    while(w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),STAT_BUSY)){
        ESP_LOGW("MEMORY IS BUSY","\n");
        vTaskDelay(1);
    }
    uint8_t *p_column_bits = reinterpret_cast<uint8_t *>(&column_addr);
        
    w25->opCode[0] = instruction_code::READ_DATA;
    w25->opCode[1] = p_column_bits[1];
    w25->opCode[2] = p_column_bits[0];
    w25->opCode[3] = 0x66; //Dummy byte

    assert((buffer_size + size_t{4}) <= w25->buffer_size); //Size of the sent message cant be bigger than the actual transmitted buffer
    err = vspi_transmission(w25->opCode, buffer_size + size_t{4}, w25->opCode, w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout); //THIS LINE IS CORRUPTING THE HEAP MEMORY
    heap_caps_check_integrity_all(true); //CHECKS THE INTEGRITY OF THE ENTIRE HEAP
    (void)memcpy(out_buffer,&w25->opCode[4],buffer_size);
    heap_caps_check_integrity_all(true); //CHECKS THE INTEGRITY OF THE ENTIRE HEAP
     
    return err;
}

esp_err_t w25_PageDataRead(const winbond_t *w25, uint16_t page_addr){
    assert(page_addr<MAX_ALLOWED_PAGEBLOCK);
    uint8_t opCode[4] = {instruction_code::PAGE_DATA_READ,0x00,0x00,0x00};
    uint8_t *p_page_addr = reinterpret_cast<uint8_t *>(&page_addr);

    opCode[2] = p_page_addr[1];
    opCode[3] = p_page_addr[0];

    return vspi_transmission(opCode, sizeof(opCode), nullptr, w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);

}

esp_err_t w25_BlockErase(const winbond_t *w25, uint16_t page_addr){
    esp_err_t err = ESP_ERR_INVALID_ARG;
    
    if (page_addr < MAX_ALLOWED_PAGEBLOCK){
        uint16_t block = (65472U & page_addr);
        uint8_t opCode[4] = {instruction_code::BLOCK_ERASE,0x00,0x00,0x00};
        uint8_t *p_page_addr = reinterpret_cast<uint8_t *>(&block);

        opCode[2] = p_page_addr[1];
        opCode[3] = p_page_addr[0];

        w25_WritePermission(w25,true);
        err = vspi_transmission(opCode, sizeof(opCode), nullptr, w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);

        while(w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),STAT_BUSY)){ //The BUSY bit is a 1 during the Block Erase cycle and becomes a 0 when the cycle is finished 
            ESP_LOGW("MEMORY IS BUSY","\n");
            vTaskDelay(1);
        }

        if (w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),E_FAIL)){
            err = ESP_ERR_INVALID_STATE;
        }
    }
    return err;
}

esp_err_t w25_LoadProgramData(const winbond_t *w25, uint16_t column_addr, const uint8_t *in_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;
    assert(column_addr <= MAX_ALLOWED_ADDR); //MAXIMUM ALLOWED ADDRESS
    assert(buffer_size <= size_t{2048+4});

    uint8_t *p_column_bits = reinterpret_cast<uint8_t *>(&column_addr);

    w25->opCode[0] = instruction_code::PROG_DATA_LOAD;
    w25->opCode[1] = p_column_bits[1];
    w25->opCode[2] = p_column_bits[0];
    
    (void)memcpy(&(w25->opCode[3]),in_buffer,buffer_size);

    w25_WritePermission(w25,true);
    err = vspi_transmission(w25->opCode, buffer_size + size_t{3}, w25->opCode, w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);
   
    return err;
}

esp_err_t w25_ProgramExecute(const winbond_t *w25, uint16_t page_addr, uint16_t max_trial_nmb){
    
    assert(page_addr<MAX_ALLOWED_PAGEBLOCK);
    uint8_t opCode[4] = {instruction_code::PROG_EXEC,0x00,0x00,0x00};
    uint8_t *p_page_addr = reinterpret_cast<uint8_t *>(&page_addr);

    opCode[2] = p_page_addr[1];
    opCode[3] = p_page_addr[0];

    esp_err_t err = vspi_transmission(opCode, sizeof(opCode), nullptr, w25->handle, w25->spi_bus_mutex, w25->semaphore_timeout);

    if (w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),P_FAIL)){
        err = ESP_ERR_INVALID_STATE;
    }else if (err == ESP_OK){
        uint16_t trial = 0;
        while(w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),STAT_BUSY)){
            ESP_LOGW("MEMORY IS BUSY","\n");
            if(trial >= max_trial_nmb){ //This ends the endless loop when the nmb of trials is exceeded
                err = ESP_ERR_TIMEOUT;
                break;
            }
            trial++;
            vTaskDelay(1);
        }
    }else{
        err = ESP_FAIL;
    }
    return err;
}

//High Level Functions

esp_err_t w25_Initialize(const winbond_t *w25){
    esp_err_t err = ESP_OK;

    if (w25 == nullptr){
        err = ESP_ERR_NOT_FOUND;
    }else{

        ESP_ERROR_CHECK(w25_Reset(w25));
        vTaskDelay(800/portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(w25_WriteStatusRegister(w25, CONFIG_REG, ECC_E|BUF|0x00));
        ESP_ERROR_CHECK(w25_WriteStatusRegister(w25, PROTEC_REG, 0x00));
    }

    return err;
}

esp_err_t w25_ReadMemory(const winbond_t *w25, uint16_t column_addr, uint16_t page_addr, uint8_t *out_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;

    ESP_ERROR_CHECK(w25_PageDataRead(w25, page_addr));
    err = w25_ReadDataBuffer(w25, column_addr, out_buffer, buffer_size);

    if((err != ESP_OK)){
        ESP_LOGE("READ MEMORY ERROR: ", "ESP_FAIL");
        err = ESP_FAIL;
    }

    if((w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),ECC_1))){
        ESP_LOGW("Wrong ECC_1: ", " Values might've been wrongly read");
    }

    return err;
}

esp_err_t w25_WriteMemory(const winbond_t *w25, uint16_t column_addr, uint16_t page_addr, const uint8_t *in_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;
    err = w25_LoadProgramData(w25, column_addr, in_buffer, buffer_size);
    
    if (err == ESP_OK){
        err = w25_ProgramExecute(w25, page_addr, N_OF_TRIAL);
    }

    return err;
}
