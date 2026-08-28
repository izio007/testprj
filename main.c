#include <stdio.h>
#include <string.h>
#include "tracking_interface.h"
#include "scenario_parser.h"
#include "sim_core.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("[USAGE] Configuration error. Use: %s <path_to_scenario.toml>\n", argv[0]);
        return 1;
    }

    const char* config_filepath = argv[1];
    TestScenarioContext test_context;

    // Вызов обновленной англоязычной функции парсера
    if (!test_env_load_toml_config(config_filepath, &test_context)) {
        printf("[CRITICAL] Error parsing structured TOML files.\n");
        return 1;
    }

    TuningBuffer core_memory_buffer;
    memset(&core_memory_buffer, 0, sizeof(TuningBuffer));

    int verification_result = run_simulator_loop(&test_context, &core_memory_buffer);

    printf("\n=======================================================================\n");
    if (verification_result == 0) {
        printf("✅ [TEST PASSED STATUS: SUCCESS]\n");
        return 0;
    } else {
        printf("❌ [TEST FAILED STATUS: REJECTED]\n");
        return 1;
    }
}
