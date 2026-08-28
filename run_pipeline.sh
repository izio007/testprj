#!/bin/bash

# Скрипт автоматического пакетного тестирования условного движка ПО
# Полное соответствие правилам двухъякорного автоконтроля из п. 6 AI.md

TESTS_DIR="./catalog"
REPORT_FILE="./сводный_отчет_тестирования.csv"
RUNNER="./testprj.exe"

# Проверка наличия собранного исполняемого файла
if [ ! -f "$RUNNER" ]; then
    echo "[CRITICAL] Исполняемый файл $RUNNER не найден в текущей директории!"
    exit 1
fi

# Инициализируем файл отчета с заголовками
echo "Идентификатор Сценария;Путь к Файлу;Статус Прохождения;Результат" > "$REPORT_FILE"

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
    
    echo ""
    echo ">>> ЗАПУСК СЦЕНАРИЯ: $scenario_name ($test_file) <<<"
    
    # ИСПРАВЛЕНИЕ: Убрано глушение (> /dev/null 2>&1). Вывод идет напрямую на экран!
    $RUNNER "$test_file"
    exit_status=$?
    
    # Анализируем Си-отмах выполнения контракта теста
    if [ "$exit_status" -eq 0 ]; then
        ((success_count++))
        echo "$scenario_name;$test_file;УСПЕХ;Доработано верно" >> "$REPORT_FILE"
        echo "--> Итог по $scenario_name:  УСПЕХ"
    else
        ((fail_count++))
        echo "$scenario_name;$test_file;БРАК;Ошибка автомата логики" >> "$REPORT_FILE"
        echo "--> Итог по $scenario_name: ❌ БРАК (Ошибка ядра)"
    fi
    echo "-----------------------------------------------------------------------"
done

echo ""
echo "======================================================================="
echo "[ПАКЕТНЫЙ ТЕСТ ЗАВЕРШЕН]"
echo "Всего прогнано сценариев: $total_count"
echo "Успешно подтверждено:     $success_count"
echo "Забраковано ошибок ядра:  $fail_count"
echo "Сводный отчет сохранен в: $REPORT_FILE"
echo "======================================================================="

if [ "$fail_count" -gt 0 ]; then
    exit 1
else
    exit 0
fi
