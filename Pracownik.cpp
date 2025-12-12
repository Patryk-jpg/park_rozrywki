//
// Created by janik on 11/12/2025.
//

#include "Pracownik.h"
#include <park_wspolne.h>
#include <cstdio>
#include <unistd.h>


int main(int argc, char* argv[]) {

    printf("[PRACOWNIK-%d] Start obsługi atrakcji: %s (PID: %d)\n",
         *argv[1], atrakcje[(int)*argv[1]].nazwa, getpid());

}
