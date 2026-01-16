
#include "park_wspolne.h"
#include <sys/wait.h>

// GLOBALNE
park_wspolne* g_park = nullptr;
std::vector<pid_t> pracownicy_pids;
std::vector<pid_t> klienci_pids;
pid_t kasa_pid = -1;
pid_t kasa_rest_pid = -1;

int kasa_rest_id =  -1;
int kasa_id = -1;
int kasa_reply_id = -1;
int kasa_rest_reply_id = -1;

pthread_t g_logger_tid;
int logger_id =  -1;
static volatile sig_atomic_t signal3 = 0;
bool testing = true;


void* watek_logger(void* arg) {
    printf("LOGGER - uruchomiony\n");
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd == -1) {
        perror("open failed");

        PRINT_ERROR("open raport");
        return NULL;
    }
    char header[] = "========================================\n"
                      "   RAPORT PARKU ROZRYWKI              \n"
                    "========================================\n\n";
    write(fd, header, strlen(header));
    LogMessage msg;
    while (true) {

        ssize_t result = msgrcv(logger_id, &msg,
                               sizeof(msg) - sizeof(long),
                               -5 ,0  ); //typ 1: logi typ 2: zakoncz

        if (result == -1) {
            if (errno == EINTR)
                continue;
            perror("msgrcv");
            break;
        }

        if (msg.mtype == 1) {
            write(fd, msg.message, strlen(msg.message)  );
        }
        else if (msg.mtype == 2) {
            write(fd, msg.message, strlen(msg.message) );
            break;
        }
        else {
            PRINT_ERROR("wrong mtype");
        }
    }
    close(fd);
    return nullptr;
}




void poczekaj_na_kasy() {
    log_message(logger_id,"[PARK] Czekam na zakończenie kas...\n");



    if (kasa_pid > 0) {
        kasa_message msg{0};

        int status;
        log_message(logger_id,"[PARK] Czekam na kasę główną (PID: %d)...\n", kasa_pid);
        printf("[PARK] Czekam na kasę główną (PID: %d)...\n", kasa_pid);

        msg.mtype = MSG_TYPE_END_QUEUE;
        msgsnd(kasa_id, &msg, sizeof(msg) - sizeof(long), 0);

        pid_t result = waitpid(kasa_pid, &status, 0);
        if (result > 0) {

            log_message(logger_id,"[PARK] Kasa główna zakończona\n");
            printf("[PARK] Kasa główna zakończona\n");
        } else {
            perror("waitpid kasa");
        }
    }
    if (kasa_rest_pid > 0) {
        int status;
        restauracja_message msg{0};
        msg.mtype = MSG_TYPE_END_QUEUE;
        msgsnd(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 0);

        log_message(logger_id,"[PARK] Czekam na kasę restauracji (PID: %d)...\n", kasa_rest_pid);
        printf("[PARK] Czekam na kasę restauracji (PID: %d)...\n", kasa_rest_pid);

        pid_t result = waitpid(kasa_rest_pid, &status, 0);
        if (result > 0) {
            log_message(logger_id,"[PARK] Kasa restauracji zakończona\n");
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
    log_message(logger_id,"[PARK] Uruchamianie kasy restauracji...\n");
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
    signal3 = 1;

}


void sigchld_handler(int i) {
    while (waitpid(-1, nullptr, WNOHANG) > 0) {
        // sprzątamy zombie
    }
};

void poczekaj_na_pracownikow() {

    log_message(logger_id,"[PARK] Czekam na zakonczenie pracowników...\n");
    for (size_t i = 0; i <  pracownicy_pids.size(); i++) {
        int status;
        if (pracownicy_pids[i] > 0) {
            kill(pracownicy_pids[i],SIGUSR1);
            pid_t pid = waitpid(pracownicy_pids[i], &status, 0);
            if (pid > 0) {

                printf("[PARK] Pracownik %zu (PID: %d) zakonczył pracę\n", i, pid);
            }
        }
    }

    log_message(logger_id,"[PARK] pracownicy zakończeni, usuwam kolejki pracowników...\n");
    for (int i = 0; i < LICZBA_ATRAKCJI + LICZBA_ATRAKCJI; i++) {
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

void poczekaj_na_klientow() {


    log_message(logger_id,"[PARK] Czekam na zakonczenie klientów...\n");
    for (size_t i = 0; i <  klienci_pids.size(); i++) {
        int status;
        // if (signal3) {
        //     kill(klienci_pids[i], SIGTERM);
        // }
        //pid_t pid = waitpid(klienci_pids[i], &status, 0); //TODO obsluga bledow do waitpida
        pid_t pid;
        while ((pid = waitpid(klienci_pids[i], &status, 0)) == -1 && errno == EINTR) {
            // przerwanie sygnałem, spróbuj ponownie
            continue;
        }

        if (pid == -1) {
            if (errno == ECHILD) {
                // Proces już zakończony i odebrany — ignorujemy
            } else {
                PRINT_ERROR("waitpid");
            }
        }

    }
    log_message(logger_id, "[PARK] - Klienci zakończeni");
};


void zakoncz() {
    wait_semaphore(g_park->park_sem,0,0);
    g_park->park_otwarty = false;
    signal_semaphore(g_park->park_sem,0);

    log_message(logger_id,"\n[PARK] Zamykam park...\n");
    printf("\n[PARK] Zamykam park...\n");
    poczekaj_na_klientow();
    poczekaj_na_pracownikow();
    poczekaj_na_kasy();

    log_message(logger_id,"[PARK] Zbieranie pozostałych procesów...\n");
    printf("[PARK] Zbieranie pozostałych procesów...\n");

    // signal(SIGTERM, SIG_IGN);
    // kill(0, SIGTERM); // Sends signal to all processes in the current PGID
    int status;
    while (wait(&status) > 0) {
    }
    log_message(logger_id,"Usuwam kolejki komunikatów...\n");
    if (msgctl(kasa_rest_id, IPC_RMID, NULL) == -1) {
        PRINT_ERROR("msgctl IPC_RMID restauracja");
    }
    if (msgctl(kasa_id, IPC_RMID, NULL) == -1) {
        PRINT_ERROR("msgctl IPC_RMID kasa");
    }
    if (msgctl(kasa_reply_id, IPC_RMID, NULL) == -1) {
        PRINT_ERROR("msgctl IPC_RMID kasa");
    }
    if (msgctl(kasa_rest_reply_id, IPC_RMID, NULL) == -1) {
        PRINT_ERROR("msgctl IPC_RMID kasa");
    }

    log_message(logger_id,"PARK ZAMKNIETY");
    printf("park zamkniety");
    log_message(logger_id,"Sprzątanie zasobów...\n");
    end_logger(logger_id);
    pthread_join(g_logger_tid, NULL);
    if (msgctl(logger_id, IPC_RMID, NULL) == -1) {
        PRINT_ERROR("msgctl IPC_RMID kasa");
    }
    free_semaphore(g_park->park_sem, 0);
    // free_semaphore(g_park->msg_overflow_sem, 0);
    detach_from_shared_block(g_park);
    destroy_shared_block((char*)SEED_FILENAME_PARK);
}

int   main() {

    init_random();

    struct sigaction sa_chld{};
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa_chld, nullptr) == -1) {
        PRINT_ERROR("sigaction SIGCHLD");
        exit(1);
    }
    // Obsługa sygnału ewakuacji
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig3handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    //signal(SIGCHLD, SIG_IGN);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        PRINT_ERROR("sigaction SIGINT");
        exit(1);
    }

    const int memfile = open(SEED_FILENAME_PARK, O_CREAT | O_RDONLY, 0666);
    error_check(memfile, "open");
    close(memfile);

    const int semfile = open(SEED_FILENAME_SEMAPHORES, O_CREAT | O_RDONLY, 0666);
    error_check(semfile, "open");
    close(semfile);

    const int queuefile = open(SEED_FILENAME_QUEUE, O_CREAT | O_RDONLY, 0666);
    error_check(queuefile, "open");
    close(queuefile);

    kasa_id = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED, 0600);
    kasa_rest_id = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED, 0600);
    logger_id = create_message_queue(SEED_FILENAME_QUEUE, 'L', 0600);
    kasa_reply_id =  create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED + 3, 0600);
    kasa_rest_reply_id   = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED + 4, 0600);

    g_park = attach_to_shared_block();
    memset(g_park, 0, sizeof(park_wspolne));

    g_park->park_otwarty = true;
    g_park->czas_w_symulacji.hour = CZAS_OTWARCIA;
    g_park->czas_w_symulacji.minute = 0;
    g_park->czas_w_symulacji.start_time = time(NULL);

    g_park->logger_id = logger_id;

    if (pthread_create(&g_logger_tid, nullptr, watek_logger, nullptr) != 0) {
        PRINT_ERROR("pthread_create logger");
        return 1;
    }

    int park_sem_key = ftok(SEED_FILENAME_SEMAPHORES, SEM_SEED + 2);
    // int msg_overflow_sem = allocate_semaphore(msg_overflow_sem_key, 1, 0600| IPC_CREAT | IPC_EXCL);
    int park_sem = allocate_semaphore(park_sem_key, 1, 0600| IPC_CREAT | IPC_EXCL);
    initialize_semaphore(park_sem, 0, 1);
    // initialize_semaphore(msg_overflow_sem, 0, 400);

    for (int i = 0; i < LICZBA_ATRAKCJI + LICZBA_ATRAKCJI; i++) {
        g_park->pracownicy_keys[i] = create_message_queue(SEED_FILENAME_QUEUE, i, 0600);
    }

    g_park->clients_count =  0;;
    g_park->park_sem = park_sem;
    g_park->kasa_reply_id = kasa_reply_id;
    g_park->kasa_rest_reply_id = kasa_rest_reply_id;
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
            log_message(logger_id,"\n[PARK] *** EWAKUACJA - ZAMYKAM PARK ***\n");

            wait_semaphore(g_park->park_sem,0,0);
            g_park->park_otwarty = false;
            signal_semaphore(g_park->park_sem,0);

            break;
        }

        wait_semaphore(g_park->park_sem,0,0);
        g_park->czas_w_symulacji.update();
        printf("time now %02d:%02d\n", curTime.hour, curTime.minute);


        if (g_park->czas_w_symulacji.minute == 0) {
            int ludzi = g_park->clients_count;
            log_message(logger_id,"[PARK] %02d:00 - Klientów w parku: %d\n",
                   g_park->czas_w_symulacji.hour, ludzi);
            fflush(stdout);
        }

        if (g_park->czas_w_symulacji.hour >= CZAS_ZAMKNIECIA && g_park->park_otwarty) {
            g_park->park_otwarty = false;
            log_message(logger_id,"[PARK] %02d:00 - PARK ZAMKNIĘTY (nie wpuszczamy nowych klientów)\n",
                   g_park->czas_w_symulacji.hour);
            fflush(stdout);
        }

        otwarty = g_park->park_otwarty;

        signal_semaphore(g_park->park_sem,0);


        // if (testing) {
        //     for (int i=0 ; i<200; i++) {
        //         pid_t pid = fork();
        //         if (pid == 0) {
        //             char arg[16];
        //             snprintf(arg, sizeof(arg), "%d", 0);
        //             char* args[] = {(char*)"klient", arg, NULL};
        //             execvp("./klient", args);
        //             perror("execvp - klient");
        //             _exit(1);
        //         }
        //         else if (pid > 0) {
        //             total_klientow++;
        //             klienci_pids.push_back(pid);
        //         }
        //     }
        //     usleep(1000);
        //     pid_t pid = fork();
        //     if (pid == 0) {
        //         char arg[16];
        //         snprintf(arg, sizeof(arg), "%d", 1);
        //         char* args[] = {(char*)"klient", arg, NULL};
        //         execvp("./klient", args);
        //         perror("execvp - klient");
        //         _exit(1);
        //     }
        //     else if (pid > 0) {
        //         total_klientow++;
        //         klienci_pids.push_back(pid);
        //     }
        //     testing = false;
        // }
        //
        if (random_chance(30) && otwarty && !signal3) {
            pid_t pid = fork();
            if (pid == -1) {
                zakoncz();
                // TODO czekamy na wszystkie procesy potomne, czyscimy struktury systemowe, wykonujemy exit
                perror("execvp - klient");
                _exit(1);
            }
            if (pid == 0) {
                //Proces klienta
                char* args[] = {(char*)"klient", NULL};
                execvp("./klient", args);
                perror("execvp - klient");
                _exit(1);
            } else if (pid > 0) {
                total_klientow++;
                klienci_pids.push_back(pid);
            }
        }

        //usleep(MINUTA);
    }

    zakoncz();
    return 0;
}
