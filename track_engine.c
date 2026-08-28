#include <string.h>
#include "tracking_interface.h"

void track_engine_process_measurement(const RadarMeasurement* measurement, 
                                      const char* target_type, 
                                      TuningBuffer* buf, 
                                      OutputTrackState* out_track) 
{
    // Шаг 0: Первичная ленивая инициализация контекста при старте нового сценария
    if (!buf->is_initialized) {
        buf->internal_interval_counter = 0;
        buf->current_track_status = TRACK_FREE;
        buf->track_id = 1001;
        buf->last_measurement_time = 0.0;
        buf->is_initialized = true;
    }

    // Сценарная адаптация параметров под тактический тип БПЛА из TOML
    double archive_lifetime_limit = 45.0; 
    if (strcmp(target_type, "БПЛА_МАЛАЯ_ЭПР") == 0) {
        archive_lifetime_limit = 30.0; // Малоразмерные дроны хранятся в памяти меньше
    }

    out_track->track_id = (buf->current_track_status != TRACK_FREE) ? buf->track_id : -1;

    // ШАГ 1: Межинтервальная селекция и завязка трассы
    if (buf->current_track_status == TRACK_FREE) {
        if (measurement->is_valid) {
            buf->internal_interval_counter++;
            buf->current_track_status = TRACK_INIT;
        }
        out_track->status = (TrackStatus)buf->current_track_status;
        return;
    }

    if (buf->current_track_status == TRACK_INIT) {
        if (measurement->is_valid) {
            buf->internal_interval_counter++;
            if (buf->internal_interval_counter >= 3) { // Подтверждение трассы по 3 интервалам
                buf->current_track_status = TRACK_CONFIRMED;
                buf->last_measurement_time = measurement->time;
            }
        } else {
            buf->internal_interval_counter = 0;
            buf->current_track_status = TRACK_FREE; // Сброс селекции при пропуске
        }
        out_track->status = (TrackStatus)buf->current_track_status;
        out_track->track_id = buf->track_id;
        return;
    }

    // ШАГ 2: Удержание трассы (Экстраполяция) при срыве пеленга на траверзе
    if (buf->current_track_status == TRACK_CONFIRMED) {
        if (measurement->is_valid) {
            buf->last_measurement_time = measurement->time;
            out_track->status = TRACK_CONFIRMED;
        } else {
            // Отметки пропали в слепой зоне. Переводим трассу в архив памяти
            buf->current_track_status = TRACK_EXTRAPOLATED;
            out_track->status = TRACK_EXTRAPOLATED;
        }
        out_track->track_id = buf->track_id;
        return;
    }

    // ШАГ 3: Восстановление архивных данных из памяти при выходе из соосности
    if (buf->current_track_status == TRACK_EXTRAPOLATED) {
        if (measurement->is_valid) {
            // Пеленг восстановлен! Вытаскиваем трассу из архива под СТАРЫМ ID цели
            buf->current_track_status = TRACK_CONFIRMED;
            out_track->status = TRACK_RESTORED; 
        } else {
            // Проверяем жесткий лимит удержания трассы в памяти без отметок
            if (measurement->time - buf->last_measurement_time > archive_lifetime_limit) {
                buf->current_track_status = TRACK_FREE; // Окончательное стирание трассы
                out_track->status = TRACK_FREE;
            } else {
                out_track->status = TRACK_EXTRAPOLATED; // Продолжаем удерживать
            }
        }
        out_track->track_id = (buf->current_track_status != TRACK_FREE) ? buf->track_id : -1;
        return;
    }
}
