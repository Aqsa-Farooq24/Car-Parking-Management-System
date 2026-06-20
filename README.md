# Car-Parking-Management-System
Car Parking Management System (C, Linux) is a command-line application for Ubuntu that manages vehicle entry/exit, parking slot allocation, billing, and record keeping using file-based storage. It demonstrates OS concepts like FCFS scheduling, process management (fork/exec/wait), and system calls.

The project is organized in a single main folder named CARPARKINGSYSTEM, which contains the core source file parking.c and a shell script run.sh used to execute the program. It also includes two subdirectories: a data folder that stores all generated .txt files such as vehicle records, billing information, and reports, ensuring persistent storage of system data; and a scripts folder that contains utility shell scripts including show_report.sh for displaying generated reports and backup.sh for creating backups of stored data.

Features: 
Vehicle entry & exit management
FCFS-based slot allocation
Slot categories: Normal, VIP, Reserved
Automated billing based on time duration
Record keeping using text files
Report generation (daily reports, revenue)
Backup utility for data safety
OS concepts: fork(), exec(), wait(), system calls

How to Run the Project?
step 1 Open Terminal
Navigate to the project folder: cd CARPARKINGSYSTEM
step 2 Give Execute Permission
Make the run script executable: chmod +x run.sh
step 3 Run the Project: ./run.sh
