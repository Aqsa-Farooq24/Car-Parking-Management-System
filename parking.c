/*
 * CAR PARKING MANAGEMENT SYSTEM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define MAX_SLOTS       20
#define MAX_QUEUE       50
#define MAX_NAME        50
#define MAX_CNIC        16
#define MAX_PLATE       12
#define MAX_LINE       256
#define NORMAL           0
#define VIP              1
#define RESERVED         2
#define RATE_NORMAL     50
#define RATE_VIP       120
#define RATE_RESERVED  180

#define FILE_CREDENTIALS  "data/credentials.txt"
#define FILE_DRIVERS      "data/drivers.txt"
#define FILE_BILLING      "data/billing.txt"
#define FILE_REPORT       "data/reports.txt"
#define FILE_BACKUP       "data/backup.txt"
#define FILE_TRANSACTIONS "data/transactions.txt"
#define FILE_SLOTS        "data/slots.txt"

/* Colors */
#define CYAN    "\033[0;36m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[0;33m"
#define RED     "\033[0;31m"
#define WHITE   "\033[0;37m"
#define RESET   "\033[0m"

/* Structures */
typedef struct { int slot_id, slot_type, is_occupied; } ParkingSlot;

typedef struct {
    char   name[MAX_NAME], cnic[MAX_CNIC], plate[MAX_PLATE];
    int    slot_id, slot_type;
    time_t entry_time;
    char   entry_str[30];
} Vehicle;

typedef struct {
    Vehicle vehicles[MAX_QUEUE];
    int front, rear, count;
} FCFSQueue;

ParkingSlot slots[MAX_SLOTS];
FCFSQueue   queue;

/* ── UI ── */
void clear_screen() { write(STDOUT_FILENO, "\033[2J\033[H", 7); }

void print_banner() {
    clear_screen();
    printf(CYAN "  ================================================\n" RESET);
    printf(CYAN "         CAR PARKING MANAGEMENT SYSTEM\n" RESET);
    printf(CYAN "  ================================================\n\n" RESET);
}

void hline() { printf("  ------------------------------------------------\n"); }

void iprompt(const char *lbl) {
    printf(YELLOW "  %-20s : " RESET, lbl);
}

void msg_ok(const char *t)   { printf(GREEN  "  [OK]  %s\n" RESET, t); }
void msg_err(const char *t)  { printf(RED    "  [ERR] %s\n" RESET, t); }
void msg_warn(const char *t) { printf(YELLOW "  [!]   %s\n" RESET, t); }

void press_enter() {
    printf("  Press ENTER to continue...");
    getchar(); getchar();
}

/* ── File I/O (system calls) ── */
int write_line_to_file(const char *fp, const char *line) {
    int fd = open(fp, O_WRONLY|O_CREAT|O_APPEND, 0644);
    if (fd < 0) { perror("open()"); return -1; }
    ssize_t b = write(fd, line, strlen(line));
    close(fd);
    return (b < 0) ? -1 : 0;
}

int read_file_contents(const char *fp, char *buf, size_t sz) {
    int fd = open(fp, O_RDONLY); if (fd < 0) return -1;
    ssize_t b = read(fd, buf, sz-1);
    if (b >= 0) buf[b] = '\0';
    close(fd); return (int)b;
}

int overwrite_file(const char *fp, const char *content) {
    int fd = open(fp, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, content, strlen(content));
    close(fd); return 0;
}

/* ── Save slot status to slots.txt ── */
void save_slots_file() {
    const char *types[] = {"Normal","VIP","Reserved"};
    int rates[] = {RATE_NORMAL, RATE_VIP, RATE_RESERVED};
    char buf[4096]; memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf),
             "=== PARKING SLOT STATUS ===\n"
             "Slot  | Type      | Rate/hr | Status\n"
             "------------------------------------------------\n");
    for (int i = 0; i < MAX_SLOTS; i++) {
        char line[128]; int t = slots[i].slot_type;
        snprintf(line, sizeof(line), "%-6d| %-10s| PKR %-4d| %s\n",
                 slots[i].slot_id, types[t], rates[t],
                 slots[i].is_occupied ? "OCCUPIED" : "FREE");
        strncat(buf, line, sizeof(buf)-strlen(buf)-1);
    }
    overwrite_file(FILE_SLOTS, buf);
}

void init_data_dir() {
    mkdir("data", 0755);
    int fd = open(FILE_CREDENTIALS, O_RDONLY);
    if (fd < 0) write_line_to_file(FILE_CREDENTIALS, "admin:admin123\n");
    else close(fd);
}
void init_slots() {

    for (int i = 0; i < MAX_SLOTS; i++) {
        slots[i].slot_id   = i + 1;
        slots[i].is_occupied = 0;
        slots[i].slot_type = (i < 10) ? NORMAL : (i < 15) ? VIP : RESERVED;
    }

    int fd = open(FILE_DRIVERS, O_RDONLY);
    if (fd < 0) { save_slots_file(); return; }
    char whole[8192]; memset(whole, 0, sizeof(whole));
    ssize_t n = read(fd, whole, sizeof(whole)-1); close(fd);
    if (n <= 0) { save_slots_file(); return; }

    char *ptr = whole;
    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        char nm[MAX_NAME], cn[MAX_CNIC], pl[MAX_PLATE];
        int sid, st; char es[30];
        if (sscanf(ptr, "%49[^|]|%15[^|]|%11[^|]|%d|%d|%29[^\n]",
                   nm, cn, pl, &sid, &st, es) == 6)
            if (sid >= 1 && sid <= MAX_SLOTS)
                slots[sid-1].is_occupied = 1;
        ptr = end + 1;
    }
    save_slots_file();
}

/* ── FCFS Queue ── */
void queue_init() { queue.front = 0; queue.rear = -1; queue.count = 0; }

int queue_enqueue(Vehicle *v) {
    if (queue.count >= MAX_QUEUE) return -1;
    queue.rear = (queue.rear + 1) % MAX_QUEUE;
    queue.vehicles[queue.rear] = *v;
    queue.count++;
    return 0;
}

Vehicle *queue_peek() { return queue.count == 0 ? NULL : &queue.vehicles[queue.front]; }

void queue_dequeue() {
    if (queue.count == 0) return;
    queue.front = (queue.front + 1) % MAX_QUEUE;
    queue.count--;
}

int find_slot(int type) {
    for (int i = 0; i < MAX_SLOTS; i++)
        if (slots[i].slot_type == type && !slots[i].is_occupied)
            return slots[i].slot_id;
    return -1;
}

int process_one_queued_vehicle() {
    Vehicle *next = queue_peek();
    if (!next) return 0;

    int sid = find_slot(next->slot_type);
    if (sid < 0) return 0;

    slots[sid-1].is_occupied = 1;
    next->slot_id    = sid;
    next->entry_time = time(NULL);
    strftime(next->entry_str, sizeof(next->entry_str), "%Y-%m-%d %H:%M:%S",
             localtime(&next->entry_time));

    char record[MAX_LINE];
    snprintf(record, sizeof(record), "%s|%s|%s|%d|%d|%s\n",
             next->name, next->cnic, next->plate,
             next->slot_id, next->slot_type, next->entry_str);
    write_line_to_file(FILE_DRIVERS, record);

    const char *st = next->slot_type == NORMAL ? "Normal" :
                     (next->slot_type == VIP   ? "VIP"    : "Reserved");
    char txn[MAX_LINE];
    snprintf(txn, sizeof(txn),
             "ENTRY | %-20s | %-10s | Slot:%-4d (%-8s) | %s\n",
             next->name, next->plate, next->slot_id, st, next->entry_str);
    write_line_to_file(FILE_TRANSACTIONS, txn);

    printf(GREEN "  [FCFS] '%s' (Plate: %s) assigned Slot %d (%s)\n" RESET,
           next->name, next->plate, next->slot_id, st);

    queue_dequeue();
    save_slots_file();
    return 1;
}

void process_fcfs_queue_after_exit() {
    int processed = 0;
    while (queue.count > 0 && process_one_queued_vehicle())
        processed++;
    if (processed > 0)
        printf(GREEN "  [FCFS] %d queued vehicle(s) assigned slots.\n" RESET, processed);
}

/* ── Login ── */
int authenticate_admin() {
    char uname[50], pass[50], stored[MAX_LINE];

    hline();
    printf("  LOGIN\n");
    hline();

    iprompt("Username"); scanf("%49s", uname);
    iprompt("Password");
    system("stty -echo"); scanf("%49s", pass); system("stty echo");
    printf("\n");

    int fd = open(FILE_CREDENTIALS, O_RDONLY);
    if (fd < 0) { msg_err("Credentials file missing!"); return 0; }
    ssize_t n = read(fd, stored, sizeof(stored)-1); close(fd);
    if (n <= 0) return 0;
    stored[n] = '\0';

    char su[50], sp[50], *ptr = stored;
    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        if (sscanf(ptr, "%49[^:]:%49s", su, sp) == 2)
            if (strcmp(su, uname) == 0 && strcmp(sp, pass) == 0) {
                msg_ok("Login successful!"); sleep(1); return 1;
            }
        ptr = end + 1;
    }
    msg_err("Invalid credentials. Access denied.");
    sleep(1); return 0;
}

void vehicle_entry(); void vehicle_exit(); void billing_process(const char *plate);
void parking_status(); void view_active_vehicles(); void search_vehicle();
void generate_report(); void revenue_summary(); void view_transactions(); void backup_data();

/* ── Admin Menu ── */
void admin_menu() {
    int choice;
    while (1) {
        print_banner();
        printf(CYAN "  MENU\n" RESET);
        hline();
        printf("  1. Vehicle Entry\n");
        printf("  2. Vehicle Exit & Billing\n");
        printf("  3. Parking Slot Status\n");
        printf("  4. Active Vehicles\n");
        printf("  5. Search Vehicle\n");
        printf("  6. Generate Report\n");
        printf("  7. Revenue Summary\n");
        printf("  8. Transaction History\n");
        printf("  9. Backup Data\n");
        printf("  0. Exit\n");
        hline();
        printf(YELLOW "  Choice: " RESET);
        char mcbuf[32];
        if (scanf("%31s", mcbuf) != 1) { choice = -1; }
        else {
            int mvalid = 1;
            for (int i = 0; mcbuf[i]; i++)
                if (mcbuf[i] < '0' || mcbuf[i] > '9') { mvalid = 0; break; }
            if (!mvalid) { msg_err("Enter a number (0-9)."); sleep(1); continue; }
            choice = atoi(mcbuf);
        }

        switch (choice) {
            case 1: vehicle_entry();        break;
            case 2: vehicle_exit();         break;
            case 3: parking_status();       break;
            case 4: view_active_vehicles(); break;
            case 5: search_vehicle();       break;
            case 6: generate_report();      break;
            case 7: revenue_summary();      break;
            case 8: view_transactions();    break;
            case 9: backup_data();          break;
            case 0:
                printf(CYAN "\n  Exiting... Thank you for using CarParking Management System!\n\n" RESET);
                exit(0);
            default:
                msg_err("Invalid choice. Enter 0-9."); sleep(1);
        }
    }
}

/* ── 1. Vehicle Entry ── */
void vehicle_entry() {
    init_slots();   /* sync slots from file before entry */

    print_banner();
    printf(CYAN "  VEHICLE ENTRY\n" RESET);
    hline();

    Vehicle v; int stc;
    char input_buf[64];

    iprompt("Driver Name");
    getchar();
    fgets(v.name, MAX_NAME, stdin);
    v.name[strcspn(v.name, "\n")] = '\0';
    if (strlen(v.name) == 0) {
        msg_err("Name cannot be empty."); press_enter(); return;
    }

    while (1) {
        iprompt("CNIC");
        scanf("%63s", input_buf);
        int clen = strlen(input_buf);
        if (clen > 15) {
            msg_err("CNIC too long! Max 15 characters (e.g. 42201-1234567-1).");
            continue;
        }
        int valid = 1;
        for (int i = 0; i < clen; i++) {
            if (input_buf[i] != '-' && (input_buf[i] < '0' || input_buf[i] > '9'))
                { valid = 0; break; }
        }
        if (!valid) {
            msg_err("CNIC must contain digits and dashes only.");
            continue;
        }
        strncpy(v.cnic, input_buf, MAX_CNIC-1);
        v.cnic[MAX_CNIC-1] = '\0';
        break;
    }

    iprompt("Car Plate No");
    scanf("%11s", v.plate);
    if (strlen(v.plate) == 0) {
        msg_err("Plate number cannot be empty."); press_enter(); return;
    }

    printf("\n");
    hline();
    printf("  Slot Type:\n");
    printf("  [0] Normal    PKR %d/hr  (Slots  1-10)\n", RATE_NORMAL);
    printf("  [1] VIP       PKR %d/hr (Slots 11-15)\n",  RATE_VIP);
    printf("  [2] Reserved  PKR %d/hr (Slots 16-20)\n",  RATE_RESERVED);
    hline();
    while (1) {
        iprompt("Choice");
        if (scanf("%63s", input_buf) != 1) { stc = -1; break; }
        int isnum = 1;
        for (int i = 0; input_buf[i]; i++)
            if (input_buf[i] < '0' || input_buf[i] > '9') { isnum = 0; break; }
        if (!isnum) {
            msg_err("Invalid input! Enter 0, 1, or 2.");
            continue;
        }
        stc = atoi(input_buf);
        if (stc < 0 || stc > 2) {
            msg_err("Invalid choice! Enter 0, 1, or 2.");
            continue;
        }
        break;
    }
    v.slot_type = stc;

    int sid = find_slot(v.slot_type);

    if (sid < 0) {
        if (v.slot_type == NORMAL) {
            printf(YELLOW "  [!] No Normal slots available.\n" RESET);
            printf(YELLOW "      Vehicle added to FCFS waiting queue.\n" RESET);
            if (queue_enqueue(&v) < 0)
                msg_err("FCFS Queue also full! Cannot register vehicle.");
            press_enter();
            return;
        } else {
            const char *req = (v.slot_type == VIP) ? "VIP" : "Reserved";
            printf(YELLOW "  [!] No %s slots available.\n" RESET, req);
            printf(YELLOW "      You can take a Normal slot (PKR %d/hr) instead,\n" RESET, RATE_NORMAL);
            printf(YELLOW "      or wait in the FCFS queue for a %s slot.\n" RESET, req);
            printf("\n  [1] Take a Normal slot now\n");
            printf("  [2] Wait in FCFS queue for %s slot\n", req);
            hline();
            iprompt("Choice");
            int fc; scanf("%d", &fc);

            if (fc == 1) {
                sid = find_slot(NORMAL);
                if (sid < 0) {
                    printf(YELLOW "  [!] Normal slots are also full.\n" RESET);
                    printf(YELLOW "      Vehicle added to FCFS queue for %s slot.\n" RESET, req);
                    if (queue_enqueue(&v) < 0)
                        msg_err("FCFS Queue also full! Cannot register vehicle.");
                    press_enter();
                    return;
                }
                v.slot_type = NORMAL;
            } else {
                printf(YELLOW "  Vehicle added to FCFS queue for %s slot.\n" RESET, req);
                if (queue_enqueue(&v) < 0)
                    msg_err("FCFS Queue also full! Cannot register vehicle.");
                press_enter();
                return;
            }
        }
    }

    slots[sid-1].is_occupied = 1;
    v.slot_id    = sid;
    v.entry_time = time(NULL);
    strftime(v.entry_str, sizeof(v.entry_str), "%Y-%m-%d %H:%M:%S",
             localtime(&v.entry_time));

    char record[MAX_LINE];
    snprintf(record, sizeof(record), "%s|%s|%s|%d|%d|%s\n",
             v.name, v.cnic, v.plate,
             v.slot_id, v.slot_type, v.entry_str);
    write_line_to_file(FILE_DRIVERS, record);

    const char *st = v.slot_type == NORMAL ? "Normal" :
                     (v.slot_type == VIP   ? "VIP"    : "Reserved");
    char txn[MAX_LINE];
    snprintf(txn, sizeof(txn), "ENTRY | %-20s | %-10s | Slot:%-4d (%-8s) | %s\n",
             v.name, v.plate, v.slot_id, st, v.entry_str);
    write_line_to_file(FILE_TRANSACTIONS, txn);

    save_slots_file();

    msg_ok("Vehicle registered successfully!");
    printf("\n");
    hline();
    printf("  Name       : %s\n", v.name);
    printf("  CNIC       : %s\n", v.cnic);
    printf("  Plate No   : %s\n", v.plate);
    printf("  Slot No    : %d (%s)\n", v.slot_id, st);
    printf("  Entry Time : %s\n", v.entry_str);
    hline();
    press_enter();
}

/* ── billing_process: runs in child process ── */
void billing_process(const char *plate) {
    char buf[8192]; memset(buf, 0, sizeof(buf));
    int fd = open(FILE_DRIVERS, O_RDONLY);
    if (fd < 0) { msg_err("Cannot open drivers file."); exit(1); }
    ssize_t n = read(fd, buf, sizeof(buf)-1); close(fd);
    if (n <= 0) { msg_err("No records found."); exit(1); }

    char nm[MAX_NAME], cn[MAX_CNIC], rpl[MAX_PLATE];
    int sid, st; char es[30];
    int found = 0;
    char new_buf[8192]; memset(new_buf, 0, sizeof(new_buf));

    char *ptr = buf;
    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        if (sscanf(ptr, "%49[^|]|%15[^|]|%11[^|]|%d|%d|%29[^\n]",
                   nm, cn, rpl, &sid, &st, es) == 6) {
            if (strcmp(rpl, plate) == 0 && !found) {
                found = 1;

                struct tm etm; memset(&etm, 0, sizeof(etm));
                sscanf(es, "%d-%d-%d %d:%d:%d",
                       &etm.tm_year, &etm.tm_mon, &etm.tm_mday,
                       &etm.tm_hour, &etm.tm_min, &etm.tm_sec);
                etm.tm_year -= 1900; etm.tm_mon -= 1; etm.tm_isdst = -1;
                time_t et  = mktime(&etm);
                time_t xt  = time(NULL);
                double hours = difftime(xt, et) / 3600.0;
                if (hours < 0.0167) hours = 0.0167;
                int rate = (st == VIP) ? RATE_VIP :
                           (st == RESERVED) ? RATE_RESERVED : RATE_NORMAL;
                double bill = hours * rate;
                char xs[30];
                strftime(xs, sizeof(xs), "%Y-%m-%d %H:%M:%S", localtime(&xt));
                const char *stn = (st == NORMAL) ? "Normal" :
                                  (st == VIP)    ? "VIP"    : "Reserved";

                printf("\n");
                hline();
                printf("  BILLING RECEIPT\n");
                hline();
                printf("  Name       : %s\n", nm);
                printf("  CNIC       : %s\n", cn);
                printf("  Plate No   : %s\n", rpl);
                printf("  Slot       : %d (%s)\n", sid, stn);
                printf("  Entry Time : %s\n", es);
                printf("  Exit Time  : %s\n", xs);
                printf("  Duration   : %.4f hours\n", hours);
                printf("  Rate       : PKR %d/hr\n", rate);
                hline();
                printf(GREEN "  Total Bill : PKR %.2f\n" RESET, bill);
                hline();

                char br[MAX_LINE];
                snprintf(br, sizeof(br), "%s|%s|%s|%d|%s|%s|%s|%.2f\n",
                         nm, cn, rpl, sid, stn, es, xs, bill);
                write_line_to_file(FILE_BILLING, br);
                char txn[MAX_LINE];
                snprintf(txn, sizeof(txn),
                         "EXIT  | %-20s | %-10s | Slot:%-4d (%-8s) | In:%s Out:%s | PKR %.2f\n",
                         nm, rpl, sid, stn, es, xs, bill);
                write_line_to_file(FILE_TRANSACTIONS, txn);

            } else {
                char line[MAX_LINE];
                snprintf(line, sizeof(line), "%s|%s|%s|%d|%d|%s\n",
                         nm, cn, rpl, sid, st, es);
                strncat(new_buf, line, sizeof(new_buf)-strlen(new_buf)-1);
            }
        }
        ptr = end + 1;
    }

    if (!found) { msg_err("Plate not found in active vehicles."); exit(1); }
    overwrite_file(FILE_DRIVERS, new_buf);

    exit(0);
}

/* ── 2. Vehicle Exit ── */
void vehicle_exit() {
    print_banner();
    printf(CYAN "  VEHICLE EXIT & BILLING\n" RESET);
    hline();
    char plate[MAX_PLATE];
    iprompt("Car Plate No"); getchar(); scanf("%11s", plate);

    pid_t pid = fork();
    if (pid < 0) { perror("fork()"); return; }
    else if (pid == 0) {
        billing_process(plate);  
    } else {
        int status; wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    
            init_slots();
            process_fcfs_queue_after_exit();
            save_slots_file();
            msg_ok("Vehicle exited. Slot free. Records updated.");
        }
    }
    press_enter();
}

/* ── 3. Parking Status ── */
void parking_status() {
    init_slots();   
    print_banner();
    printf(CYAN "  PARKING SLOT STATUS\n" RESET);
    hline();

    int occ = 0;
    for (int i = 0; i < MAX_SLOTS; i++) if (slots[i].is_occupied) occ++;

    printf("  Total     : %d\n", MAX_SLOTS);
    printf("  Available : %d\n", MAX_SLOTS - occ);
    printf("  Occupied  : %d\n\n", occ);

    if (queue.count > 0)
        printf(YELLOW "  FCFS Queue: %d vehicle(s) waiting for a slot.\n\n" RESET, queue.count);

    hline();
    printf("  %-6s  %-10s  %-8s  %s\n", "Slot", "Type", "Rate", "Status");
    hline();

    const char *types[] = {"Normal","VIP","Reserved"};
    int rates[] = {RATE_NORMAL, RATE_VIP, RATE_RESERVED};
    for (int i = 0; i < MAX_SLOTS; i++) {
        int t = slots[i].slot_type;
        if (slots[i].is_occupied)
            printf("  %-6d  %-10s  PKR %-4d  " RED "[OCCUPIED]\n" RESET,
                   slots[i].slot_id, types[t], rates[t]);
        else
            printf("  %-6d  %-10s  PKR %-4d  " GREEN "[FREE]\n" RESET,
                   slots[i].slot_id, types[t], rates[t]);
    }

    save_slots_file();
    press_enter();
}

/* ── 4. Active Vehicles ── */
void view_active_vehicles() {
    print_banner();
    printf(CYAN "  ACTIVE VEHICLES\n" RESET);
    hline();

    char buf[8192]; memset(buf, 0, sizeof(buf));
    int bytes = read_file_contents(FILE_DRIVERS, buf, sizeof(buf));

    int record_count = 0;
    if (bytes > 0) {
        char tmp[8192]; memcpy(tmp, buf, sizeof(tmp));
        char *p = tmp;
        while (*p) {
            char *e = strchr(p, '\n'); if (!e) break; *e = '\0';
            char nm[MAX_NAME], cn[MAX_CNIC], pl[MAX_PLATE];
            int sid, st; char es[30];
            if (sscanf(p, "%49[^|]|%15[^|]|%11[^|]|%d|%d|%29[^\n]",
                       nm, cn, pl, &sid, &st, es) == 6)
                record_count++;
            p = e + 1;
        }
    }

    if (record_count == 0) {
        msg_warn("No active vehicles.");
        /* Still show queue if any */
        if (queue.count > 0) {
            printf(YELLOW "\n  FCFS Queue (%d waiting):\n" RESET, queue.count);
            hline();
            const char *qt[] = {"Normal","VIP","Reserved"};
            for (int i = 0; i < queue.count; i++) {
                int idx = (queue.front + i) % MAX_QUEUE;
                Vehicle *qv = &queue.vehicles[idx];
                int t = (qv->slot_type >= 0 && qv->slot_type <= 2) ? qv->slot_type : 0;
                printf("  [%d] %-20s  %-12s  Waiting for: %s\n",
                       i+1, qv->name, qv->plate, qt[t]);
            }
        } else {
            printf(YELLOW "\n  No vehicles in FCFS waiting queue.\n" RESET);
        }
        press_enter();
        return;
    }

    printf("  %-20s  %-16s  %-12s  %-6s  %-10s  %s\n",
           "Name","CNIC","Plate","Slot","Type","Entry Time");
    hline();

    const char *types[] = {"Normal","VIP","Reserved"};
    char nm[MAX_NAME], cn[MAX_CNIC], pl[MAX_PLATE];
    int sid, st; char es[30];
    int count = 0;
    char *ptr = buf;

    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        if (sscanf(ptr, "%49[^|]|%15[^|]|%11[^|]|%d|%d|%29[^\n]",
                   nm, cn, pl, &sid, &st, es) == 6) {
            int t = (st >= 0 && st <= 2) ? st : 0;
            printf("  %-20s  %-16s  %-12s  %-6d  %-10s  %s\n",
                   nm, cn, pl, sid, types[t], es);
            count++;
        }
        ptr = end + 1;
    }

    hline();
    printf("  Total: %d vehicle(s)\n", count);

    /* FCFS Queue section */
    printf("\n");
    hline();
    if (queue.count > 0) {
        printf(YELLOW "  FCFS Queue (%d waiting):\n" RESET, queue.count);
        hline();
        const char *qt[] = {"Normal","VIP","Reserved"};
        for (int i = 0; i < queue.count; i++) {
            int idx = (queue.front + i) % MAX_QUEUE;
            Vehicle *qv = &queue.vehicles[idx];
            int t = (qv->slot_type >= 0 && qv->slot_type <= 2) ? qv->slot_type : 0;
            printf("  [%d] %-20s  %-12s  Waiting for: %s\n",
                   i+1, qv->name, qv->plate, qt[t]);
        }
    } else {
        printf(YELLOW "  No vehicles in FCFS waiting queue.\n" RESET);
    }

    press_enter();
}

/* ── 5. Search Vehicle ── */
void search_vehicle() {
    print_banner();
    printf(CYAN "  SEARCH VEHICLE\n" RESET);
    hline();
    char plate[MAX_PLATE];
    iprompt("Plate No"); getchar(); scanf("%11s", plate);

    char buf[8192]; memset(buf, 0, sizeof(buf));
    read_file_contents(FILE_DRIVERS, buf, sizeof(buf));
    const char *types[] = {"Normal","VIP","Reserved"};
    char nm[MAX_NAME], cn[MAX_CNIC], rpl[MAX_PLATE];
    int sid, st; char es[30];
    int found = 0; char *ptr = buf;
    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        if (sscanf(ptr, "%49[^|]|%15[^|]|%11[^|]|%d|%d|%29[^\n]",
                   nm, cn, rpl, &sid, &st, es) == 6) {
            if (strcmp(rpl, plate) == 0) {
                struct tm etm; memset(&etm, 0, sizeof(etm));
                sscanf(es, "%d-%d-%d %d:%d:%d",
                       &etm.tm_year, &etm.tm_mon, &etm.tm_mday,
                       &etm.tm_hour, &etm.tm_min, &etm.tm_sec);
                etm.tm_year -= 1900; etm.tm_mon -= 1; etm.tm_isdst = -1;
                time_t et  = mktime(&etm);
                time_t now = time(NULL);
                double hours = difftime(now, et) / 3600.0;
                if (hours < 0) hours = 0;
                int rate = (st == VIP) ? RATE_VIP :
                           (st == RESERVED) ? RATE_RESERVED : RATE_NORMAL;
                int t = (st >= 0 && st <= 2) ? st : 0;
                hline();
                printf("  Name       : %s\n", nm);
                printf("  CNIC       : %s\n", cn);
                printf("  Plate No   : %s\n", rpl);
                printf("  Slot       : %d (%s)\n", sid, types[t]);
                printf("  Entry Time : %s\n", es);
                printf("  Parked For : %.2f hours\n", hours);
                printf(GREEN "  Est. Bill  : PKR %.2f\n" RESET, hours * rate);
                hline();
                found = 1; break;
            }
        }
        ptr = end + 1;
    }
    if (!found) msg_err("Vehicle not found.");
    press_enter();
}

/* ── 6. Generate Report ── */
void generate_report() {
    print_banner();
    printf("\n");
    printf(CYAN "  DAILY REPORT\n" RESET);
    hline();

    time_t now = time(NULL); struct tm *t = localtime(&now);
    char date_str[30]; strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", t);

    char bbuf[16384]; memset(bbuf, 0, sizeof(bbuf));
    read_file_contents(FILE_BILLING, bbuf, sizeof(bbuf));
    int occ = 0; for (int i = 0; i < MAX_SLOTS; i++) if (slots[i].is_occupied) occ++;

    double total_rev = 0; int txn_count = 0;
    char nm[MAX_NAME], cn[MAX_CNIC], pl[MAX_PLATE], stn[12], es[30], xs[30];
    int sid; double bill;

    printf("  Generated : %s\n\n", date_str);
    printf("  Total Slots : %d  |  Occupied : %d  |  Available : %d\n\n",
           MAX_SLOTS, occ, MAX_SLOTS-occ);
    hline();
    printf("  %-20s  %-10s  %-6s  %-8s  %s\n","Name","Plate","Slot","Type","Bill");
    hline();

    char rpt[32768]; memset(rpt, 0, sizeof(rpt));
    snprintf(rpt, sizeof(rpt),
             "================================================\n"
             "         CAR PARKING MANAGEMENT SYSTEM\n"
             "================================================\n\n"
             "DAILY REPORT\n"
             "------------------------------------------------\n"
             "Generated : %s\n\n"
             "Total Slots : %d  |  Occupied : %d  |  Available : %d\n\n"
             "------------------------------------------------\n"
             "  %-20s  %-10s  %-6s  %-8s  %s\n"
             "------------------------------------------------\n",
             date_str, MAX_SLOTS, occ, MAX_SLOTS-occ,
             "Name","Plate","Slot","Type","Bill");

    char *ptr = bbuf;
    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        if (sscanf(ptr, "%49[^|]|%15[^|]|%11[^|]|%d|%11[^|]|%29[^|]|%29[^|]|%lf",
                   nm, cn, pl, &sid, stn, es, xs, &bill) == 8) {
            printf("  %-20s  %-10s  %-6d  %-8s  PKR %.2f\n", nm, pl, sid, stn, bill);
            char rec[256];
            snprintf(rec, sizeof(rec), "  %-20s  %-10s  %-6d  %-8s  PKR %.2f\n",
                     nm, pl, sid, stn, bill);
            strncat(rpt, rec, sizeof(rpt)-strlen(rpt)-1);
            total_rev += bill; txn_count++;
        }
        ptr = end + 1;
    }

    hline();
    printf(GREEN "  Total Revenue : PKR %.2f  (%d transactions)\n" RESET, total_rev, txn_count);
    hline();

    char foot[256];
    snprintf(foot, sizeof(foot),
             "------------------------------------------------\n"
             "Total Revenue : PKR %.2f  (%d transactions)\n"
             "------------------------------------------------\n",
             total_rev, txn_count);
    strncat(rpt, foot, sizeof(rpt)-strlen(rpt)-1);

    overwrite_file(FILE_REPORT, rpt);
    printf("  Report saved to: data/reports.txt\n");
    press_enter();
}

/* ── 7. Revenue Summary ── */
void revenue_summary() {
    print_banner();
    printf(CYAN "  REVENUE SUMMARY\n" RESET);
    hline();

    char bbuf[16384]; memset(bbuf, 0, sizeof(bbuf));
    read_file_contents(FILE_BILLING, bbuf, sizeof(bbuf));
    double total = 0, nr = 0, vr = 0, rr = 0;
    int tc = 0, nc = 0, vc = 0, rc = 0;
    char nm[MAX_NAME], cn[MAX_CNIC], pl[MAX_PLATE], stn[12], es[30], xs[30];
    int sid; double bill; char *ptr = bbuf;
    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        if (sscanf(ptr, "%49[^|]|%15[^|]|%11[^|]|%d|%11[^|]|%29[^|]|%29[^|]|%lf",
                   nm, cn, pl, &sid, stn, es, xs, &bill) == 8) {
            total += bill; tc++;
            if      (strcmp(stn, "Normal")   == 0) { nr += bill; nc++; }
            else if (strcmp(stn, "VIP")      == 0) { vr += bill; vc++; }
            else                                    { rr += bill; rc++; }
        }
        ptr = end + 1;
    }

    printf("  %-12s  %-8s  %s\n","Type","Txns","Revenue");
    hline();
    printf("  %-12s  %-8d  PKR %.2f\n","Normal",   nc, nr);
    printf("  %-12s  %-8d  PKR %.2f\n","VIP",      vc, vr);
    printf("  %-12s  %-8d  PKR %.2f\n","Reserved", rc, rr);
    hline();
    printf(GREEN "  %-12s  %-8d  PKR %.2f\n" RESET,"TOTAL", tc, total);
    hline();
    press_enter();
}

/* ── 8. Transactions ── */
void view_transactions() {
    print_banner();
    printf(CYAN "  TRANSACTION HISTORY\n" RESET);
    hline();

    char buf[16384]; memset(buf, 0, sizeof(buf));
    int n = read_file_contents(FILE_TRANSACTIONS, buf, sizeof(buf));
    if (n <= 0) { printf("  No transactions yet.\n"); press_enter(); return; }

    char *ptr = buf; int row = 0;
    while (*ptr) {
        char *end = strchr(ptr, '\n'); if (!end) break; *end = '\0';
        if      (strncmp(ptr, "ENTRY", 5) == 0) printf(GREEN "  %s\n" RESET, ptr);
        else if (strncmp(ptr, "EXIT",  4) == 0) printf(RED   "  %s\n" RESET, ptr);
        else                                     printf("  %s\n", ptr);
        row++; ptr = end + 1;
    }
    hline();
    printf("  Total: %d record(s)\n", row);
    press_enter();
}

/* ── 9. Backup ── */
void backup_data() {
    print_banner();
    printf(CYAN "  DATA BACKUP\n" RESET);
    hline();
    printf("  Running backup via child process (fork + execl)...\n\n");

    pid_t pid = fork();
    if (pid < 0) { perror("fork()"); return; }
    else if (pid == 0) {
        execl("/bin/sh","sh","scripts/backup.sh",(char*)NULL);
        time_t now = time(NULL); char ts[30];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
        char hdr[256]; snprintf(hdr, sizeof(hdr), "=== BACKUP: %s ===\n\n", ts);
        int fd = open(FILE_BACKUP, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        if (fd >= 0) {
            char tmp[16384]; int r;
            write(fd, hdr, strlen(hdr));
            write(fd, "-- DRIVERS --\n", 14);
            r = read_file_contents(FILE_DRIVERS, tmp, sizeof(tmp)); if (r > 0) write(fd, tmp, r);
            write(fd, "\n-- BILLING --\n", 15);
            r = read_file_contents(FILE_BILLING, tmp, sizeof(tmp)); if (r > 0) write(fd, tmp, r);
            write(fd, "\n-- TRANSACTIONS --\n", 20);
            r = read_file_contents(FILE_TRANSACTIONS, tmp, sizeof(tmp)); if (r > 0) write(fd, tmp, r);
            write(fd, "\n-- SLOTS --\n", 13);
            r = read_file_contents(FILE_SLOTS, tmp, sizeof(tmp)); if (r > 0) write(fd, tmp, r);
            close(fd);
        }
        exit(0);
    } else wait(NULL);

    msg_ok("Backup saved to: data/backup.txt");
    press_enter();
}

/* ── main ── */
int main() {
    init_data_dir(); init_slots(); queue_init();
    int attempts = 0;
    while (attempts < 3) {
        print_banner();
        if (authenticate_admin()) { admin_menu(); return 0; }
        attempts++;
        if (attempts < 3)
            printf(RED "  Attempts remaining: %d\n" RESET, 3 - attempts);
        sleep(1);
    }
    print_banner();
    msg_err("System locked. Too many failed attempts.");
    printf("\n"); return 1;
}
