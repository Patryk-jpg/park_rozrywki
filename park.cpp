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
    g_park->liczba_klientow_w_parku  = 0;
    g_park->park_otwarty = true;
    g_park->czas_w_symulacji.hour = CZAS_OTWARCIA;
    g_park->czas_w_symulacji.minute = 0;
    g_park->uruchom_pracownikow();
    g_park->uruchom_kase();
    //g_park->uruchom_kase_restauracji();
    //signal(SIGINT, handler_zamknij_park);
    while (g_park->park_otwarty) {
        usleep(100000);
        g_park->czas_w_symulacji.increment_minute();
        //g_park->czas_w_symulacji.print();
        if (g_park->czas_w_symulacji.hour == CZAS_ZAMKNIECIA) {
            g_park->park_otwarty = false;
        }
        if (rand() % 100 > 40 ) {
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
    detach_from_shared_block(g_park);
    destroy_shared_block((char*)SEED_FILENAME_PARK);

}
