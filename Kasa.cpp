
#include "park_wspolne.h"
#include <map>

park_wspolne* g_park = nullptr;
static volatile sig_atomic_t koniec = 0;
int logger_id = -1;
void sig3handler(int sig) {
    koniec = 1;
}

/**
 * Próbuje zarezerwować miejsca dla klientów w parku.
 * Zwraca 0 jeśli sukces, -1 jeśli park pełny lub zamknięty.
 */



int update_licznik_klientow(klient_message& request) {
    wait_semaphore(g_park->park_sem,0,0);

    if (!g_park->park_otwarty) {
        signal_semaphore(g_park->park_sem, 0);
        return -1;
    }
    if (g_park->clients_count + request.ilosc_osob > MAX_KLIENTOW_W_PARKU ) {
        signal_semaphore(g_park->park_sem, 0);
        return -1;
    }
    g_park->clients_count += request.ilosc_osob;
    signal_semaphore(g_park->park_sem, 0);
    return 0;
}

float oblicz_doplate(const serwer_message& bilet_info, int czas_nadmiarowy) {
    if (czas_nadmiarowy <= 0) return 0.0f;

    // Cena za minutę = (cena biletu / czas biletu w min) * 2 (100% dopłata)
    int czas_biletu_min = bilety[bilet_info.typ_biletu].czasTrwania * 60;
    float cena_za_minute = (bilet_info.cena / (float)czas_biletu_min) * 2.0f;

    return cena_za_minute * czas_nadmiarowy;

}

void wydrukuj_paragon(pid_t pid, const serwer_message& bilet, const payment_message& payment) {
    float kwota_bilet = bilet.cena;
    float koszt_restauracji = oblicz_koszt_restauracji(payment.czasWRestauracji);
    koszt_restauracji *= bilet.ilosc_osob;

    float doplata = 0.0f;
    bool czyVIP = (bilet.typ_biletu == BILETVIP);

    // VIP nie płaci dopłaty za czas
    if (!czyVIP) {
        wait_semaphore(g_park->park_sem, 0, 0);
        int czas_nadmiarowy = g_park->czas_w_symulacji.toMinutes() - bilet.end_biletu.toMinutes();
        signal_semaphore(g_park->park_sem, 0);

        if (czas_nadmiarowy > 0) {
            doplata = oblicz_doplate(bilet, czas_nadmiarowy) * bilet.ilosc_osob;
        }
    }

    // Dzieci <2 lata nie płacą za bilet
    if (payment.wiekDziecka > 0 && payment.wiekDziecka < 2) {
        kwota_bilet = 0.0f;
    }

    float suma = kwota_bilet + koszt_restauracji + doplata;

    printf("\n========================================\n");
    printf("           PARAGON - Klient %d\n", pid);
    printf("========================================\n");
    printf("Koszt biletu:       %7.2f zł (x%d osób)\n", kwota_bilet, bilet.ilosc_osob);
    printf("Koszt restauracji:  %7.2f zł (%d min)\n", koszt_restauracji, payment.czasWRestauracji);

    wait_semaphore(g_park->park_sem, 0, 0);
    int czas_nadmiarowy = g_park->czas_w_symulacji.toMinutes() - bilet.end_biletu.toMinutes();
    signal_semaphore(g_park->park_sem, 0);

    if (czas_nadmiarowy > 0) {
        printf("Dopłata za czas:    %7.2f zł (%d min)\n", doplata, czas_nadmiarowy);
    }else {
        czas_nadmiarowy = 0;
    }
    printf("----------------------------------------\n");
    printf("SUMA:               %7.2f zł\n", suma);
    printf("========================================\n\n");
    fflush(stdout);
    log_message(logger_id, "[KASA] - Klient %d , osób: %d koszt:(B+R+C|T):(%7.2f+%7.2f+%7.2f|%7.2f) czas w restauracji:%d, nadmiarowy %d\n", pid, bilet.ilosc_osob,kwota_bilet,koszt_restauracji,
        doplata, suma, payment.czasWRestauracji, czas_nadmiarowy );

}



int main(int argc, char *argv[]) {
    g_park = attach_to_shared_block();
    logger_id = g_park->logger_id;

    // Ignoruj SIGINT (obsługuje go Kierownik)
    signal(SIGINT, SIG_IGN);

    // Obsługa sygnału zamknięcia od Kierownika
    struct sigaction sa_int{};
    sa_int.sa_handler = sig3handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGUSR1, &sa_int, nullptr);

    // Inicjalizacja
    float zarobki = 0.0f;
    int transakcje = 0;

    log_message(logger_id,"[KASA] - CZYNNA (PID: %d)\n", getpid());
    fflush(stdout);



    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);

    // Mapa przechowująca informacje o biletach klientów
    std::map<pid_t, serwer_message> clients_pids;

    struct msqid_ds buf;
    msgctl(kasaId, IPC_STAT, &buf);

    while (true) {

        if (koniec) {
            log_message(logger_id,"[KASA] - Otrzymano sygnał zakończenia, zamykam kasę\n");
            fflush(stdout);
            break;
        }



        payment_message payment_request{};
        // ssize_t wychodzacy = msgrcv(kasaId, &payment_request,
        //                             sizeof(payment_request) - sizeof(long),
        //                             MSG_TYPE_EXIT_PAYMENT, IPC_NOWAIT);

        while (msgrcv(kasaId, &payment_request, sizeof(payment_request) - sizeof(long),MSG_TYPE_EXIT_PAYMENT, IPC_NOWAIT) != -1) {
            if (clients_pids.find(payment_request.pid) == clients_pids.end()) {
                log_message(logger_id,"[KASA] - BŁĄD: Brak danych biletu dla klienta %d\n", payment_request.pid);
                continue;
            }
            wydrukuj_paragon(payment_request.pid, clients_pids[payment_request.pid], payment_request);

            float total = 0.0f;
            const serwer_message& bilet = clients_pids[payment_request.pid];

            float kwota_bilet = bilet.cena;
            if (payment_request.wiekDziecka > 0 && payment_request.wiekDziecka < 2) {
                kwota_bilet = 0.0f;
            }
            float koszt_rest = oblicz_koszt_restauracji(payment_request.czasWRestauracji) * bilet.ilosc_osob;

            float doplata = 0.0f;
            if (bilet.typ_biletu != BILETVIP) {
                wait_semaphore(g_park->park_sem, 0, 0);
                int czas_nadmiarowy = g_park->czas_w_symulacji.toMinutes() - bilet.end_biletu.toMinutes();
                signal_semaphore(g_park->park_sem, 0);

                if (czas_nadmiarowy > 0) {
                    doplata = oblicz_doplate(bilet, czas_nadmiarowy) * bilet.ilosc_osob;
                }
            }
            total = kwota_bilet + koszt_rest + doplata;

            payment_request.mtype = payment_request.pid;
            payment_request.suma = total;

            // daj paragon

            msgsnd(kasaId, &payment_request, sizeof(payment_request) - sizeof(long), 0);


            // Aktualizuj statystyki
            zarobki += total;
            transakcje++;

            //zmniejsz licznik
            wait_semaphore(g_park->park_sem, 0, 0);
            g_park->clients_count -= clients_pids[payment_request.pid].ilosc_osob;
            signal_semaphore(g_park->park_sem, 0);

            // Usuń z mapy

            clients_pids.erase(payment_request.pid);



        }

        klient_message request{};
        serwer_message reply{};


        //log_message(logger_id,"KASA widzi %d klientow w parku", klienci_w_parku);
        // USUWAJ PARAGONY


        // WPUSZAJ DO PARKU
        //size_t n =
        while (msgrcv(kasaId, &request, sizeof(request) - sizeof(long), -10, IPC_NOWAIT) != -1) {
            reply.mtype = request.pid_klienta;
            reply.typ_biletu = request.typ_biletu;
            reply.ilosc_osob = request.ilosc_osob;

            // Oblicz cenę (VIP = 0)
            if (request.typ_biletu == BILETVIP) {
                reply.cena = 0.0f;
            } else {
                reply.cena = request.ilosc_biletow * bilety[request.typ_biletu].cena;
            }

            // Oblicz czas biletu
            wait_semaphore(g_park->park_sem, 0, 0);
            reply.start_biletu = g_park->czas_w_symulacji;
            int czas_trwania_h = bilety[request.typ_biletu].czasTrwania;
            reply.end_biletu = reply.start_biletu + SimTime(czas_trwania_h, 0);
            signal_semaphore(g_park->park_sem, 0);

            // Spróbuj zarezerwować miejsca
            reply.status = update_licznik_klientow(request);

            // Sprawdź czy park nadal otwarty
            wait_semaphore(g_park->park_sem, 0, 0);
            if (!g_park->park_otwarty || koniec) {
                reply.status = -1;
            }
            signal_semaphore(g_park->park_sem, 0);

            // Wyślij odpowiedź
            msgsnd(kasaId, &reply, sizeof(reply) - sizeof(long), 0);
            if (reply.status == 0) {
                clients_pids[request.pid_klienta] = reply;
                wait_semaphore(g_park->park_sem, 0, 0);
                SimTime curtime = g_park->czas_w_symulacji;
                signal_semaphore(g_park->park_sem, 0);
                log_message(logger_id,"[KASA] - %02d:%02d - Klient %d kupił bilet %s (%.2f zł), osób: %d, ważny do %02d:%02d\n",curtime.hour,curtime.minute,
                         request.pid_klienta, bilety[request.typ_biletu].nazwa,
                         reply.cena, request.ilosc_osob,
                         reply.end_biletu.hour, reply.end_biletu.minute);
            } else {
                log_message(logger_id,"[KASA] - Klient %d ODRZUCONY (park pełny lub zamknięty)\n", request.pid_klienta);
            }
            fflush(stdout);
        }

        msgctl(kasaId, IPC_STAT, &buf);
        wait_semaphore(g_park->park_sem, 0, 0);
        int klienci_w_parku = g_park->clients_count;
        bool otwarty = g_park->park_otwarty;
        signal_semaphore(g_park->park_sem, 0);

        // if (!otwarty && klienci_w_parku == 0) {
        //     log_message(logger_id,"[KASA] - Park zamknięty, brak klientów, zamykam kasę\n");
        //     fflush(stdout);
        //     break;
        // }
        usleep(1000);

    }

    log_message(logger_id,"[KASA] Zamykam kase -Liczba transakcji: %d,Suma zarobków: %.2f zł\n",transakcje,zarobki);

    fflush(stdout);


    usleep(100000);
    detach_from_shared_block(g_park);

    return 0;

}