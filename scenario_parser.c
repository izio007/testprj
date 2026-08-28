#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "toml.h"
#include "scenario_parser.h"

#define MAX_RECURSION_DEPTH 3

/**
 * @brief Вспомогательный разбор трехмерного вектора координат из TOML-массива.
 */
static Vector3D parse_toml_vector3d(toml_array_t* arr) {
    Vector3D v = {0.0, 0.0, 0.0};
    if (!arr || toml_array_nelem(arr) < 3) return v;
    toml_datum_t x = toml_array_at_double(arr, 0);
    toml_datum_t y = toml_array_at_double(arr, 1);
    toml_datum_t z = toml_array_at_double(arr, 2);
    if (x.ok) v.x = x.u.d;
    if (y.ok) v.y = y.u.d;
    if (z.ok) v.z = z.u.d;
    return v;
}

/**
 * @brief Внутренняя рекурсивная функция обхода дерева TOML-файлов и инклудов.
 */
static bool load_toml_recursive(const char* filepath, TestScenarioContext* ctx, int depth) {
    if (depth > MAX_RECURSION_DEPTH) {
        printf("[ERROR] Exceeded max TOML recursion depth: %s\n", filepath);
        return false;
    }

    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        printf("[ERROR] Failed to open TOML file: %s\n", filepath);
        return false;
    }

    char errbuf[1024];
    toml_table_t* root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!root) {
        printf("[ERROR] TOML syntax error in %s: %s\n", filepath, errbuf);
        return false;
    }

    // 1. Извлечение метаданных сценария (только на верхнем уровне вложенности)
    if (depth == 0) {
        toml_table_t* meta = toml_table_in(root, "мета");
        if (meta) {
            toml_datum_t id = toml_string_in(meta, "ид_сценария");
            toml_datum_t dt = toml_double_in(meta, "интервал_времени_сек");
            toml_datum_t seed = toml_int_in(meta, "инициализатор_шума_seed");
            if (id.ok) { strncpy(ctx->scenario_id, id.u.s, MAX_STR_LEN); free(id.u.s); }
            if (dt.ok) ctx->time_step = dt.u.d;
            if (seed.ok) ctx->random_seed = (unsigned int)seed.u.i;
        }
    }

    // 2. Пошаговый разбор объектов сценарной обстановки
    toml_array_t* obj_array = toml_array_in(root, "объекты_сцены");
    if (obj_array) {
        int num_objs = toml_array_nelem(obj_array);
        for (int i = 0; i < num_objs && ctx->object_count < MAX_OBJECTS; i++) {
            toml_table_t* obj_table = toml_table_at(obj_array, i);
            SimObject* obj = &ctx->objects[ctx->object_count];
            memset(obj, 0, sizeof(SimObject));

            toml_datum_t id = toml_int_in(obj_table, "id");
            toml_datum_t type = toml_string_in(obj_table, "тип");
            toml_datum_t mode = toml_string_in(obj_table, "режим_движения");
            toml_datum_t loop = toml_bool_in(obj_table, "циклический_маршрут");

            if (id.ok) obj->id = id.u.i;
            if (type.ok) { strncpy(obj->type, type.u.s, MAX_STR_LEN); free(type.u.s); }
            if (mode.ok) { strncpy(obj->movement_mode, mode.u.s, MAX_STR_LEN); free(mode.u.s); }
            if (loop.ok) obj->is_route_looped = loop.u.b;

            // Загрузка статических координат
            toml_array_t* coord_arr = toml_array_in(obj_table, "координаты");
            if (coord_arr) {
                obj->pos_current = parse_toml_vector3d(coord_arr);
                obj->pos_start = obj->pos_current;
            }

            // Загрузка плоского массива путевых точек маршрута
            toml_array_t* route_arr = toml_array_in(obj_table, "маршрут");
            if (route_arr) {
                int num_wps = toml_array_nelem(route_arr);
                for (int j = 0; j < num_wps && j < MAX_WAYPOINTS; j++) {
                    toml_table_t* wp_table = toml_table_at(route_arr, j);
                    toml_array_t* pt_arr = toml_array_in(wp_table, "точка");
                    toml_datum_t v_kmh = toml_double_in(wp_table, "скорость_км_ч");

                    if (pt_arr) obj->route[j].pos = parse_toml_vector3d(pt_arr);
                    if (v_kmh.ok) obj->route[j].speed_ms = v_kmh.u.d / 3.6;
                    obj->waypoint_count++;
                }
                if (obj->waypoint_count > 0) {
                    obj->pos_current = obj->route[0].pos;
                }
            }
            ctx->object_count++;
        }
    }

    // 3. Обработка сквозных инклудов (вложенных TOML под-файлов)
    toml_table_t* trajectories = toml_table_in(root, "траектории");
    if (trajectories) {
        toml_datum_t sub_file = toml_string_in(trajectories, "цель_101");
        if (sub_file.ok) {
            load_toml_recursive(sub_file.u.s, ctx, depth + 1);
            free(sub_file.u.s);
        }
    }

    toml_table_t* criteria = toml_table_in(root, "критерии_теста");
    if (criteria) {
        toml_datum_t sub_file = toml_string_in(criteria, "проверка");
        if (sub_file.ok) {
            load_toml_recursive(sub_file.u.s, ctx, depth + 1);
            free(sub_file.u.s);
        }
    }

    // 4. Загрузка условий прерывания симуляции
    toml_table_t* stop_sec = toml_table_in(root, "критерий_останова");
    if (stop_sec) {
        toml_datum_t cond = toml_string_in(stop_sec, "условие");
        toml_datum_t dist = toml_double_in(stop_sec, "дистанция_после_соосности_метров");
        toml_datum_t timeout = toml_double_in(stop_sec, "макс_таймаут_сек");
        if (cond.ok) { strncpy(ctx->stop_condition_type, cond.u.s, MAX_STR_LEN); free(cond.u.s); }
        if (dist.ok) ctx->stop_max_distance_after = dist.u.d;
        if (timeout.ok) ctx->stop_max_time = timeout.u.d;
    }

    // 5. Разбор каскада контрольных точек валидации аналитика
    toml_table_t* logic = toml_table_in(root, "логика_валидации");
    if (logic) {
        toml_array_t* levels = toml_array_in(logic, "уровни");
        if (levels) {
            int num_cps = toml_array_nelem(levels);
            for (int i = 0; i < num_cps && ctx->checkpoint_count < MAX_WAYPOINTS; i++) {
                toml_table_t* step = toml_table_at(levels, i);

                toml_datum_t t_check = toml_double_in(step, "время_контроля_сек");
                toml_datum_t status = toml_string_in(step, "ожидаемый_статус_ПО");
                toml_datum_t chk_id = toml_bool_in(step, "проверять_сохранение_id");
                toml_datum_t note = toml_string_in(step, "примечание"); // Явное чтение сути условия

                CheckpointConfig* cp = &ctx->checkpoints[ctx->checkpoint_count];

                if (t_check.ok) cp->check_time = t_check.u.d;

                if (status.ok) {
                    strncpy(cp->expected_status_str, status.u.s, MAX_STR_LEN - 1);
                    cp->expected_status_str[MAX_STR_LEN - 1] = '\0';
                    free(status.u.s);
                }

                if (chk_id.ok) cp->check_id_preservation = chk_id.u.b;

                if (note.ok) {
                    strncpy(cp->comment, note.u.s, (MAX_STR_LEN * 2) - 1);
                    cp->comment[(MAX_STR_LEN * 2) - 1] = '\0';
                    free(note.u.s);
                } else {
                    strcpy(cp->comment, "Суть условия не задана аналитиком в TOML");
                }

                ctx->checkpoint_count++;
            }
        }
    }

    toml_free(root);
    return true;
}

/* Публичный англоязычный интерфейс, соответствующий scenario_parser.h */
bool test_env_load_toml_config(const char* filepath, TestScenarioContext* out_ctx) {
    memset(out_ctx, 0, sizeof(TestScenarioContext));
    return load_toml_recursive(filepath, out_ctx, 0);
}
