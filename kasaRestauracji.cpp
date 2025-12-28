//
// Created by janik on 23/12/2025.
//

#include "kasaRestauracji.h"



int main(int argc, char* argv[]) {
    printf("[KASA RESTAURACJI] Uruchamianie...\n");
    fflush(stdout);
    init_random();
    g_park = attach_to_shared_block();

    int kasa_rest_id = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED);
    restauracja_message msg;
    int licznik_transakcji = 0;
    float suma_przychodow = 0.0f;
    struct msqid_ds buf;
    msgctl(kasa_rest_id, IPC_STAT, &buf);

    while (g_park->park_otwarty || buf.msg_qnum > 0) {
        usleep(50000);
        msgctl(kasa_rest_id, IPC_STAT, &buf);
        if (buf.msg_qnum == 0) {continue;}
        if (msgrcv(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT) == -1) {continue;}
        msg.kwota = oblicz_koszt_restauracji(msg.czas_pobytu_min);
        suma_przychodow += msg.kwota;
        licznik_transakcji++;
        printf("Klient %d płaci %.2f zł za %d min. (Transakcja %d, o godz  %02d:%02d)\n",
              msg.pid_klienta,
              msg.kwota,
              msg.czas_pobytu_min,
              licznik_transakcji,
              g_park->czas_w_symulacji.hour,
              g_park->czas_w_symulacji.minute);
        fflush(stdout);
        msg.mtype = msg.pid_klienta;
        msgsnd(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 0);
    }

    msgctl(kasa_rest_id, IPC_RMID, NULL);

    detach_from_shared_block(g_park);
    return 0;
}