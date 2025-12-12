//
// Created by janik on 11/12/2025.
//

#include "park_wspolne.h"

using namespace std;

int get_shared_block_id() {
    key_t key;
    key = ftok(SEED_FILENAME, *PARK_SEED);
    if (key == IPC_ERROR) {
        perror("ftok");
        return IPC_ERROR;
    }
    return shmget(key, sizeof(park_wspolne), 0666 | IPC_CREAT);
}

park_wspolne * attach_to_shared_block() {
    int blockId = get_shared_block_id();
    park_wspolne *result;
    if (blockId == IPC_ERROR){
        perror("shmget");
        return NULL;
    }
    result = (park_wspolne*) shmat(blockId, 0, 0);
    if (result == (park_wspolne*) IPC_ERROR) {
        perror("shmat");
        return NULL;
    }
    return result;
}

bool detach_from_shared_block(park_wspolne *block) {
    return shmdt(block) != IPC_ERROR;
}

bool destroy_shared_block(char *filename) {
        int shared_block_id = get_shared_block_id();
        if (shared_block_id == IPC_ERROR) {
            return NULL;
        }
    return shmctl(shared_block_id, IPC_RMID, NULL) != IPC_ERROR;
}

int allocate_semaphore(key_t key, int number, int flag) {
    int semId =  semget(key, number, flag);
    if (semId == -1) {
        perror("semget");
        exit(1);
    }
    return semId;
}

int free_semaphore(int semId, int number) {
    return semctl(semId, number, IPC_RMID, 0);
}

void initialize_semaphore(int semId, int number, int val) {
    if ( semctl(semId, number, SETVAL, val) == -1 ) {
        perror("semctl SETVAl");
        exit(1);
    }
}

int wait_semaphore(int semId, int number, int flags) {
    int result;
    struct sembuf operacje[1];
    operacje[0].sem_num = number;
    operacje[0].sem_flg = 0  | flags;
    operacje[0].sem_op = -1;
    if (semop(semId, operacje, 1) == -1) {
        perror("semop");
        return -1;
    }
    return 1;
}

void signal_semaphore(int semID, int number) {
    struct sembuf operacje[1];
    operacje[0].sem_num = number;
    operacje[0].sem_op = 1;
    if (semop(semID, operacje, 1) == -1 )
        perror("semop SIGNAL ");
}

int read_semaphore(int semID, int number) {
    return semctl(semID, number, GETVAL, NULL);
}

void park_wspolne::uruchom_pracownikow() {

    for (int i=0; i < 17; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork - pracownik");
            exit(1);
        }
        if (pid == 0) {
            execl("./pracownik", "pracownik", i, NULL);
            perror("execl");
            exit(1);
        }
    }

}
void park_wspolne::uruchom_kase() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork - kasa");
        exit(1);
    }
    execl("./Kasa", "Kasa", NULL);

}

void park_wspolne::uruchom_kase_restauracji() {

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork - kasa restauracji");
        execl("./Restauracja", "Kasa", NULL);
    }
}


void handler_zamknij_park(int sig) {

    printf("zamknij_park\n");
}
