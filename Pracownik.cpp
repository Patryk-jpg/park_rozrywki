//
// Created by janik on 11/12/2025.
//

#include "Pracownik.h"
#include <park_wspolne.h>
#include <cstdio>
#include <unistd.h>


int main(int nr_atrakcji) {

    printf("[PRACOWNIK-%d] Start obsługi atrakcji: %s (PID: %d)\n",
         nr_atrakcji, atrakcje[nr_atrakcji].nazwa, getpid());

}
