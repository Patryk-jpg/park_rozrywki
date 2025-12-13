//
// Created by janik on 11/12/2025.
//

#include "klient.h"

#include <unistd.h>

#include "park_wspolne.h"
park_wspolne* g_park = nullptr;
static klient g_klient;


int main(int argc, char* argv[]) {


    srand(time(NULL));
    g_park = attach_to_shared_block();
    if (rand()%100 ==1) {
        g_klient.czyVIP=true;
    }
    g_klient.wiek = 1 + rand()%90;
    g_klient.wzrost = 50 + rand()%151;
    g_klient.pidKlienta = getpid();

    printf("Klient: %d, wzrost: %d, wiek: %d, utworzono, %02d:%02d\n",
        g_klient.pidKlienta,
        g_klient.wzrost,
        g_klient.wiek,
        g_park->czas_w_symulacji.hour,
        g_park->czas_w_symulacji.minute);
    wejdz_do_parku();

    fflush(stdout);
    detach_from_shared_block(g_park);

}
void wejdz_do_parku() {
    int semId = ftok(SEED_FILENAME_SEMAPHORES, SEM_SEED);

    klient_message k_msg;
    serwer_message reply;
    int kasaId = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    typ_biletu bilet;
    k_msg.mtype = 5;
    k_msg.typ_biletu =  rand() % 4;
    if (g_klient.czyVIP == true) {
        k_msg.mtype = 1;
        k_msg.typ_biletu = 4;
    }
    k_msg.ilosc_biletow = 1;
    k_msg.pid_klienta = g_klient.pidKlienta;
    printf("Klient %d - zapisuje sie do kolejki", g_klient.pidKlienta);
    msgsnd(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), 0);

    msgrcv(kasaId, &reply, sizeof(reply) - sizeof(long), g_klient.pidKlienta, 0);

    printf("Klient %d wychodzi z kolejki",g_klient.pidKlienta);
    if (reply.status == -1) {
        printf("Nie udalo sie wejsc do parku, klient %d ucieka", g_klient.pidKlienta);
        return;
    }
    g_klient.czasWejscia = reply.start_biletu;
    g_klient.cena = reply.cena;
    printf("Klient %d w parku, Ilosc ludzi w parku: %d ",g_klient.pidKlienta, read_semaphore(semId,1));


}
