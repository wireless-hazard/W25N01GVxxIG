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

struct winbond{
	explicit winbond(size_t max_trans_size) : dev_config{
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
    }, buffer_size{max_trans_size}{
        
        opCode = static_cast<uint8_t *>(heap_caps_malloc(max_trans_size, MALLOC_CAP_DMA)); //creates a DMA-suitable chunk of memory
        memset(opCode, 0, max_trans_size);

        spi_bus_mutex = xSemaphoreCreateBinary();

        gpio_set_direction(HOLD,GPIO_MODE_OUTPUT);
        gpio_set_direction(WP,GPIO_MODE_OUTPUT);
        gpio_set_level(HOLD, 1); // 0 = Puts the memory out of the pause state
        gpio_set_level(WP, 1); //0 =  Read only hardware protection state
    }

    spi_device_interface_config_t dev_config;
    spi_device_handle_t handle;
    uint8_t *opCode;
    size_t buffer_size;
    SemaphoreHandle_t spi_bus_mutex;

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
    xSemaphoreTake(w25->spi_bus_mutex, portMAX_DELAY);
    esp_err_t err = spi_bus_remove_device(w25->handle);
    if (err == ESP_OK){
        err = spi_bus_free(VSPI_HOST);
    }else{
        err = ESP_FAIL;
    }
    xSemaphoreGive(w25->spi_bus_mutex);
    return err;
}

winbond_t *init_w25_struct(size_t max_trans_size){
    assert(max_trans_size <= MAX_TRANS_SIZE);
	winbond_t *w25 = new winbond_t(max_trans_size);
	return w25;
}

esp_err_t deinit_w25_struct(winbond_t *w25){
    xSemaphoreTake(w25->spi_bus_mutex, portMAX_DELAY);
    w25->opCode_free();
    delete(w25);
    return ESP_OK;
}

uint16_t w25_RecoverCurrentAddr(void){
    return current_address_RTC;
}

esp_err_t w25_CommitCurrentAddr(uint16_t page_addr){
    esp_err_t err = ESP_OK;
    current_address_RTC = page_addr;
    return err;
}

static esp_err_t vspi_transmission(const uint8_t *opCode, size_t opCode_size, uint8_t *out_buffer, spi_device_handle_t handle, SemaphoreHandle_t spi_bus_mutex){
    spi_transaction_t transaction = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = opCode_size*static_cast<size_t>(8),
        .rxlength = 0,
        .user = NULL,
        .tx_buffer = opCode,
        .rx_buffer = out_buffer
    };
    xSemaphoreTake(spi_bus_mutex, portMAX_DELAY);
    esp_err_t err = spi_device_transmit(handle,&transaction);
    xSemaphoreGive(spi_bus_mutex);
    return err;
}

/* LOW LEVEL DRIVER FUNCTIONS*/

esp_err_t w25_Reset(const winbond_t *w25){
    
    esp_err_t err = ESP_OK;
    if (w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),STAT_BUSY)){ //RESETS IF THE MEMORY IS NOT BUSY

        err = ESP_FAIL;

    }else{

        uint8_t opCode[] = {instruction_code::W25_DEVICE_RESET};
        err = vspi_transmission(opCode,sizeof(opCode),NULL,w25->handle, w25->spi_bus_mutex);
    }
    return err;
}

esp_err_t w25_GetJedecID(const winbond_t *w25, uint8_t *out_buffer, size_t buffer_size){
    assert(buffer_size >= static_cast<size_t>(3));
    uint8_t opCode[5] = {instruction_code::JEDEC_ID, 0x00, 0x00, 0x00, 0x00};
    esp_err_t err = vspi_transmission(opCode,sizeof(opCode),opCode,w25->handle, w25->spi_bus_mutex);
    out_buffer[0] = opCode[2];
    out_buffer[1] = opCode[3];
    out_buffer[2] = opCode[4];
    return err;
}

uint8_t w25_ReadStatusRegister(const winbond_t *w25, reg_addr register_address){
    uint8_t opCode[9] = {instruction_code::READ_STATUS_REG, register_address, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t buffer = 0;
    esp_err_t err = vspi_transmission(opCode,sizeof(opCode),opCode,w25->handle, w25->spi_bus_mutex);
    assert(err==ESP_OK);
    if (err == ESP_OK){
        buffer = opCode[2];
    }
    return buffer;
}

bool w25_evaluateStatusRegisterBit(uint8_t registerOutput, uint8_t bitValue){
    return ((registerOutput & bitValue) > 0U); 
}

esp_err_t w25_WriteStatusRegister(const winbond_t *w25, reg_addr register_address, uint8_t bitValue){
    uint8_t opCode[3] = {instruction_code::WRITE_STATUS_REG, register_address, bitValue};
    return vspi_transmission(opCode,sizeof(opCode),NULL,w25->handle, w25->spi_bus_mutex);
}

esp_err_t w25_WritePermission(const winbond_t *w25, bool state){
    uint8_t opCode[1];
    if (state){
        opCode[0] = WRITE_ENABLE;
    }else{
        opCode[0] = WRITE_DISABLE;
    }
    return vspi_transmission(opCode,1,NULL,w25->handle, w25->spi_bus_mutex);
}

esp_err_t w25_ReadDataBuffer(const winbond_t *w25, uint16_t column_addr, uint8_t *out_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;
    assert(column_addr<=MAX_ALLOWED_ADDR); //MAXIMUM ALLOWED ADDRESS
    if (w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),STAT_BUSY)){ //CHECKS IF MEMORY IS BUSY
        err = ESP_FAIL;
    }else{
        w25->opCode[0] = instruction_code::READ_DATA;
        w25->opCode[3] = 0x66; //Dummy byte
        std::bitset<16> column_bits{column_addr};

        for (int i = 0; i < 8; i++)
        {
            w25->opCode[2] |= (column_bits[i]<<i);
            w25->opCode[1] |= (column_bits[i+8]<<i);
        }
        assert((buffer_size + static_cast<size_t>(4)) <= w25->buffer_size); //Size of the sent message cant be bigger than the actual transmitted buffer
        err = vspi_transmission(w25->opCode,buffer_size+4,w25->opCode,w25->handle, w25->spi_bus_mutex); //THIS LINE IS CORRUPTING THE HEAP MEMORY
        heap_caps_check_integrity_all(true); //CHECKS THE INTEGRITY OF THE ENTIRE HEAP
        memcpy(out_buffer,w25->opCode+4,buffer_size);
        heap_caps_check_integrity_all(true); //CHECKS THE INTEGRITY OF THE ENTIRE HEAP
    } 
    return err;
}

esp_err_t w25_PageDataRead(const winbond_t *w25, uint16_t page_addr){
    assert(page_addr<MAX_ALLOWED_PAGEBLOCK);
    uint8_t opCode[4] = {instruction_code::PAGE_DATA_READ,0x00,0x00,0x00};
    std::bitset<16> page_bits{page_addr};

    for (int i = 0; i < 8; i++)
    {
        opCode[3] |= (page_bits[i]<<i);
        opCode[2] |= (page_bits[i+8]<<i);
    }

    return vspi_transmission(opCode, sizeof(opCode), NULL, w25->handle, w25->spi_bus_mutex);

}

esp_err_t w25_BlockErase(const winbond_t *w25, uint16_t page_addr){
    assert(page_addr<MAX_ALLOWED_PAGEBLOCK);
    uint16_t block = (65472U & page_addr);
    uint8_t opCode[4] = {instruction_code::BLOCK_ERASE,0x00,0x00,0x00};
    std::bitset<16> page_bits{block};

    for (int i = 0; i < 8; i++)
    {
        opCode[3] |= (page_bits[i]<<i);
        opCode[2] |= (page_bits[i+8]<<i);
    }
    
    w25_WritePermission(w25,true);
    esp_err_t err = vspi_transmission(opCode, sizeof(opCode), NULL, w25->handle, w25->spi_bus_mutex);

    if (w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),E_FAIL)){
        err = ESP_ERR_INVALID_STATE;
    }

    return err;
}

esp_err_t w25_LoadProgramData(const winbond_t *w25, uint16_t column_addr, const uint8_t *in_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;
    assert(column_addr<=MAX_ALLOWED_ADDR); //MAXIMUM ALLOWED ADDRESS
    assert(buffer_size <= static_cast<size_t>(2048+4) );
    w25->opCode[0] = instruction_code::PROG_DATA_LOAD;
    std::bitset<16> column_bits{column_addr};

    for (int i = 0; i < 8; i++)
    {
        w25->opCode[2] |= (column_bits[i]<<i);
        w25->opCode[1] |= (column_bits[i+8]<<i);
    }
    
    memcpy((w25->opCode)+3,in_buffer,buffer_size);
    w25_WritePermission(w25,true);
    err = vspi_transmission(w25->opCode, buffer_size+3, w25->opCode, w25->handle, w25->spi_bus_mutex);
   
    return err;
}

esp_err_t w25_ProgramExecute(const winbond_t *w25, uint16_t page_addr){
    
    assert(page_addr<MAX_ALLOWED_PAGEBLOCK);
    uint8_t opCode[4] = {instruction_code::PROG_EXEC,0x00,0x00,0x00};

    std::bitset<16> page_bits{page_addr};

    for (int i = 0; i < 8; i++)
    {
        opCode[3] |= (page_bits[i]<<i);
        opCode[2] |= (page_bits[i+8]<<i);
    }

    esp_err_t err = vspi_transmission(opCode, sizeof(opCode), NULL, w25->handle, w25->spi_bus_mutex);

    if (w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),P_FAIL)){
        err = ESP_ERR_INVALID_STATE;
    }else if (err == ESP_OK){
        while(w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),STAT_BUSY)){
            ESP_LOGW("MEMORY IS BUSY","\n");
            vTaskDelay(10);
        }
    }else{
        err = ESP_FAIL;
    }
    return err;
}

//High Level Functions

esp_err_t w25_Initialize(const winbond_t *w25){
    esp_err_t err = ESP_OK;

    ESP_ERROR_CHECK(w25_Reset(w25));
    vTaskDelay(800/portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(w25_WriteStatusRegister(w25, CONFIG_REG, ECC_E|BUF|0x00));
    ESP_ERROR_CHECK(w25_WriteStatusRegister(w25, PROTEC_REG, 0x00));

    return err;
}

esp_err_t w25_ReadMemory(const winbond_t *w25, uint16_t column_addr, uint16_t page_addr, uint8_t *out_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;

    ESP_ERROR_CHECK(w25_PageDataRead(w25, page_addr));
    err = w25_ReadDataBuffer(w25, column_addr, out_buffer, buffer_size);

    if( ((w25_evaluateStatusRegisterBit(w25_ReadStatusRegister(w25,STATUS_REG),ECC_1))) || (err != ESP_OK)){
        ESP_LOGE("READ MEMORY ERROR: ", "ESP_FAIL");
        err = ESP_FAIL;
    }

    return err;
}

esp_err_t w25_WriteMemory(const winbond_t *w25, uint16_t column_addr, uint16_t page_addr, const uint8_t *in_buffer, size_t buffer_size){
    esp_err_t err = ESP_OK;

    err = w25_LoadProgramData(w25, column_addr, in_buffer, buffer_size);
    
    if (err == ESP_OK){
        err = w25_ProgramExecute(w25, page_addr);
    }

    return err;
}