//
// Created by x on 11/12/2025.
//

#include "Pracownik.h"
#include <park_wspolne.h>
#include <cstdio>
#include <unistd.h>


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Brak numeru atrakcji!\n");
        return 1;
    }
    int nr_atrakcji = atoi(argv[1]);
    if (nr_atrakcji < 0 || nr_atrakcji >= 17) {
        fprintf(stderr, "Niepoprawny numer atrakcji: %d\n", nr_atrakcji);
        _exit(1);
    }
    printf("[PRACOWNIK-%d] Start obsługi atrakcji: %s (PID: %d)\n",
         nr_atrakcji, atrakcje[nr_atrakcji].nazwa, getpid());
    fflush(stdout);
    _exit(0);

}
