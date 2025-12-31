
#include "Kasa.h"

#include <map>

#include "park_wspolne.h"

park_wspolne* g_park = nullptr;
static volatile sig_atomic_t ewakuacja = 0;
void sig3handler(int sig) {
    ewakuacja = 1;
}
int update_licznik_klientow(klient_message& request) {
    wait_semaphore(g_park->park_sem,0,0);
    int zajete = 0;
    for (int i = 0; i < request.ilosc_osob; i++) {
        if (wait_semaphore(g_park->licznik_klientow, 0, IPC_NOWAIT) == -1) {
            break;
        }
        zajete++;
    }

    if (zajete < request.ilosc_osob || !g_park->park_otwarty) {
        for (int i = 0; i < zajete; i++) {
            signal_semaphore(g_park->licznik_klientow, 0);
        }
        signal_semaphore(g_park->park_sem,0);
        return -1;
    }else {
        signal_semaphore(g_park->park_sem,0);
        return  0;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN);
    struct sigaction sa_int{};
    sa_int.sa_handler = sig3handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGUSR1, &sa_int, nullptr);
    float zarobki = 0.0f;
    int transakcje = 0;
    printf("KASA CZYNNA  PID: %d", getpid() );
    fflush(stdout);
    g_park = attach_to_shared_block();
    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    std::map<pid_t, serwer_message> clients_pids;
    initialize_semaphore(g_park->licznik_klientow, 0, MAX_KLIENTOW_W_PARKU);
    struct msqid_ds buf;
    msgctl(kasaId, IPC_STAT, &buf);

    while (1) {
        if (ewakuacja) {
            break;
        }
        msgctl(kasaId, IPC_STAT, &buf);
        wait_semaphore(g_park->park_sem, 0, 0);
            int klienci_w_parku = MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0);
            bool otwarty = g_park->park_otwarty;
        signal_semaphore(g_park->park_sem, 0);

        if (!otwarty && klienci_w_parku == 0 && buf.msg_qnum == 0) {
            break;
        }
        klient_message request{};
        serwer_message reply{};
        payment_message payment_request{};

        size_t wychodzacy = msgrcv(kasaId, &payment_request, sizeof(payment_request) - sizeof(long), 101, IPC_NOWAIT);

        if (wychodzacy != -1) {

            float total = 0.0f;
            float koszt_r = 0.0f;
            float doplata = 0.0f;
            float kwPodstawowa = clients_pids[payment_request.pid].cena;
            koszt_r = oblicz_koszt_restauracji(payment_request.czasWRestauracji) *  clients_pids[payment_request.pid].ilosc_osob;
            bool czyVIP = (clients_pids[payment_request.pid].typ_biletu == 4);

            if (!czyVIP) {
                wait_semaphore(g_park->park_sem, 0, 0);
                    int czasNadmiarowy = g_park->czas_w_symulacji.toMinutes() - clients_pids[payment_request.pid].end_biletu.toMinutes();
                signal_semaphore(g_park->park_sem, 0);
                if (czasNadmiarowy > 0) {
                    float cenaZaNadmiarowaMinute = ((float)kwPodstawowa / (bilety[clients_pids[payment_request.pid].typ_biletu].czasTrwania * 60.0f)) * 2.0f;
                    doplata =  cenaZaNadmiarowaMinute * czasNadmiarowy;
                }
            }
            if (payment_request.wiekDziecka > 2) {
                kwPodstawowa  = kwPodstawowa * clients_pids[payment_request.pid].ilosc_osob;
                doplata = doplata * clients_pids[payment_request.pid].ilosc_osob;
            }
            total = kwPodstawowa + koszt_r + doplata;
            printf("PARAGON - Klient %d\n", payment_request.pid);
            printf("========================================\n");
            printf("Koszt biletu: %.2f zł Osób: %d \n", kwPodstawowa, clients_pids[payment_request.pid].ilosc_osob);
            printf("Koszt restauracji:   %.2f zł (%d min)\n",
            koszt_r, payment_request.czasWRestauracji);
            wait_semaphore(g_park->park_sem, 0, 0);
                 int czasNadmiarowy = g_park->czas_w_symulacji.toMinutes() - clients_pids[payment_request.pid].end_biletu.toMinutes();
            signal_semaphore(g_park->park_sem, 0);
            printf("Dopłata za czas:     %.2f zł (%d min)\n", doplata, czasNadmiarowy);
            printf("----------------------------------------\n");
            printf("SUMA:                %.2f zł\n", total);
            printf("========================================\n");
            fflush(stdout);
            zarobki += total;
            transakcje++;
            payment_request.mtype = payment_request.pid;
            payment_request.suma = total;
            msgsnd(kasaId, &payment_request, sizeof(payment_request) - sizeof(long), 0);

            clients_pids.erase(payment_request.pid);


        }

        size_t n = msgrcv(kasaId, &request, sizeof(request) - sizeof(long), -10, IPC_NOWAIT);
        if (n == -1) {
            usleep(1000);
            continue;
        }
        reply.mtype = request.pid_klienta;
        reply.cena = request.ilosc_biletow * bilety[request.typ_biletu].cena;
        wait_semaphore(g_park->park_sem, 0, 0);
            reply.end_biletu = SimTime(g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute) + SimTime(bilety[request.typ_biletu].czasTrwania,0);
            reply.start_biletu = g_park->czas_w_symulacji;
        signal_semaphore(g_park->park_sem, 0);
        reply.typ_biletu = request.typ_biletu;
        reply.ilosc_osob = request.ilosc_osob;
        clients_pids[request.pid_klienta] = reply;
        reply.status = update_licznik_klientow(request);
        wait_semaphore(g_park->park_sem, 0, 0);
            if (!g_park->park_otwarty || ewakuacja) {
                reply.status = -1;
            }
        signal_semaphore(g_park->park_sem, 0);
        msgsnd(kasaId, &reply, sizeof(reply) - sizeof(long), 0);

    }

    printf("\n[KASA] Zamykam kasę\n");
    printf("========================================\n");
    printf("Podsumowanie dnia:\n");
    printf("Liczba transakcji: %d\n", transakcje);
    printf("Suma zarobków:     %.2f zł\n", zarobki);
    if (transakcje > 0) {
        printf("Średnia wartość:   %.2f zł\n", zarobki / transakcje);
    }
    printf("========================================\n");
    fflush(stdout);

    usleep(1000);



    detach_from_shared_block(g_park);
    return 0;

}