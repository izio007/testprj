#ifndef TRACKING_INTERFACE_H
#define TRACKING_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_OBJECTS 64      /* Предельное число объектов в сценарии */
#define MAX_WAYPOINTS 32    /* Максимальное число путевых точек маршрута */
#define MAX_STR_LEN 64      /* Длина строковых буферов под типы TOML */

typedef struct {
    double x;               /* Координата X (метры) */
    double y;               /* Координата Y (метры) */
    double z;               /* Высота Z (метры) */
} Vector3D;

typedef struct {
    double AtA;             /* Информационная матрица Фишера */
    double H;               /* Вектор-строка Якобиана измерений */
    double last_predict_pos;/* Экстраполяционный буфер прогноза трассы */
    
    int internal_interval_counter; /* Такты накопления отметок для селекции */
    int current_track_status;      /* Состояние условного движка сопровождения */
    int track_id;                  /* Номер подтвержденной трассы */
    double last_measurement_time;  /* Время последней валидной отметки */
    bool is_initialized;           /* Флаг первичной инициализации ОЗУ-буфера */
} TuningBuffer;

typedef struct {
    double time;            /* Время симуляции (секунды) */
    int radar_id;           /* ID выдавшего отметку пеленгатора */
    bool is_valid;          /* Признак валидности (false - слепая зона) */
    double measured_az;     /* Азимут в радианах [-pi; pi] от оси OX */
    double measured_el;     /* Угол места в радианах [-pi/2; pi/2] */
} RadarMeasurement;

typedef enum {
    TRACK_FREE,             /* Трасса свободна */
    TRACK_INIT,             /* Завязка трассы межинтервальной селекцией */
    TRACK_CONFIRMED,        /* Устойчивое автосопровождение */
    TRACK_EXTRAPOLATED,     /* Удержание трассы в архиве памяти (прогноз) */
    TRACK_RESTORED          /* Восстановление архива под старым ID */
} TrackStatus;

typedef struct {
    int track_id;           /* Сквозной номер сопровождаемой трассы */
    TrackStatus status;     /* Логический статус автомата сопровождения */
    Vector3D estimated_pos; /* Сглаженная оценка координат цели движком ПО */
} OutputTrackState;

typedef struct {
    Vector3D pos;           /* Координаты путевой точки (метры) */
    double speed_ms;        /* Заданная скорость пролета (м/с) */
} Waypoint;

typedef struct {
    int id;                                 /* Сценарный номер объекта из TOML */
    char type[MAX_STR_LEN];                 /* Тип/особенности цели ("БПЛА_МАЛАЯ_ЭПР") */
    char movement_mode[MAX_STR_LEN];         /* Режим полета ("СТАТИКА", "ПО_ТОЧКАМ") */
    bool is_route_looped;                   /* Флаг циклического барражирования */
    
    Waypoint route[MAX_WAYPOINTS];          /* Плоский массив точек маршрута */
    size_t waypoint_count;                  /* Количество путевых точек в маршруте */
    size_t current_waypoint_index;          /* Индекс текущей путевой точки цели */
    
    Vector3D pos_start;                     /* Исправление: Начальная позиция для сброса */
    Vector3D pos_current;                   /* Текущая координата объекта */
    Vector3D vel_current;                   /* Текущий вектор скорости объекта */
    bool is_emitting;                       /* Наличие излучения цели */
} SimObject;

void track_engine_process_measurement(const RadarMeasurement* measurement, 
                                      const char* target_type, 
                                      TuningBuffer* buf, 
                                      OutputTrackState* out_track);

#endif /* TRACKING_INTERFACE_H */
