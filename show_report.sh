#!/bin/bash
# show_report.sh
# Displays the generated parking report
# Used via execl() from the C program (OS concept)

REPORT_FILE="${1:-data/reports.txt}"

echo ""
echo  "\033[1;35m  Reading report via shell script...\033[0m"
echo ""

if [ ! -f "$REPORT_FILE" ]; then
    echo  "\033[1;31m  ERROR... Report file not found! $REPORT_FILE\033[0m"
    exit 1
fi

# Display using cat 
cat "$REPORT_FILE"

echo ""
echo  "\033[1;32m  Report displayed successfully.\033[0m"
exit 0
