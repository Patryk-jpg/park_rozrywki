#include "park_wspolne.h"

using namespace std;

void print_error_impl(const char* file, int line, const char* func, const std::string& msg) {
    int error_code = errno; // bieżący kod błędu
    std::cerr << file << ":" << line << " " << func
              << "() | " << msg
              << ": " << std::strerror(error_code) << std::endl;
}

int get_shared_block_id() {
    key_t key = ftok(SEED_FILENAME_PARK, PARK_SEED);
    if (key == IPC_ERROR) {
        perror("ftok");
        return IPC_ERROR;
    }
    return shmget(key, sizeof(park_wspolne), 0666 | IPC_CREAT);
}

park_wspolne* attach_to_shared_block() {
    int blockId = get_shared_block_id();
    if (blockId == IPC_ERROR) {
        perror("shmget");
        return NULL;
    }

    park_wspolne* result = (park_wspolne*)shmat(blockId, 0, 0);
    if (result == (park_wspolne*)IPC_ERROR) {
        perror("shmat");
        return NULL;
    }

    return result;
}

bool detach_from_shared_block(park_wspolne *block) {
    return shmdt(block) != IPC_ERROR;
}

bool destroy_shared_block(char* filename) {
    int shared_block_id = get_shared_block_id();
    if (shared_block_id == IPC_ERROR) {
        return false;
    }
    return (shmctl(shared_block_id, IPC_RMID, NULL) != IPC_ERROR);
}

int allocate_semaphore(key_t key, int number, int flag) {
    int semId = semget(key, number, flag);
    if (semId == -1) {
        PRINT_ERROR("semget");
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
    struct sembuf operacje[1];
    operacje[0].sem_num = number;
    operacje[0].sem_flg = SEM_UNDO | flags;
    operacje[0].sem_op = -1;

    int result = semop(semId, operacje, 1);

    if (result == 0) {
        return 0; // Sukces
    }

    if (errno == EINTR) {
        return wait_semaphore(semId, number, flags); // Retry dla EINTR
    }

    if (errno == EIDRM || errno == EINVAL) {
        return -1;
    }

    PRINT_ERROR("semop wait");
    return -1;
}

void signal_semaphore(int semID, int number) {
    struct sembuf operacje[1];
    operacje[0].sem_num = number;
    operacje[0].sem_op = 1;
    operacje[0].sem_flg = SEM_UNDO;

    if (semop(semID, operacje, 1) == -1) {
        if (errno != EIDRM && errno != EINVAL) {
            PRINT_ERROR("semop signal");
        }
    }
}

int read_semaphore(int semID, int number) {
    int val = semctl(semID, number, GETVAL, NULL);
    if (val == -1) {
        // Semafor usunięty lub błąd
        if (errno == EIDRM || errno == EINVAL) {
            return 0; // Zwróć 0 jeśli usunięty
        }
    }
    return val;
}

SimTime SimTime::operator+(const SimTime& other) const {
    SimTime result;
    int total = (hour * 60 + minute) + (other.hour * 60 + other.minute);
    result.hour = total / 60;
    result.minute = total % 60;

    if (result.hour >= 24) {
        result.hour = result.hour % 24;
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
    error_check((int)key, "ftok");

    int queueId = msgget(key, 0666);
    if (queueId == -1) {
        if (errno == ENOENT) {
            return -1;
        }
        error_check(queueId, "msgget");
    }
    return queueId;
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