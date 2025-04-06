/* treasure_manager.c - Phase 1 implementation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"
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
    mkdir(hunt_id);
    chmod(hunt_id, 0755);
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

int main(int argc, char **argv) {
    if(argc < 3) { printf("Usage error\n"); exit(1); }

    if(!strcmp(argv[1], "--add")) add_treasure(argv[2]);
    else if(!strcmp(argv[1], "--list")) list_treasures(argv[2]);
    else if(!strcmp(argv[1], "--remove_treasure")) {
        if(argc != 4) { printf("Usage error\n"); exit(1); }
        remove_treasure(argv[2], atoi(argv[3]));
    }
    else if(!strcmp(argv[1], "--remove_hunt")) remove_hunt(argv[2]);
    else printf("Unknown command\n");

    char symlink_path[256], logfile_path[256];
    snprintf(logfile_path, sizeof(logfile_path), "%s/%s", argv[2], LOG_FILE);
    snprintf(symlink_path, sizeof(symlink_path), "logged_hunt-%s", argv[2]);
    symlink(logfile_path, symlink_path);

    return 0;
}
