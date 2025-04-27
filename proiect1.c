/* treasure_system.c - Phase 1 + Phase 2 implementation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"
#define COMMAND_FILE "command.txt"
#define MAX_CLUE 256
#define USER_NAME_SIZE 32

// Phase 1 - Treasure Management

typedef struct {
    int treasure_id;
    char username[USER_NAME_SIZE];
    double latitude;
    double longitude;
    char clue[MAX_CLUE];
    int value;
} Treasure;

void log_operation(char *hunt_id, char *message) {
    char logpath[256];
    snprintf(logpath, sizeof(logpath), "%s/%s", hunt_id, LOG_FILE);
    int fd = open(logpath, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd >= 0) {
        write(fd, message, strlen(message));
        write(fd, "\n", 1);
        close(fd);
    }
}

void create_hunt_dir(char *hunt_id) {
    if (mkdir(hunt_id, 0755) < 0) {
        perror("mkdir");
        exit(1);
    }
}

void add_treasure(char *hunt_id) {
    Treasure tr;
    char path[256];
    int fd;

    create_hunt_dir(hunt_id);

    printf("Enter Treasure ID: "); scanf("%d", &tr.treasure_id);
    printf("Enter Username: "); scanf("%s", tr.username);
    printf("Enter Latitude: "); scanf("%lf", &tr.latitude);
    printf("Enter Longitude: "); scanf("%lf", &tr.longitude);
    printf("Enter Clue: "); getchar(); fgets(tr.clue, MAX_CLUE, stdin); strtok(tr.clue, "\n");
    printf("Enter Value: "); scanf("%d", &tr.value);

    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);
    fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if(fd < 0){ perror("open"); exit(1); }
    write(fd, &tr, sizeof(Treasure));
    close(fd);

    log_operation(hunt_id, "Treasure added");
}

void list_treasures(char *hunt_id) {
    Treasure tr;
    char path[256];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);
    int fd = open(path, O_RDONLY);
    if(fd < 0){ perror("open"); exit(1); }

    stat(path, &st);
    printf("Hunt: %s\n", hunt_id);
    printf("File size: %ld bytes, Last modified: %s", st.st_size, ctime(&st.st_mtime));

    printf("Treasures:\n");
    while(read(fd, &tr, sizeof(Treasure)) > 0) {
        printf("ID: %d | User: %s | Lat: %.4f | Long: %.4f | Clue: %s | Value: %d\n",
               tr.treasure_id, tr.username, tr.latitude, tr.longitude, tr.clue, tr.value);
    }
    close(fd);

    log_operation(hunt_id, "List treasures");
}

void remove_treasure(char *hunt_id, int tid) {
    Treasure tr;
    char path[256], tmp[256];

    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);
    snprintf(tmp, sizeof(tmp), "%s/tmp.dat", hunt_id);

    int fd = open(path, O_RDONLY);
    int tmpfd = open(tmp, O_CREAT | O_WRONLY, 0644);

    if(fd < 0 || tmpfd < 0) { perror("open"); exit(1); }

    int found = 0;
    while(read(fd, &tr, sizeof(Treasure)) > 0) {
        if(tr.treasure_id != tid)
            write(tmpfd, &tr, sizeof(Treasure));
        else
            found = 1;
    }

    close(fd); close(tmpfd);
    rename(tmp, path);

    if(found) log_operation(hunt_id, "Treasure removed");
}

void remove_hunt(char *hunt_id) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", hunt_id);
    system(cmd);
}

// Phase 2 - Processes & Signals

pid_t monitor_pid = -1;
int monitor_exiting = 0;

void sigchld_handler(int sig) {
    wait(NULL);
    monitor_pid = -1;
    printf("Monitor has exited.\n");
}

void monitor_process() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    while(1) {
        pause();

        FILE *cmd = fopen(COMMAND_FILE, "r");
        if (!cmd) continue;

        char command[32];
        char hunt_id[128];
        int tid;
        fscanf(cmd, "%s", command);

        if (strcmp(command, "list_hunts") == 0) {
            system("ls -d */ | sed 's#/##'");
        }
        else if (strcmp(command, "list_treasures") == 0) {
            fscanf(cmd, "%s", hunt_id);
            list_treasures(hunt_id);
        }
        else if (strcmp(command, "view_treasure") == 0) {
            fscanf(cmd, "%s %d", hunt_id, &tid);
            printf("View Treasure ID: %d in Hunt: %s (details not implemented yet)\n", tid, hunt_id);
        }
        else if (strcmp(command, "stop") == 0) {
            printf("Monitor exiting...\n");
            usleep(2000000); // 2 seconds delay
            exit(0);
        }
        fclose(cmd);
    }
}

void start_monitor() {
    monitor_pid = fork();
    if (monitor_pid == 0) {
        monitor_process();
        exit(0);
    } else if (monitor_pid < 0) {
        perror("fork");
        exit(1);
    }
    printf("Monitor started. PID: %d\n", monitor_pid);
}

void send_command(const char *cmdline) {
    FILE *cmd = fopen(COMMAND_FILE, "w");
    if (!cmd) { perror("fopen"); return; }
    fprintf(cmd, "%s\n", cmdline);
    fclose(cmd);
}

int main(int argc, char **argv) {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    while (1) {
        char input[256];
        printf("Enter command: ");
        if (!fgets(input, sizeof(input), stdin)) break;
        strtok(input, "\n");

        if (strcmp(input, "start_monitor") == 0) {
            if (monitor_pid > 0) printf("Monitor already running.\n");
            else start_monitor();
        }
        else if (strcmp(input, "list_hunts") == 0) {
            if (monitor_pid > 0) {
                send_command("list_hunts");
                kill(monitor_pid, SIGUSR1);
            } else printf("Monitor not running.\n");
        }
        else if (strncmp(input, "list_treasures", 14) == 0) {
            if (monitor_pid > 0) {
                char *hunt = input + 15;
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "list_treasures %s", hunt);
                send_command(buffer);
                kill(monitor_pid, SIGUSR2);
            } else printf("Monitor not running.\n");
        }
        else if (strncmp(input, "view_treasure", 13) == 0) {
            if (monitor_pid > 0) {
                char *args = input + 14;
                send_command(args);
                kill(monitor_pid, SIGTERM);
            } else printf("Monitor not running.\n");
        }
        else if (strcmp(input, "stop_monitor") == 0) {
            if (monitor_pid > 0) {
                send_command("stop");
                kill(monitor_pid, SIGINT);
                monitor_exiting = 1;
            } else printf("Monitor not running.\n");
        }
        else if (strcmp(input, "exit") == 0) {
            if (monitor_pid > 0) printf("Cannot exit while monitor is running!\n");
            else break;
        }
        else {
            printf("Unknown command.\n");
        }
    }
    return 0;
}
