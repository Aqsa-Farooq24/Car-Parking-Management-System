#!/bin/bash
# =====================================================
# run.sh -- Compile & Run Car Parking Management System
# =====================================================

mkdir -p data scripts
chmod +x scripts/show_report.sh scripts/backup.sh 2>/dev/null

echo -e "\033[1;33m  Compiling parking.c...\033[0m"
gcc -Wall -o parking_system parking.c 2>&1

if [ $? -ne 0 ]; then
    echo -e "\033[1;31m  Compilation failed!\033[0m"
    exit 1
fi

echo -e "\033[1;32m  Compiled successfully. Starting...\033[0m"
sleep 1
./parking_system
