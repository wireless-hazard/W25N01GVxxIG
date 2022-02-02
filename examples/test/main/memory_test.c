#include "unity.h"

void app_main(void){

	UNITY_BEGIN();

	unity_run_test_by_name("struct initialization");
	unity_run_test_by_name("JEDED");
	unity_run_test_by_name("ERASE_BLOCK");
	unity_run_test_by_name("LOAD/READ TO THE INTERMEDIARY BUFFER");
	unity_run_test_by_name("WRITE/READ 1 PAGE (ERASE_BLOCK FIRST)");
	unity_run_test_by_name("WRITE/READ 2 PAGES (ERASE_BLOCK FIRST)");
	unity_run_test_by_name("WRITE/READ DIFFERENT BLOCKS (ERASE FIRST)");
	unity_run_test_by_name("PROPERLY ERASING BLOCKS");
	unity_run_test_by_name("WRITING BIG CHUNKS OF DATA");
	unity_run_test_by_name("WRITING FLOAT ARRAYS");

	UNITY_END();
	// unity_run_menu();
}