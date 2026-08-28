#!/bin/bash

# Скрипт автоматического пакетного тестирования условного движка ПО
# Полное соответствие правилам двухъякорного автоконтроля из п. 6 AI.md

TESTS_DIR="./catalog"
REPORT_FILE="./сводный_отчет_тестирования.csv"
FULL_LOG_FILE="./полный_протокол_валидации.txt"
RUNNER="./testprj.exe"

if [ ! -f "$RUNNER" ]; then
    echo "[CRITICAL] Исполняемый файл $RUNNER не найден!"
    exit 1
fi

# Подготовка чистых файлов отчетов при старте пайплайна
echo "Идентификатор Сценария;Путь к Файлу;Статус Прохождения;Результат" > "$REPORT_FILE"

echo "=======================================================================" > "$FULL_LOG_FILE"
echo "        ОБЩИЙ ПРОТОКОЛ ПАКЕТНЫХ ИСПЫТАНИЙ ДВИЖКА СОПРОВОЖДЕНИЯ" >> "$FULL_LOG_FILE"
echo "=======================================================================" >> "$FULL_LOG_FILE"

echo "======================================================================="
echo "[PIPELINE] Запуск пакетного прогона разнородных сценариев..."
echo "======================================================================="

success_count=0
fail_count=0
total_count=0

# Итерационный Verify-Loop по всем TOML-файлам в корне папки catalog
for test_file in "$TESTS_DIR"/*.toml; do
    [ -e "$test_file" ] || continue
    
    ((total_count++))
    scenario_name=$(basename "$test_file" .toml)
    
    # Запись шапки в монолитный лог на диске
    echo "" >> "$FULL_LOG_FILE"
    echo ">>> ЗАПУСК СЦЕНАРИЯ [$total_count]: $scenario_name <<<" >> "$FULL_LOG_FILE"
    
    # Прямой вызов готового бинарника (Тест САМ запишет всю суть в файл, экран пуст)
    $RUNNER "$test_file" > /dev/null 2>&1
    exit_status=$?
    
    # Форматированный вывод строго по вашему эталону
    if [ "$exit_status" -eq 0 ]; then
        ((success_count++))
        echo "$scenario_name;$test_file;УСПЕХ;Доработано верно" >> "$REPORT_FILE"
        printf "[%d] Сценария %s:  УСПЕХ\n" "$total_count" "$scenario_name"
        echo "--> Итог по $scenario_name: УСПЕХ" >> "$FULL_LOG_FILE"
    else
        ((fail_count++))
        echo "$scenario_name;$test_file;БРАК;Ошибка автомата логики" >> "$REPORT_FILE"
        printf "[%d] Сценария %s: ❌ БРАК\n" "$total_count" "$scenario_name"
        echo "--> Итог по $scenario_name: ❌ БРАК (Ошибка ядра)" >> "$FULL_LOG_FILE"
    fi
    echo "-----------------------------------------------------------------------" >> "$FULL_LOG_FILE"
done

echo ""
echo "======================================================================="
echo "[ПАКЕТНЫЙ ТЕСТ ЗАВЕРШЕН]"
echo "Всего прогнано сценариев: $total_count"
echo "Успешно подтверждено:     $success_count"
echo "Забраковано ошибок ядра:  $fail_count"
echo "Сводный отчет сохранен в: $REPORT_FILE"
echo "Полный монолитный лог условий сохранен в: $FULL_LOG_FILE"
echo "======================================================================="

if [ "$fail_count" -gt 0 ]; then
    exit 1
else
    exit 0
fi
