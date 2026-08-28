#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "tracking_interface.h"
#include "scenario_parser.h"
#include "sim_core.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Вспомогательный генератор шума Гаусса с фиксированным seed (п. 3 AI.md) */
static double generate_gauss_noise(double sigma) {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    if (u1 < 1e-9) u1 = 1e-9;
    return sigma * sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* Вычислитель положения цели на маршруте барражирования по путевым точкам */
static void update_object_kinematics(SimObject* obj, double dt) {
    if (strcmp(obj->movement_mode, "СТАТИКА") == 0 || obj->waypoint_count < 2) {
        obj->vel_current = (Vector3D){0.0, 0.0, 0.0};
        return;
    }

    size_t curr_idx = obj->current_waypoint_index;
    size_t next_idx = curr_idx + 1;

    if (next_idx >= obj->waypoint_count) {
        if (obj->is_route_looped) {
            next_idx = 0;
        } else {
            obj->vel_current = (Vector3D){0.0, 0.0, 0.0};
            return;
        }
    }

    Waypoint* p_curr = &obj->route[curr_idx];
    Waypoint* p_next = &obj->route[next_idx];

    double dx = p_next->pos.x - obj->pos_current.x;
    double dy = p_next->pos.y - obj->pos_current.y;
    double dz = p_next->pos.z - obj->pos_current.z;
    double dist_to_next = sqrt(dx*dx + dy*dy + dz*dz);

    if (dist_to_next < 10.0) {
        obj->current_waypoint_index = next_idx;
        return;
    }

    obj->vel_current.x = (dx / dist_to_next) * p_curr->speed_ms;
    obj->vel_current.y = (dy / dist_to_next) * p_curr->speed_ms;
    obj->vel_current.z = (dz / dist_to_next) * p_curr->speed_ms;

    obj->pos_current.x += obj->vel_current.x * dt;
    obj->pos_current.y += obj->vel_current.y * dt;
    obj->pos_current.z += obj->vel_current.z * dt;
}

/* Главный управляющий конвейер ядра симулятора */
int run_simulator_loop(TestScenarioContext* ctx, TuningBuffer* buffer_OZU) {
    printf("[SIM CORE] Initializing pipeline for scenario: %s\n", ctx->scenario_id);

    srand(ctx->random_seed);
    double t = 0.0;
    bool stop_required = false;

    SimObject* radar = &ctx->objects[0];
    SimObject* target = &ctx->objects[1];

    int saved_track_id = -1;
    bool checkpoint_triggered[MAX_WAYPOINTS];

    /* Принудительный сброс массива триггеров */
    for (size_t i = 0; i < MAX_WAYPOINTS; i++) {
        checkpoint_triggered[i] = false;
    }

    /* Инициализация статусов чекпоинтов в контексте */
    for (size_t i = 0; i < ctx->checkpoint_count; i++) {
        ctx->checkpoints[i].is_passed = false;
        strncpy(ctx->checkpoints[i].actual_status_str, "НЕ ДОСТИГНУТ", MAX_STR_LEN - 1);
        ctx->checkpoints[i].actual_status_str[MAX_STR_LEN - 1] = '\0';
    }

    while (!stop_required) {
        t += ctx->time_step;

        /* [Шаг 1: Геометрия] */
        update_object_kinematics(target, ctx->time_step);
        update_object_kinematics(radar, ctx->time_step);

        /* [Шаг 2: Фильтры] */
        double dx = target->pos_current.x - radar->pos_current.x;
        double dy = target->pos_current.y - radar->pos_current.y;
        double dist = sqrt(dx*dx + dy*dy);

        double true_az = atan2(dy, dx);
        double cos_angle = fabs(target->pos_current.y / dist);
        bool is_alignment_zone = (cos_angle < sin(2.0 * M_PI / 180.0));

        RadarMeasurement measurement;
        measurement.time = t;
        measurement.radar_id = radar->id;
        measurement.measured_el = 0.0;

        if (is_alignment_zone) {
            measurement.is_valid = false;
            measurement.measured_az = 0.0;
        } else {
            measurement.is_valid = true;
            measurement.measured_az = true_az + generate_gauss_noise(0.002);
        }

        /* [Шаг 3: Математика] */
        OutputTrackState current_track = { .track_id = -1, .status = TRACK_FREE };
        track_engine_process_measurement(&measurement, target->type, buffer_OZU, &current_track);

        if (current_track.status == TRACK_CONFIRMED && saved_track_id == -1) {
            saved_track_id = current_track.track_id;
        }

        /* [Шаг 4: Анализ] — Хронологическая проверка условий на чекпоинтах по временному створу такта */
        for (size_t i = 0; i < ctx->checkpoint_count; i++) {
            CheckpointConfig* cp = &ctx->checkpoints[i];
            
            if (!checkpoint_triggered[i] && (t >= cp->check_time) && (t < (cp->check_time + ctx->time_step))) {

                TrackStatus expected_status = TRACK_FREE;
                if (strcmp(cp->expected_status_str, "TRACK_INIT") == 0) expected_status = TRACK_INIT;
                else if (strcmp(cp->expected_status_str, "TRACK_CONFIRMED") == 0) expected_status = TRACK_CONFIRMED;
                else if (strcmp(cp->expected_status_str, "TRACK_EXTRAPOLATED") == 0) expected_status = TRACK_EXTRAPOLATED;
                else if (strcmp(cp->expected_status_str, "TRACK_RESTORED") == 0) expected_status = TRACK_RESTORED;

                bool status_ok = (current_track.status == expected_status);
                bool id_ok = (!cp->check_id_preservation || (current_track.track_id == saved_track_id));

                cp->is_passed = (status_ok && id_ok);

                const char* status_ru = "НЕИЗВЕСТНО";
                switch (current_track.status) {
                    case TRACK_FREE:         status_ru = "СВОБОДНА/СБРОС"; break;
                    case TRACK_INIT:         status_ru = "СЕЛЕКЦИЯ"; break;
                    case TRACK_CONFIRMED:    status_ru = "СОПРОВОЖДЕНИЕ"; break;
                    case TRACK_EXTRAPOLATED: status_ru = "ЭКСТРАПОЛЯЦИЯ (АРХИВ)"; break;
                    case TRACK_RESTORED:     status_ru = "ВОССТАНОВЛЕНА"; break;
                }

                strncpy(cp->actual_status_str, status_ru, MAX_STR_LEN - 1);
                cp->actual_status_str[MAX_STR_LEN - 1] = '\0';

                checkpoint_triggered[i] = true;
            }
        }

        /* КРИТЕРИЙ ОСТАНОВА */
        if (strcmp(ctx->stop_condition_type, "ВЫХОД_ИЗ_СТВОРА") == 0) {
            if (target->pos_current.y > ctx->stop_max_distance_after) {
                stop_required = true;
            }
        }
        if (t >= ctx->stop_max_time) {
            stop_required = true;
        }
    }

    /* ФОРМИРОВАНИЕ СУДЕЙСКОГО ОТЧЕТА В КОНСОЛЬ ДЛЯ АНАЛИТИКА */
    printf("\n=======================================================================\n");
    printf("ПРОТОКОЛ ВЕРИФИКАЦИИ УСЛОВИЙ ДЛЯ СЦЕНАРИЯ: %s\n", ctx->scenario_id);
    printf("=======================================================================\n");

    bool final_success = true;
    for (size_t i = 0; i < ctx->checkpoint_count; i++) {
        CheckpointConfig* cp = &ctx->checkpoints[i];

        printf("Точка N%zu [%.1f сек] | Суть: %s\n", i + 1, cp->check_time, cp->comment);
        printf("  -> Ожидалось:  %s\n", cp->expected_status_str);
        printf("  -> Фактически: %s\n", cp->actual_status_str);

        if (cp->is_passed) {
            printf("  -> Вердикт точки:  [ОТРАБОТАНО ВЕРНО]\n");
        } else {
            printf("  -> Вердикт точки: ❌ [НАРУШЕНО]\n");
            final_success = false;
        }
        printf("-----------------------------------------------------------------------\n");
    }

    return final_success ? 0 : 1;
}
