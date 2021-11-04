#ifndef W25N_H
#define W25N_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>

#define MAX_TRANS_SIZE 2048+4
#define MAX_ALLOWED_ADDR 0x07FF

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
#define ECC_1      0b00110000 //ECC Status Bit (Status-Only)
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
esp_err_t w25_reset(const winbond_t *w25);
esp_err_t w25_getJedecID(const winbond_t *w25, uint8_t *out_buffer, size_t buffer_size);
uint8_t w25_ReadStatusRegister(const winbond_t *w25, reg_addr register_address);
bool w25_evaluateStatusRegisterBit(uint8_t registerOutput, uint8_t bitValue);
esp_err_t w25_WriteStatusRegister(const winbond_t *w25, reg_addr register_address, uint8_t bitValue);
esp_err_t w25_WritePermission(const winbond_t *w25, bool state);
esp_err_t w25_ReadDataBuffer(const winbond_t *w25, uint16_t column_addr, uint8_t *out_buffer, size_t buffer_size);
esp_err_t w25_PageDataRead(const winbond_t *w25, uint16_t page_addr);
esp_err_t w25_BlockErase(const winbond_t *w25, uint16_t page_addr);
esp_err_t w25_LoadProgramData(const winbond_t *w25, uint16_t column_addr, const uint8_t *in_buffer, size_t buffer_size);
esp_err_t w25_ProgramExecute(const winbond_t *w25, uint16_t page_addr);


#ifdef __cplusplus
}
#endif

#endif