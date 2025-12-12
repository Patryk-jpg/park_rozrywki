//
// Created by janik on 11/12/2025.
//

#include "Park.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
#include <stdbool.h>
Park* g_park = nullptr;


int   main() {
    int fd = open(SEED_FILENAME, O_CREAT | O_RDONLY, 0666);
    close(fd);
    g_park = attach_to_shared_block();
    memset(g_park, 0, sizeof(Park));
    g_park->pid_parku = getpid();
    detach_from_shared_block(g_park);





}

int get_shared_block_id() {
    key_t key;
    key = ftok(SEED_FILENAME, *PARK_SEED);
    if (key == IPC_ERROR) {
        perror("ftok");
        return IPC_ERROR;
    }
    return shmget(key, sizeof(Park), 0666 | IPC_CREAT);
}

Park * attach_to_shared_block() {
    int blockId = get_shared_block_id();
    Park *result;
    if (blockId == IPC_ERROR){
        perror("shmget");
        return NULL;
    }
    result = (Park*) shmat(blockId, 0, 0);
    if (result == (Park*) IPC_ERROR) {
        perror("shmat");
        return NULL;
    }
    return result;
}

bool detach_from_shared_block(Park *block) {
    return shmdt(block) != IPC_ERROR;
}

bool destroy_shared_block(char *filename) {
        int shared_block_id = get_shared_block_id();
        if (shared_block_id == IPC_ERROR) {
            return NULL;
        }
    return shmctl(shared_block_id, IPC_RMID, NULL) != IPC_ERROR;
}
