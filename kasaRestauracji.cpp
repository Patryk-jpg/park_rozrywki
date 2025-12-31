

#include "kasaRestauracji.h"
static volatile sig_atomic_t ewakuacja = 0;
void sig3handler(int sig) {
    ewakuacja = 1;
}

int main(int argc, char* argv[]) {
    struct sigaction sa_int{};
    sa_int.sa_handler = sig3handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, nullptr);
    printf("[KASA RESTAURACJI] Uruchamianie...\n");
    fflush(stdout);
    init_random();
    g_park = attach_to_shared_block();
    //signal(SIGINT, SIG_IGN);

    int kasa_rest_id = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED);
    restauracja_message msg;
    int licznik_transakcji = 0;
    float suma_przychodow = 0.0f;
    struct msqid_ds buf;
    msgctl(kasa_rest_id, IPC_STAT, &buf);

    while (1) {
        if (ewakuacja) {
            break;
        }
        msgctl(kasa_rest_id, IPC_STAT, &buf);

        wait_semaphore(g_park->park_sem,0,0);
        if (!g_park->park_otwarty && buf.msg_qnum == 0) {
            signal_semaphore(g_park->park_sem,0);
            break;  // park zamknięty i kolejka pusta
        }
        signal_semaphore(g_park->park_sem,0);

        if (buf.msg_qnum == 0) {continue;}
        if (msgrcv(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT) == -1) {
            usleep(10000);
            continue;
        }
        msg.kwota = oblicz_koszt_restauracji(msg.czas_pobytu_min);
        suma_przychodow += msg.kwota;
        licznik_transakcji++;
        wait_semaphore(g_park->park_sem,0,0);
            printf("Klient %d płaci %.2f zł za %d min. (Transakcja %d, o godz  %02d:%02d)\n",
                  msg.pid_klienta,
                  msg.kwota,
                  msg.czas_pobytu_min,
                  licznik_transakcji,
                  g_park->czas_w_symulacji.hour,
                  g_park->czas_w_symulacji.minute);
            fflush(stdout);
        signal_semaphore(g_park->park_sem,0);
        msg.mtype = msg.pid_klienta;
        msgsnd(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 0);

    }

    printf("\n[KASA RESTAURACJI] Zamykam kasę\n");
    printf("========================================\n");
    printf("Podsumowanie dnia:\n");
    printf("Liczba transakcji: %d\n", licznik_transakcji);
    printf("Suma przychodów:   %.2f zł\n", suma_przychodow);
    if (licznik_transakcji > 0) {
        printf("Średnia wartość:   %.2f zł\n", suma_przychodow / licznik_transakcji);
    }
    printf("========================================\n");
    fflush(stdout);


    detach_from_shared_block(g_park);

    printf("Zakończono kase restauracji (PID: %d)\n", getpid());
    return 0;
}