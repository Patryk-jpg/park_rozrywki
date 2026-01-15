

#include "kasaRestauracji.h"

park_wspolne* g_park = nullptr;
static volatile sig_atomic_t zakonczenie = 0;
int logger_id = -1;
void sig3handler(int sig) {
    zakonczenie = 1;
}

int main(int argc, char* argv[]) {

    g_park = attach_to_shared_block();
    logger_id = g_park->logger_id;

    signal(SIGINT, SIG_IGN);
    struct sigaction sa_int{};
    sa_int.sa_handler = sig3handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGUSR1, &sa_int, nullptr);

    log_message(logger_id,"[KASA RESTAURACJI] Uruchomiona (PID: %d)\n", getpid());
    fflush(stdout);

    init_random();

    int kasa_rest_id = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED);

    int licznik_transakcji = 0;
    float suma_przychodow = 0.0f;

    struct msqid_ds buf;
    msgctl(kasa_rest_id, IPC_STAT, &buf);

    while (true) {
        if (zakonczenie) {
            log_message(logger_id,"Otrzymano sygnał zakonczenia, zamykam kasę restauracji\n");
            break;
        }
        msgctl(kasa_rest_id, IPC_STAT, &buf);

        restauracja_message msg;
        ssize_t result = msgrcv(kasa_rest_id, &msg,
                                sizeof(msg) - sizeof(long),
                                -MSG_TYPE_END_QUEUE, 0);

        if (result == -1) {
            //usleeep(10000);
            continue;
        }
        if (msg.mtype == MSG_TYPE_END_QUEUE) {
            break; //zakoncz
        }
        msg.kwota = oblicz_koszt_restauracji(msg.czas_pobytu_min);

        suma_przychodow += msg.kwota;
        licznik_transakcji++;

        wait_semaphore(g_park->park_sem, 0, 0);
        SimTime curTime = g_park->czas_w_symulacji;
        signal_semaphore(g_park->park_sem, 0);

        log_message(logger_id,"[RESTAURACJA] %02d:%02d - Klient %d: %.2f zł za %d min\n",
               curTime.hour, curTime.minute,
               msg.pid_klienta, msg.kwota, msg.czas_pobytu_min);
        fflush(stdout);

        msg.mtype = msg.pid_klienta;
        msgsnd(g_park->kasa_rest_reply_id, &msg, sizeof(msg) - sizeof(long), 0);

    }

    log_message(logger_id,"[RESTAURACJA] Zamykam kase -Liczba transakcji: %d,Suma zarobków: %.2f zł\n",licznik_transakcji,suma_przychodow);


    detach_from_shared_block(g_park);

    return 0;
}