
#include "park_wspolne.h"
#include <sys/wait.h>

// GLOBALNE
park_wspolne* g_park = nullptr;
std::vector<pid_t> pracownicy_pids;
pid_t kasa_pid = -1;
pid_t kasa_rest_pid = -1;
int kasa_rest_id =  -1;
int kasa_id = -1;
static volatile sig_atomic_t signal3 = 0;


void poczekaj_na_kasy() {
    printf("[PARK] Czekam na zakończenie kas...\n");
    if (signal3) {
        if (kasa_pid > 0) kill(kasa_pid, SIGUSR1);
        if (kasa_rest_pid > 0) kill(kasa_rest_pid, SIGUSR1);
    }
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
    usleep(10000);
}
void uruchom_kase_restauracji() {
    printf("[PARK] Uruchamianie kasy restauracji...\n");
    pid_t pid = fork();
    if (pid == -1) {
        PRINT_ERROR("fork - kasa restauracji");
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

    for (int i=0; i < LICZBA_ATRAKCJI; i++) {
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
    //printf("Otrzymano sygnal totalnej ewakuacji i zamkniecia (3) sygnał %d!\n", sig);
    signal3 = 1;

}
void zakoncz_pracownikow() {

    printf("[PARK] Czekam na zakonczenie pracowników...\n");
    for (size_t i = 0; i <  pracownicy_pids.size(); i++) {
        int status;
        pid_t pid = waitpid(pracownicy_pids[i], &status, 0);
        if (pid > 0) {
            printf("[PARK] Pracownik %zu (PID: %d) zakonczył pracę\n", i, pid);
        }
    }

    printf("[PARK] Usuwam kolejki pracowników...\n");
    for (int i = 0; i < LICZBA_ATRAKCJI; i++) {
        int kolejka_id = g_park->pracownicy_keys[i];
        if (kolejka_id > 0) {
            if (msgctl(kolejka_id, IPC_RMID, NULL) == -1) {
                PRINT_ERROR("msgctl IPC_RMID podczas zakończenia pracowników");
            } else {
                printf("[PARK] Usunięto kolejkę atrakcji %d\n", i);
            }
            wait_semaphore(g_park->park_sem,0,0);
            g_park->pracownicy_keys[i] = -1;  // Oznacz jako nieważną
            signal_semaphore(g_park->park_sem,0);
        }
    }

}

int   main() {

    init_random();

    // Obsługa sygnału ewakuacji
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig3handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        PRINT_ERROR("sigaction SIGINT");
        exit(1);
    }

    printf("========================================\n");
    printf("   PARK ROZRYWKI - SYMULACJA          \n");
    printf("========================================\n\n");

    const int memfile = open(SEED_FILENAME_PARK, O_CREAT | O_RDONLY, 0666);
    error_check(memfile, "open");
    close(memfile);

    const int semfile = open(SEED_FILENAME_SEMAPHORES, O_CREAT | O_RDONLY, 0666);
    error_check(semfile, "open");
    close(semfile);

    const int queuefile = open(SEED_FILENAME_QUEUE, O_CREAT | O_RDONLY, 0666);
    error_check(queuefile, "open");
    close(queuefile);

    kasa_id = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED, 0655);
    kasa_rest_id = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED, 0644);

    g_park = attach_to_shared_block();
    memset(g_park, 0, sizeof(park_wspolne));

    g_park->park_otwarty = true;
    g_park->czas_w_symulacji.hour = CZAS_OTWARCIA;
    g_park->czas_w_symulacji.minute = 0;

    int park_sem_key = ftok(SEED_FILENAME_SEMAPHORES, SEM_SEED + 2);
    int park_sem = allocate_semaphore(park_sem_key, 1, 0600| IPC_CREAT | IPC_EXCL);
    initialize_semaphore(park_sem, 0, 1);

    for (int i = 0; i < LICZBA_ATRAKCJI; i++) {
        g_park->pracownicy_keys[i] = create_message_queue(SEED_FILENAME_QUEUE, i, 0600);
    }

    g_park->clients_count =  0;;
    g_park->park_sem = park_sem;

    uruchom_pracownikow();
    uruchom_kase();
    uruchom_kase_restauracji();

    int total_klientow = 0;

    while (true) {

        wait_semaphore(g_park->park_sem,0,0);
            int licznik_klientow = g_park->clients_count;
            bool otwarty = g_park->park_otwarty;
            SimTime curTime = g_park->czas_w_symulacji;

        signal_semaphore(g_park->park_sem,0);
        // Zamknij jeśli park zamknięty i brak klientów
        if (licznik_klientow == 0 && !otwarty) {
            break;
        }

        if (signal3) {
            printf("\n[PARK] *** EWAKUACJA - ZAMYKAM PARK ***\n");

            wait_semaphore(g_park->park_sem,0,0);
            g_park->park_otwarty = false;
            signal_semaphore(g_park->park_sem,0);

            break;
        }

        wait_semaphore(g_park->park_sem,0,0);
        g_park->czas_w_symulacji.increment_minute();

        if (g_park->czas_w_symulacji.minute == 0) {
            int ludzi = g_park->clients_count;
            printf("[PARK] %02d:00 - Klientów w parku: %d\n",
                   g_park->czas_w_symulacji.hour, ludzi);
            fflush(stdout);
        }

        if (g_park->czas_w_symulacji.hour >= CZAS_ZAMKNIECIA && g_park->park_otwarty) {
            g_park->park_otwarty = false;
            printf("[PARK] %02d:00 - PARK ZAMKNIĘTY (nie wpuszczamy nowych klientów)\n",
                   g_park->czas_w_symulacji.hour);
            fflush(stdout);
        }

        otwarty = g_park->park_otwarty;

        signal_semaphore(g_park->park_sem,0);


        if (random_chance(40) && otwarty && !signal3) {
            pid_t pid = fork();
            if (pid == 0) {
                // Proces klienta
                char* args[] = {(char*)"klient", NULL};
                execvp("./klient", args);
                perror("execvp - klient");
                _exit(1);
            } else if (pid > 0) {
                total_klientow++;
            }
        }

        usleep(10000);
    }

    printf("\n[PARK] Zamykam park...\n");
    zakoncz_pracownikow();
    poczekaj_na_kasy();
    wait_semaphore(g_park->park_sem,0,0);
    g_park->park_otwarty = false;
    signal_semaphore(g_park->park_sem,0);
    printf("[PARK] Zbieranie pozostałych procesów...\n");
    signal(SIGTERM, SIG_IGN);
    kill(0, SIGTERM); // Sends signal to all processes in the current PGID
    int status;
    while (wait(&status) > 0) {
    }
    printf("Usuwam kolejki komunikatów...\n");
    if (msgctl(kasa_rest_id, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID restauracja");
    }
    if (msgctl(kasa_id, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID kasa");
    }


    printf("PARK ZAMKNIETY");
    printf("Sprzątanie zasobów...\n");
    free_semaphore(g_park->park_sem, 0);

    detach_from_shared_block(g_park);
    destroy_shared_block((char*)SEED_FILENAME_PARK);
    printf(" Koniec symulacji\n");
    return 0;
}
