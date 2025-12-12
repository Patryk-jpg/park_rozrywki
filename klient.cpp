//
// Created by janik on 11/12/2025.
//

#include "klient.h"

#include <unistd.h>

#include "park_wspolne.h"
park_wspolne* g_park = nullptr;
static klient* g_klient;

int main() {
    srand(time(NULL));
    g_park = attach_to_shared_block();
    if (rand()%100 ==1) {
        g_klient->czyVIP=true;
    }
    g_klient->wiek = 1 + rand()%90;
    g_klient->wzrost = 50 + rand()%151;
    g_klient->pidKlienta = getpid();

    printf("Klient: %d, wzrost: %d, wiek: %d, utworzono, %02d:%02d\n",
        g_klient->pidKlienta,
        g_klient->wzrost,
        g_klient->wiek,
        g_park->czas_w_symulacji.hour,
        g_park->czas_w_symulacji.minute);

    detach_from_shared_block(g_park);

}