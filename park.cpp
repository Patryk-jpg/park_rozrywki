//
// Created by janik on 12/12/2025.
//
#include "park_wspolne.h"
park_wspolne* g_park = nullptr;

int   main() {
    printf("========================================\n");
    printf("   PARK ROZRYWKI - SYMULACJA          \n");
    printf("========================================\n\n");
    srand(time(NULL));

    int fd = open(SEED_FILENAME, O_CREAT | O_RDONLY, 0666);
    close(fd);
    g_park = attach_to_shared_block();
    memset(g_park, 0, sizeof(park_wspolne));
    g_park->pid_parku = getpid();
    g_park->park_otwarty = true;
    g_park->czas_w_symulacji.hour = CZAS_OTWARCIA;
    g_park->czas_w_symulacji.minute = 0;
    g_park->uruchom_pracownikow();
    //g_park->uruchom_kase();
    //g_park->uruchom_kase_restauracji();
    signal(SIGINT, handler_zamknij_park);
    while (g_park->park_otwarty) {
        g_park->czas_w_symulacji.increment_minute();
        if (g_park->czas_w_symulacji.hour == CZAS_ZAMKNIECIA) {
            handler_zamknij_park(SIGINT);
        }
        if (rand() % 100 > 40) {
            pid_t pid = fork();
            if (pid == 0) {
                char* args[] = { (char*)"klient", NULL };
                execvp("./klient", args);
                perror("execvp-klient");
            }
        }
    }

    printf("PARK ZAMKNIETY");
}
