
#include "park_wspolne.h"
#include <sys/wait.h>
park_wspolne* g_park = nullptr;
std::vector<pid_t> pracownicy_pids;
pid_t kasa_pid = -1;
pid_t kasa_rest_pid = -1;
int kasa_rest_id =  -1;
int kasa_id = -1;
static volatile sig_atomic_t signal3 = 0;

void poczekaj_na_kasy() {
    printf("[PARK] Czekam na zakończenie kas...\n");

    if (kasa_pid > 0) {
        int status;
        printf("[PARK] Czekam na kasę główną (PID: %d)...\n", kasa_pid);
        pid_t result = waitpid(kasa_pid, &status, 0);
        if (result > 0) {
            printf("[PARK] Kasa główna zakończona\n");
        } else {
            perror("waitpid kasa");
        }
    }

    if (kasa_rest_pid > 0) {
        int status;
        printf("[PARK] Czekam na kasę restauracji (PID: %d)...\n", kasa_rest_pid);
        pid_t result = waitpid(kasa_rest_pid, &status, 0);
        if (result > 0) {
            printf("[PARK] Kasa restauracji zakończona\n");
        } else {
            perror("waitpid kasa restauracji");
        }
    }
}
void uruchom_kase() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork - kasa");
        exit(1);
    }
    if (pid == 0) {
        char* args[] = { (char*)"Kasa", NULL };
        execvp("./Kasa", args);
        perror("execvp - kasa");
        _exit(1);
    }
    kasa_pid = pid;
}
void uruchom_kase_restauracji() {
    printf("[PARK] Uruchamianie kasy restauracji...\n");
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork - kasa restauracji");
        exit(1);
    }
    if (pid == 0) {
        char* args[] = { (char*)"kasaRestauracji", NULL };
        execvp("./restauracja", args);
        perror("execvp - kasa restauracji");
        _exit(1);
    }
    kasa_rest_pid = pid;
}
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
    for (size_t i = 0; i < LICZBA_ATRAKCJI-1; i++) {
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
    kasa_id = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    kasa_rest_id = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED);

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
    uruchom_kase();
    uruchom_kase_restauracji();
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

    if (!signal3) {
        zakoncz_pracownikow();
    }
    poczekaj_na_kasy();
    if (msgctl(kasa_rest_id, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID restauracja");
    }
    if (msgctl(kasa_id, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID kasa");
    }
    for (int i = 0; i < LICZBA_ATRAKCJI - 1; i++) {
        int kolejka_id = g_park->pracownicy_keys[i];
        if (kolejka_id > 0) {
            if (msgctl(kolejka_id, IPC_RMID, NULL) == -1) {
                perror("msgctl IPC_RMID pracownicy msqueue");
            }
        }
    }
    printf("[PARK] Zbieranie pozostałych procesów...\n");
    int status;
    // while (wait(&status) > 0) {
    // }
    printf("PARK ZAMKNIETY");
    printf("Sprzątanie zasobów...\n");
    free_semaphore(g_park->licznik_klientow, 0);
    detach_from_shared_block(g_park);
    destroy_shared_block((char*)SEED_FILENAME_PARK);
    printf(" Koniec symulacji\n");
    return 0;
}
