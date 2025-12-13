//
// Created by janik on 11/12/2025.
//

#include "Kasa.h"
#include "park_wspolne.h"

park_wspolne* g_park = nullptr;

int main(int argc, char *argv[]) {
    g_park = attach_to_shared_block();
    int kasaId = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    int semId = ftok(SEED_FILENAME_SEMAPHORES, SEM_SEED);
    int licznik_klientow = allocate_semaphore(semId, 1, 0666| IPC_CREAT);
    initialize_semaphore(licznik_klientow, 1, MAX_KLIENTOW_W_PARKU);
    while (g_park->park_otwarty) {
        klient_message request;
        serwer_message reply;
        ssize_t n = msgrcv(kasaId, &request, sizeof(request) - sizeof(long), -10, IPC_NOWAIT);
        if (n == -1) {
            usleep(1000);
            continue;
        }
        reply.mtype = request.pid_klienta;
        reply.cena = request.ilosc_biletow * bilety[request.typ_biletu].cena;
        reply.start_biletu = g_park->czas_w_symulacji;
        reply.end_biletu = g_park->czas_w_symulacji + SimTime(bilety[request.typ_biletu].czasTrwania,0);
        reply.status = -1;
        wait_semaphore(semId, 1,0);
        if (g_park->park_otwarty) {
            reply.status = 0;
        }
        msgsnd(kasaId, &reply, sizeof(reply) - sizeof(long), 0);
    }

}