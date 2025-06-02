#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define TREASURE_FILE "treasures.dat"
#define USER_NAME_SIZE 32
#define MAX_USERS 100

typedef struct {
    int treasure_id;
    char username[USER_NAME_SIZE];
    double latitude;
    double longitude;
    char clue[256];
    int value;
} Treasure;

typedef struct {
    char username[USER_NAME_SIZE];
    int total_score;
} UserScore;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return 1;
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", argv[1], TREASURE_FILE);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    UserScore users[MAX_USERS];
    int user_count = 0;

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) > 0) {
        int found = 0;
        for (int i = 0; i < user_count; ++i) {
            if (strcmp(users[i].username, t.username) == 0) {
                users[i].total_score += t.value;
                found = 1;
                break;
            }
        }
        if (!found && user_count < MAX_USERS) {
            strncpy(users[user_count].username, t.username, USER_NAME_SIZE);
            users[user_count].total_score = t.value;
            user_count++;
        }
    }
    close(fd);

    for (int i = 0; i < user_count; ++i) {
        printf("%s: %d\n", users[i].username, users[i].total_score);
    }
    return 0;
}
