

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
#include <errno.h>

#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"
#define COMMAND_FILE "command.txt"
#define MAX_CLUE 256
#define USER_NAME_SIZE 32

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
    mkdir(hunt_id, 0755); // if already exists, ignore error
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
    if(fd < 0){ printf("Could not open treasures for hunt %s\n", hunt_id); return; }

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

void view_treasure(char *hunt_id, int tid) {
    Treasure tr;
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", hunt_id, TREASURE_FILE);

    int fd = open(path, O_RDONLY);
    if(fd < 0){ printf("Could not open treasures for hunt %s\n", hunt_id); return; }

    int found = 0;
    while(read(fd, &tr, sizeof(Treasure)) > 0) {
        if (tr.treasure_id == tid) {
            printf("Treasure Details:\n");
            printf("ID: %d\nUser: %s\nLatitude: %.4f\nLongitude: %.4f\nClue: %s\nValue: %d\n",
                   tr.treasure_id, tr.username, tr.latitude, tr.longitude, tr.clue, tr.value);
            found = 1;
            break;
        }
    }
    close(fd);
    if (!found)
        printf("Treasure ID %d not found in hunt %s.\n", tid, hunt_id);
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

/* === Phase 2/3: Process & Signal Handling === */
pid_t monitor_pid = -1;
int monitor_exiting = 0;
int pipefd[2];

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
            view_treasure(hunt_id, tid);
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
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(1);
    }
    monitor_pid = fork();
    if (monitor_pid == 0) {
        // Child: redirect stdout to pipe
        close(pipefd[0]); // close read end
        dup2(pipefd[1], STDOUT_FILENO); // stdout -> write end
        close(pipefd[1]);
        monitor_process();
        exit(0);
    } else if (monitor_pid < 0) {
        perror("fork");
        exit(1);
    }
    close(pipefd[1]); // parent closes write end
    printf("Monitor started. PID: %d\n", monitor_pid);
}

void send_command(const char *cmdline) {
    FILE *cmd = fopen(COMMAND_FILE, "w");
    if (!cmd) { perror("fopen"); return; }
    fprintf(cmd, "%s\n", cmdline);
    fclose(cmd);
}

void print_from_monitor_pipe() {
    // Non-blocking read (so child can exit and not deadlock)
    char buffer[4096];
    ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = 0;
        printf("%s", buffer);
    }
}

/* === Main Loop === */

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

        // ---- Treasure manager CLI ----
        if (strncmp(input, "add ", 4) == 0) {
            add_treasure(input + 4);
        }
        else if (strncmp(input, "list ", 5) == 0) {
            list_treasures(input + 5);
        }
        else if (strncmp(input, "view ", 5) == 0) {
            char hunt[128];
            int tid;
            if (sscanf(input + 5, "%s %d", hunt, &tid) == 2)
                view_treasure(hunt, tid);
            else
                printf("Usage: view <hunt_id> <id>\n");
        }
        else if (strncmp(input, "remove_treasure ", 16) == 0) {
            char hunt[128]; int tid;
            if (sscanf(input + 16, "%s %d", hunt, &tid) == 2)
                remove_treasure(hunt, tid);
            else
                printf("Usage: remove_treasure <hunt_id> <id>\n");
        }
        else if (strncmp(input, "remove_hunt ", 12) == 0) {
            remove_hunt(input + 12);
        }

        // ---- Monitor commands ----
        else if (strcmp(input, "start_monitor") == 0) {
            if (monitor_pid > 0) printf("Monitor already running.\n");
            else start_monitor();
        }
        else if (strcmp(input, "list_hunts") == 0) {
            if (monitor_pid > 0) {
                send_command("list_hunts");
                kill(monitor_pid, SIGUSR1);
                usleep(100 * 1000); // give child time to process
                print_from_monitor_pipe();
            } else printf("Monitor not running.\n");
        }
        else if (strncmp(input, "list_treasures ", 15) == 0) {
            if (monitor_pid > 0) {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "list_treasures %s", input + 15);
                send_command(buffer);
                kill(monitor_pid, SIGUSR2);
                usleep(100 * 1000);
                print_from_monitor_pipe();
            } else printf("Monitor not running.\n");
        }
        else if (strncmp(input, "view_treasure ", 14) == 0) {
            if (monitor_pid > 0) {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "view_treasure %s", input + 14);
                send_command(buffer);
                kill(monitor_pid, SIGTERM);
                usleep(100 * 1000);
                print_from_monitor_pipe();
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
        // ---- Phase 3: calculate_score ----
        else if (strcmp(input, "calculate_score") == 0) {
            // List all hunt directories
            FILE *fp = popen("ls -d */ 2>/dev/null", "r");
            if (!fp) { perror("popen"); continue; }
            char hunt[128];
            while (fgets(hunt, sizeof(hunt), fp)) {
                // Remove trailing '/' and '\n'
                char *slash = strchr(hunt, '/');
                if (slash) *slash = 0;
                else hunt[strcspn(hunt, "\n")] = 0;

                int cpipe[2];
                if (pipe(cpipe) < 0) continue;
                pid_t pid = fork();
                if (pid == 0) {
                    // Child: redirect stdout to pipe, run score_calculator
                    close(cpipe[0]);
                    dup2(cpipe[1], STDOUT_FILENO);
                    close(cpipe[1]);
                    execl("./score_calculator", "./score_calculator", hunt, NULL);
                    perror("execl");
                    exit(1);
                } else if (pid > 0) {
                    close(cpipe[1]);
                    char out[1024];
                    ssize_t n = read(cpipe[0], out, sizeof(out)-1);
                    if (n > 0) {
                        out[n] = 0;
                        printf("Scores for hunt %s:\n%s", hunt, out);
                    }
                    close(cpipe[0]);
                    waitpid(pid, NULL, 0);
                }
            }
            pclose(fp);
        }
        else {
            printf("Unknown command.\n");
        }
    }
    return 0;
}
