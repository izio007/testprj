#ifndef SCENARIO_PARSER_H
#define SCENARIO_PARSER_H

#include "tracking_interface.h"

/* Контекст загруженного сценария для тестового окружения */
typedef struct {
    double check_time;                               /* Время контроля (секунды) */
    char expected_status_str[MAX_STR_LEN];           /* Ожидаемый статус из TOML */
    char actual_status_str[MAX_STR_LEN];             /* Исправление: Фактический статус ядра ПО */
    bool check_id_preservation;                      /* Требование сохранения track_id */
    bool is_passed;                                  /* Исправление: Бинарный вердикт по этой точке */
    char comment[MAX_STR_LEN * 2];                    /* Исправление: Суть условия (примечание аналитика) */
} CheckpointConfig;


/* Глобальный контекст загруженного сценария */
typedef struct {
    char scenario_id[MAX_STR_LEN];                   /* Уникальный текстовый идентификатор */
    double time_step;                                /* Шаг симуляции в секундах (dt) */
    unsigned int random_seed;                        /* Стартовое значение генератора шума */
    
    SimObject objects[MAX_OBJECTS];                  /* Плоский массив РЛС и траекторий целей */
    size_t object_count;                             /* Фактическое количество объектов на сцене */
    
    CheckpointConfig checkpoints[MAX_WAYPOINTS];     /* Каскад контрольных точек проверки */
    size_t checkpoint_count;                         /* Количество чекпоинтов аналитика */
    
    double stop_max_time;                            /* Максимальный таймаут завершения теста */
    double stop_max_distance_after;                  /* Дистанция выхода за створ для автоостанова */
    char stop_condition_type[MAX_STR_LEN];           /* Тип правила прерывания симуляции */
} TestScenarioContext;

/* Публичный англоязычный интерфейс рекурсивного загрузчика TOML */
bool test_env_load_toml_config(const char* filepath, TestScenarioContext* out_ctx);

#endif /* SCENARIO_PARSER_H */
