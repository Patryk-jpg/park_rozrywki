//
// Created by janik on 11/12/2025.
//

#include "park_wspolne.h"

using namespace std;


int get_shared_block_id() {
    key_t key;
    key = ftok(SEED_FILENAME_PARK, PARK_SEED);
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
            return false;
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
    union semun arg;
    arg.val = val;
    if ( semctl(semId, number, SETVAL, arg) == -1 ) {
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
    while (true) {
        int result = semop(semId, operacje, 1);

        if (result == 0) {
            return 1;
        }
        if (errno == EINTR) {
            continue;
        }
        perror("semop");
        return -1;
    }
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

SimTime SimTime::operator+(const SimTime &other) const {

        SimTime result;
        int total = (hour * 60 + minute) + (other.hour * 60 + other.minute);
        result.hour = total / 60;
        result.minute = total % 60;
        if (result.hour >= CZAS_ZAMKNIECIA) {
            result.hour = CZAS_ZAMKNIECIA;
            if (result.minute > 0) {
                result.minute = 0;
            }
        }
        return result;

}







int create_message_queue(const char* filename, int seed, int msgflg) {
    key_t key = ftok(filename, seed);
    error_check((int) key, "ftok");
    int kasaId = msgget(key, msgflg | IPC_CREAT | IPC_EXCL);
    error_check(kasaId, "msgget");
    return kasaId;
}
int join_message_queue(const char* filename, int seed) {
    key_t key = ftok(filename, seed);
    error_check((int) key, "ftok");
    int kasaId = msgget(key, 0666 );
    error_check(kasaId, "msgget");
    return kasaId;
}
void error_check(int id, const string& message) {
    if (id < 0) {
        perror(message.c_str());
        exit(1);
    }
}

float oblicz_koszt_restauracji(int czas_min) {
    if (czas_min == 0) return 0.0f;
    float koszt = 5.0f + (czas_min / 10) * 2.0f;
    koszt += random_int(5, 20);
    return koszt;
}