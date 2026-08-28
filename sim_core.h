#ifndef SIM_CORE_H
#define SIM_CORE_H

#include "tracking_interface.h"
#include "scenario_parser.h"

/* Публичный интерфейс запуска симуляционного конвейера */
int run_simulator_loop(TestScenarioContext* ctx, TuningBuffer* buffer_OZU);

#endif /* SIM_CORE_H */
