#ifndef W25N_H
#define W25N_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>

//Protection Register Bit Fields
#define SRP0       0b10000000 //Status Regiter Protect-0 (Volatile Writable, OTP Lock)
#define BP3        0b01000000 //Block Protect Bits (Volatile Writable, OTP Lock)
#define BP2        0b00100000 //Block Protect Bits (Volatile Writable, OTP Lock)
#define BP1        0b00010000 //Block Protect Bits (Volatile Writable, OTP Lock)
#define BP0        0b00001000 //Block Protect Bits (Volatile Writable, OTP Lock)
#define TB         0b00000100 //Top/Botton Procted Bits (Volatile Writable, OTP Lock)
#define WP_E       0b00000010 ///WP Enable Bit (Volatile Writable, OTP Lock)
#define SRP1       0b00000001 //Status Register Protect-1 (Volatile Writable, OTP Lock)
#define BPB        BP3|BP2|BP1|BP0 //Block Protect Bits

//Configuration Register Bit Fields
#define OTP_L      0b10000000 //OTP Data Pages Lock (OTP Lock)
#define OTP_E      0b01000000 //Enter OTP Mode (Volatile Writable)
#define SR1_L      0b00100000 //Status Regiter-1 Lock (OTP Lock)
#define ECC_E      0b00010000 //Enable ECC (Volatile Writable)
#define BUF        0b00001000 //Buffer Mode (Volatile Writable)

//Status Register-3
#define LUT_F      0b01000000 //BBM LUT Full (Status-Only)
#define ECC_1      0b00100000 //ECC Status Bit (Status-Only)
#define P_FAIL     0b00001000 //Program Failure (Status-Only)
#define E_FAIL     0b00000100 //Erase Failure (Status-Only)
#define WEL        0b00000010 //Write Enable Latch (Status-Only)
#define STAT_BUSY  0b00000001

#define WINBOND_MAN_ID       0xEF
#define W25_DEV_ID           0xAA21

//Registers
typedef enum {
	PROTEC_REG = 0xA0,
	CONFIG_REG = 0xB0,
	STATUS_REG = 0xC0
} reg_addr;

typedef struct winbond winbond_t;

winbond_t *init_w25_struct(size_t max_trans_size);
esp_err_t deinit_w25_struct(winbond_t *w25);
esp_err_t vspi_w25_alloc_bus(winbond_t *w25);
esp_err_t vspi_w25_free_bus(winbond_t *w25);

uint16_t w25_RecoverCurrentAddr(void);
esp_err_t w25_CommitCurrentAddr(uint16_t page_addr);
uint16_t w25_RecoverCurrentColumn(void);
esp_err_t w25_CommitCurrentColumn(uint16_t column_addr);

/**
Resets the memory to its initial state, clearing volatile registers
@param winbond_t* **w25** - pointer to the object refered to.
@return **esp_err_t** - Error code according to esp idf documentation. 
*/
esp_err_t w25_Reset(const winbond_t *w25);
/**
Retrieves the memory's JEDEC ID. This function can be used to verify 
if the device can be reached.
@param winbond_t* **w25** - pointer to the object refered to.
@return **esp_err_t** - Error code according to esp idf documentation.
*/
esp_err_t w25_GetJedecID(const winbond_t *w25, uint8_t *out_buffer, size_t buffer_size);
/**
Read the Status Register and return its value. Look up the definitions to know the purpose
of each individual register 
@param winbond_t* **w25** - pointer to the object refered to.
@oaram reg_addr **register_address** - the address of the register you want to read
@return **uint8_t** returns the current register's value
*/
uint8_t w25_ReadStatusRegister(const winbond_t *w25, reg_addr register_address);
/**
Checks if a certain bit field of the register is set
@param uint8_t **registerOutput** - The output received by the function ReadStatusRegister
@param uint8_t **bitValue** - The especific bit field wanted
@return **bool** return true if the register is set, false otherwise
*/
bool w25_evaluateStatusRegisterBit(uint8_t registerOutput, uint8_t bitValue);
/**
Write the Status Register and returns if it was successful.
@param winbond_t* **w25** - pointer to the object refered to.
@param reg_addr **register_address** - the address of the register you want to read
@param uint8_t **bitValue** - bitmask of the values to store in the Status register
\bug This function overwrites the previous configuration of the register.
\attention Pay attention to which register are read only
@return **esp_err_t** - Error code according to esp idf documentation.
*/
esp_err_t w25_WriteStatusRegister(const winbond_t *w25, reg_addr register_address, uint8_t bitValue);
/**
Turns on or off the ability to write into memory, including the internal registers.  
@param winbond_t* **w25** - pointer to the object refered to.
@param bool **state** - true if you want to enable the memory to be written, false otherwise.
@return **esp_err_t** - Error code according to esp idf documentation.
*/
esp_err_t w25_WritePermission(const winbond_t *w25, bool state);
/**

*/
esp_err_t w25_ReadDataBuffer(const winbond_t *w25, uint16_t column_addr, uint8_t *out_buffer, size_t buffer_size);
esp_err_t w25_PageDataRead(const winbond_t *w25, uint16_t page_addr);
/**
Sets all memory of the specified block field on the (page_addr) to the default value.
\remark See the Issues tab on the github repository for more information on how to use this function.
@param winbond_t* **w25** - pointer to the object refered to.
@param uint16_t **page_addr** - the block to be erase corresponds to the 10 most significant bits of the page_addr
@return **esp_err_t** - Error code according to esp idf documentation.
*/
esp_err_t w25_BlockErase(const winbond_t *w25, uint16_t page_addr);
/**

*/
esp_err_t w25_LoadProgramData(const winbond_t *w25, uint16_t column_addr, const uint8_t *in_buffer, size_t buffer_size);
esp_err_t w25_ProgramExecute(const winbond_t *w25, uint16_t page_addr);

esp_err_t w25_Initialize(const winbond_t *w25);
esp_err_t w25_ReadMemory(const winbond_t *w25, uint16_t column_addr, uint16_t page_addr, uint8_t *out_buffer, size_t buffer_size);
esp_err_t w25_WriteMemory(const winbond_t *w25, uint16_t column_addr, uint16_t page_addr, const uint8_t *in_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif