#!/bin/bash
# =====================================================
# backup.sh
# Backs up all parking data files to backup.txt
# Called from C program using fork()+execl()
# =====================================================

DATA_DIR="data"
BACKUP_FILE="$DATA_DIR/backup.txt"
TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")

echo ""
echo  "\033[1;33m  Starting backup...\033[0m"

# Create backup using shell file operations
{
    echo "=========================================="
    echo "  CAR PARKING SYSTEM - DATA BACKUP"
    echo "  Timestamp: $TIMESTAMP"
    echo "=========================================="
    echo ""

    echo "--- ACTIVE VEHICLES (drivers.txt) ---"
    if [ -f "$DATA_DIR/drivers.txt" ]; then
        cat "$DATA_DIR/drivers.txt"
    else
        echo "  (no records)"
    fi

    echo ""
    echo "--- BILLING RECORDS (billing.txt) ---"
    if [ -f "$DATA_DIR/billing.txt" ]; then
        cat "$DATA_DIR/billing.txt"
    else
        echo "  (no records)"
    fi

    echo ""
    echo "--- TRANSACTION LOG (transactions.txt) ---"
    if [ -f "$DATA_DIR/transactions.txt" ]; then
        cat "$DATA_DIR/transactions.txt"
    else
        echo "  (no records)"
    fi

    echo ""
    echo "=========================================="
    echo "  END OF BACKUP"
    echo "=========================================="

} > "$BACKUP_FILE"

if [ $? -eq 0 ]; then
    echo  "\033[1;32m  Backup saved to: $BACKUP_FILE\033[0m"
    # Show file size
    SIZE=$(wc -c < "$BACKUP_FILE")
    echo  "  File size: ${SIZE} bytes"
else
    echo "\033[1;31m  ERROR... Backup failed!\033[0m"
    exit 1
fi

exit 0
