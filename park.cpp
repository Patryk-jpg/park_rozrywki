//
// Created by janik on 12/12/2025.
//
#include "park_wspolne.h"
#include <sys/wait.h>
park_wspolne* g_park = nullptr;
std::vector<pid_t> pracownicy_pids;
static volatile sig_atomic_t signal3 = 0;


void uruchom_pracownikow() {

    for (int i=0; i < 17; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork - pracownik");
            exit(1);
        }
        if (pid == 0) {
            char arg[16];
            snprintf(arg, sizeof(arg), "%d", i);

            char* args[] = { (char*)"pracownik", arg, NULL };
            execvp("./pracownik",args);
            perror("execvp-pracownik");
            _exit(1);
        }

        pracownicy_pids.push_back(pid);


    }

}

void sig3handler(int sig) {
    printf("Otrzymano sygnal totalnej ewakuacji i zamkniecia (3) sygnał %d!\n", sig);
    signal3 = 1;

}
void zakoncz_pracownikow() {
    printf("[PARK] Wysylam sygnały SIGUSR1 do pracownikow...\n");
    for (auto pid : pracownicy_pids) {
        if (kill(pid, SIGUSR1) == -1) {
            perror("kill SIGUSR1");
        }
    }

    printf("[PARK] Czekam na zakonczenie pracowników...\n");
    for (size_t i = 0; i < pracownicy_pids.size(); i++) {
        int status;
        pid_t pid = waitpid(pracownicy_pids[i], &status, 0);
        if (pid > 0) {
            printf("[PARK] Pracownik %zu (PID: %d) zakonczył pracę\n", i, pid);
        }
    }
}
void zbierz_zombie_procesy() {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}
int   main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig3handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }
    printf("========================================\n");
    printf("   PARK ROZRYWKI - SYMULACJA          \n");
    printf("========================================\n\n");
    srand(time(NULL));
    const int memfile = open(SEED_FILENAME_PARK, O_CREAT | O_RDONLY, 0666);
    error_check(memfile, "open");
    close(memfile);
    const int semfile = open(SEED_FILENAME_SEMAPHORES, O_CREAT | O_RDONLY, 0666);
    error_check(semfile, "open");
    close(semfile);
    const int queuefile = open(SEED_FILENAME_QUEUE, O_CREAT | O_RDONLY, 0666);
    error_check(queuefile, "open");
    close(queuefile);

    g_park = attach_to_shared_block();
    memset(g_park, 0, sizeof(park_wspolne));
    g_park->pid_parku = getpid();
    g_park->park_otwarty = true;
    g_park->czas_w_symulacji.hour = CZAS_OTWARCIA;
    g_park->czas_w_symulacji.minute = 0;
    int sem_key = ftok(SEED_FILENAME_SEMAPHORES, SEM_SEED);
    int licznik_klientow = allocate_semaphore(sem_key, 1, 0666| IPC_CREAT | IPC_EXCL);
    for (int i = 0; i < (sizeof(atrakcje) / sizeof(atrakcje[0]))-1; i++) {
        g_park->pracownicy_keys[i] = create_message_queue(SEED_FILENAME_QUEUE, i);
    }
    g_park->licznik_klientow = licznik_klientow;
    uruchom_pracownikow();
    g_park->uruchom_kase();
    g_park->uruchom_kase_restauracji();
    while (g_park->park_otwarty || MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0) != 0) {
        if (signal3) {
            printf("\n[PARK] *** EWAKUACJA - ZAMYKAM PARK ***\n");
            g_park->park_otwarty = false;
            zakoncz_pracownikow();
            break;
        }
        usleep(50000);
        g_park->czas_w_symulacji.increment_minute();
        //g_park->czas_w_symulacji.print();
        printf("Aktualni ludzie w parku %d, \n", MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0));
        fflush(stdout);
        if (g_park->czas_w_symulacji.hour >= CZAS_ZAMKNIECIA) {

            g_park->park_otwarty = false;
        }
        if ((rand() % 100 > 40) && (g_park->park_otwarty)) {
            pid_t pid = fork();
            if (pid == 0) {
                char* args[] = { (char*)"klient", NULL };
                execvp("./klient", args);
                perror("execvp-klient");
                _exit(1);

            }
        }
    }
    printf("PARK ZAMKNIETY");
    free_semaphore(g_park->licznik_klientow, 0);
    detach_from_shared_block(g_park);
    destroy_shared_block((char*)SEED_FILENAME_PARK);

}
