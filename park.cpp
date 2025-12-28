//
// Created by janik on 12/12/2025.
//
#include "park_wspolne.h"
#include <sys/wait.h>

park_wspolne* g_park = nullptr;

int   main() {
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
    int pracownicy_ftok_key = ftok(SEED_FILENAME_SEMAPHORES, 244);
    for (int i = 0; i < (sizeof(atrakcje) / sizeof(atrakcje[0]))-1; i++) {
        g_park->pracownicy_keys[i] = create_message_queue(SEED_FILENAME_QUEUE, i);
    }
    g_park->licznik_klientow = licznik_klientow;
    initialize_semaphore(g_park->licznik_klientow, 0, MAX_KLIENTOW_W_PARKU);

    g_park->uruchom_pracownikow();
    g_park->uruchom_kase();
    g_park->uruchom_kase_restauracji();
    //signal(SIGINT, handler_zamknij_park);
    while (g_park->park_otwarty || MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0) != 0) {
        usleep(50000);
        g_park->czas_w_symulacji.increment_minute();
        //g_park->czas_w_symulacji.print();
        //printf("Aktualni ludzie w parku %d, \n", MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0));
        fflush(stdout);
        if (g_park->czas_w_symulacji.hour == CZAS_ZAMKNIECIA) {

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
