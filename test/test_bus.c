#include "unity.h"
#include "W25N01GV.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_log.h"

#define SIZE 2048

static winbond_t *w25 = NULL;

void setUp(void) {

    // w25 = init_w25_struct(2048 + 4);
    // vspi_w25_alloc_bus(w25);

	// w25_Initialize(w25);
}

void tearDown(void) {
    // vspi_w25_free_bus(w25);
    // deinit_w25_struct(w25);
}

TEST_CASE("struct initialization", "[init denit]")
{
    w25 = init_w25_struct(2048 + 4);
    vspi_w25_alloc_bus(w25);
	TEST_ASSERT_NOT_NULL(w25);
	esp_err_t err = w25_Initialize(w25);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_Initialize(NULL);
	TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
}

TEST_CASE("JEDED", "[]")
{
	uint8_t jedec_id[3] = {0xEF,0xAA,0x21};
	uint8_t out_buffer[3] = {0};
	esp_err_t err = w25_GetJedecID(w25, out_buffer, 3);
	TEST_ASSERT_EQUAL_INT (ESP_OK, err);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(jedec_id, out_buffer, 3);
}

TEST_CASE("ERASE_BLOCK", "[]")
{	
	esp_err_t err = w25_BlockErase(w25, 0b0000000000000000, 20U);
	TEST_ASSERT_EQUAL_INT (ESP_OK, err);
	err = w25_BlockErase(w25, 65472U, 20U);
	TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
	err = w25_BlockErase(w25, 0b0000000100000000, 20U);
	TEST_ASSERT_EQUAL_INT (ESP_OK, err);
}

TEST_CASE("LOAD/READ TO THE INTERMEDIARY BUFFER", "[]"){
	uint8_t jedec_id[3] = {0xEF,0xAA,0x21};
	uint8_t jedec_recv[3] = {0,0,0};
	esp_err_t err = ESP_FAIL;

	err = w25_LoadProgramData(w25, 0x0000, jedec_id, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_ReadDataBuffer(w25, 0x0000, jedec_recv, 3, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	TEST_ASSERT_EQUAL_HEX8_ARRAY(jedec_id, jedec_recv, 3);
}

TEST_CASE("WRITE/READ 1 PAGE (ERASE_BLOCK FIRST)", "[]"){
	uint8_t jedec_id[3] = {0xAA,0xBB,0xC1};
	uint8_t jedec_recv[3] = {0,0,0};

	esp_err_t err = w25_BlockErase(w25, 0x0000, 20U);
	err = w25_WriteMemory(w25, 0x0000, 0x0000, jedec_id, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_ReadMemory(w25, 0x0000, 0x0000, jedec_recv, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	TEST_ASSERT_EQUAL_HEX8_ARRAY(jedec_id, jedec_recv, 3);

}

TEST_CASE("WRITE/READ 2 PAGES (ERASE_BLOCK FIRST)", "[]"){
	uint8_t jedec_id[3] = {0xA0,0x0B,0xC1};
	uint8_t jedec_id2[3] = {0xAA,0xBB,0xC1};
	uint8_t jedec_recv[3] = {0,0,0};

	esp_err_t err = w25_BlockErase(w25, 0x0000, 20U);
	err = w25_WriteMemory(w25, 0x0000, 0x0000, jedec_id, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_WriteMemory(w25, 0x0000, 0x0001, jedec_id2, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	err = w25_ReadMemory(w25, 0x0000, 0x0000, jedec_recv, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(jedec_id, jedec_recv, 3);
	
	err = w25_ReadMemory(w25, 0x0000, 0x0001, jedec_recv, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(jedec_id2, jedec_recv, 3);
	
}

TEST_CASE("WRITE/READ DIFFERENT BLOCKS (ERASE FIRST)", "[]"){
	uint8_t jedec_id[3] = {0xA2,0x2B,0x21};
	uint8_t jedec_id2[3] = {0x11,0xB2,0xC1};
	uint8_t jedec_id3[3] = {0xFF, 0xFF, 0xFF}; //It means the memory is clean
	uint8_t jedec_recv[3] = {0,0,0};

	esp_err_t err = w25_BlockErase(w25, 0x0000, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_WriteMemory(w25, 0x0000, 0x0000, jedec_id, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_BlockErase(w25, 0x0100, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_WriteMemory(w25, 0x0000, 0x0100, jedec_id2, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	err = w25_ReadMemory(w25, 0x0000, 0x0000, jedec_recv, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(jedec_id, jedec_recv, 3);
	
	err = w25_ReadMemory(w25, 0x0000, 0x0100, jedec_recv, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(jedec_id2, jedec_recv, 3);

	err = w25_ReadMemory(w25, 0x0000, 0x0001, jedec_recv, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(jedec_id3, jedec_recv, 3);
}

TEST_CASE("PROPERLY ERASING BLOCKS", "[]"){
	uint8_t big_chunk1[10] = {0};
	uint8_t big_chunk2[10] = {1};
	memset(big_chunk2,1,10);
	uint8_t receiver[10] = {2};
	uint8_t clean_memory[10] = {0xFF}; //Vector to represent a clean memory
	memset (clean_memory,0xFF,10);

	esp_err_t err = w25_BlockErase(w25, 0x0000, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_WriteMemory(w25, 0x0000, 0x0000, big_chunk1, 10);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_BlockErase(w25, 0x0001, 20U); //Blocks are represented by the second 8 bits of the address
									   //Changing the first 8 bits will re-erase the block over and over again
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_ReadMemory(w25, 0x0000, 0x0000, receiver, 10);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(clean_memory, receiver, 10);

	//The next correct block should be
	err = w25_BlockErase(w25, 0x0100, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_WriteMemory(w25, 0x0000, 0x0100, big_chunk2, 10);
	err = w25_ReadMemory(w25, 0x0000, 0x0100, receiver, 10);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(big_chunk2, receiver, 10);
}

TEST_CASE("WRITING BIG CHUNKS OF DATA", "[]"){
	uint8_t big_chunk1[1024] = {0};
	memset(big_chunk1,1,1024);
	uint8_t receiver[512] = {0};
	uint8_t clean_memory[512] = {0xFF}; //Vector to represent a clean memory
	memset (clean_memory,0xFF,512);

	esp_err_t err = w25_BlockErase(w25, 0x0100, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_WriteMemory(w25, 0x0000, 0x0100, big_chunk1, 512);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_ReadMemory(w25, 0x0000, 0x0100, receiver, 512);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	TEST_ASSERT_EQUAL_HEX8_ARRAY(big_chunk1, receiver, 512);
}

TEST_CASE("WRITING FLOAT ARRAYS", "[]"){
	float big_float_chunk[3][128] = {{0},{79.0,36.5,0,89.0,15,89.46,0.687},{0}};
	uint8_t *p_big_float_chunk = NULL;
	uint8_t receiver[128*4] = {0};
	float *p_receiver_float = NULL;

	esp_err_t err = w25_BlockErase(w25, 0x0000, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	p_big_float_chunk = (uint8_t *)&big_float_chunk[1][0];
	err = w25_WriteMemory(w25, 0x0000, 0x0000, p_big_float_chunk, 128*4);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	err = w25_ReadMemory(w25, 0x0000, 0x0000, receiver, 128*4);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	p_receiver_float = (float *)&receiver;
	TEST_ASSERT_EQUAL_FLOAT_ARRAY(&big_float_chunk[1], p_receiver_float, 128);
}

TEST_CASE("Continuous writing, same block, Start from 0", "[]"){

	esp_err_t err = w25_BlockErase(w25, 0, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	
	//First written Block - Start
	
	const float first_chunk[3] = {1.2, 3.1415, -15.02};
	uint8_t *first_chunkUINT8_T = (uint8_t *)(&first_chunk[0]);

	uint8_t first_chunkUINT8_T_RECEIVED[3*sizeof(float)] = {0};
	float *first_chunk_RECEIVED = NULL;
	
	const uint8_t second_chunk[3] = {2, 225, 15};
	uint8_t second_chunk_RECEIVED[3] = {0};

	uint8_t huge_temp_array[(3*sizeof(float)+3)] = {0};
	memcpy(&huge_temp_array[0], first_chunkUINT8_T, 3*sizeof(float));
	memcpy(&huge_temp_array[3*sizeof(float)], &second_chunk[0], 3);

	err = w25_WriteMemory(w25, 0x0000, 0x0000, huge_temp_array, (3*sizeof(float)+3));
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	err = w25_ReadMemory(w25, 0x0000, 0x0000, first_chunkUINT8_T_RECEIVED, 3*sizeof(float));
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	err = w25_ReadMemory(w25, 3*sizeof(float), 0x0000, second_chunk_RECEIVED, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	first_chunk_RECEIVED = (float *)&first_chunkUINT8_T_RECEIVED[0];
	TEST_ASSERT_EQUAL_FLOAT_ARRAY(first_chunk, first_chunk_RECEIVED, 3);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(second_chunk, second_chunk_RECEIVED, 3);	

	
}

TEST_CASE("Continuous writing, same block", "[]"){
	esp_err_t err = w25_BlockErase(w25, 0, 20U);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);
	
	//First written Block - Start
	
	const float first_chunk[3] = {1.2, 3.1415, -15.02};
	uint8_t *first_chunkUINT8_T = (uint8_t *)(&first_chunk[0]);

	uint8_t first_chunkUINT8_T_RECEIVED[3*sizeof(float)] = {0};
	float *first_chunk_RECEIVED = NULL;
	
	const uint8_t second_chunk[3] = {2, 225, 15};
	uint8_t second_chunk_RECEIVED[3] = {0};

	err = w25_WriteMemory(w25, 0x0000, 0x0000, first_chunkUINT8_T, (3*sizeof(float)));
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	err = w25_WriteMemory(w25, (3*sizeof(float)), 0x0000, second_chunk, (3));
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);


	err = w25_ReadMemory(w25, 0x0000, 0x0000, first_chunkUINT8_T_RECEIVED, 3*sizeof(float));
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	err = w25_ReadMemory(w25, 3*sizeof(float), 0x0000, second_chunk_RECEIVED, 3);
	TEST_ASSERT_EQUAL_INT(ESP_OK, err);

	first_chunk_RECEIVED = (float *)&first_chunkUINT8_T_RECEIVED[0];
	TEST_ASSERT_EQUAL_FLOAT_ARRAY(first_chunk, first_chunk_RECEIVED, 3);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(second_chunk, second_chunk_RECEIVED, 3);	


}