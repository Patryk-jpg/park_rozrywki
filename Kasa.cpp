
#include "park_wspolne.h"
#include <map>

park_wspolne* g_park = nullptr;
static volatile sig_atomic_t ewakuacja = 0;

void sig3handler(int sig) {
    ewakuacja = 1;
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

    int zajete = 0;
    for (int i = 0; i < request.ilosc_osob; i++) {
        if (wait_semaphore(g_park->licznik_klientow, 0, IPC_NOWAIT) == -1) {
            // Nie ma miejsca - zwolnij już zajęte
            for (int j = 0; j < zajete; j++) {
                signal_semaphore(g_park->licznik_klientow, 0);
            }
            signal_semaphore(g_park->park_sem, 0);
            return -1;
        }
        zajete++;
    }
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
    }
    printf("----------------------------------------\n");
    printf("SUMA:               %7.2f zł\n", suma);
    printf("========================================\n\n");
    fflush(stdout);


}



int main(int argc, char *argv[]) {

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

    printf("KASA CZYNNA (PID: %d)\n", getpid());
    fflush(stdout);

    g_park = attach_to_shared_block();
    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);

    // Mapa przechowująca informacje o biletach klientów
    std::map<pid_t, serwer_message> clients_pids;

    // Inicjalizacja semafora licznika klientów
    initialize_semaphore(g_park->licznik_klientow, 0, MAX_KLIENTOW_W_PARKU);

    struct msqid_ds buf;
    msgctl(kasaId, IPC_STAT, &buf);

    while (true) {

        if (ewakuacja) {
            printf("Otrzymano sygnał ewakuacji, zamykam kasę\n");
            fflush(stdout);
            break;
        }

        msgctl(kasaId, IPC_STAT, &buf);
        wait_semaphore(g_park->park_sem, 0, 0);
        int klienci_w_parku = MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0);
        bool otwarty = g_park->park_otwarty;
        signal_semaphore(g_park->park_sem, 0);

        if (!otwarty && klienci_w_parku == 0 && buf.msg_qnum == 0) {
            printf("Park zamknięty, brak klientów, zamykam kasę\n");
            fflush(stdout);
            break;
        }

        payment_message payment_request{};
        ssize_t wychodzacy = msgrcv(kasaId, &payment_request,
                                    sizeof(payment_request) - sizeof(long),
                                    MSG_TYPE_EXIT_PAYMENT, IPC_NOWAIT);

        if (wychodzacy != -1) {
            if (clients_pids.find(payment_request.pid) == clients_pids.end()) {
                printf("BŁĄD: Brak danych biletu dla klienta %d\n", payment_request.pid);
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

            // Wyślij potwierdzenie
            payment_request.mtype = payment_request.pid;
            payment_request.suma = total;
            msgsnd(kasaId, &payment_request, sizeof(payment_request) - sizeof(long), 0);

            // Aktualizuj statystyki
            zarobki += total;
            transakcje++;

            // Usuń z mapy
            clients_pids.erase(payment_request.pid);

        }

        klient_message request{};
        serwer_message reply{};

        size_t n = msgrcv(kasaId, &request, sizeof(request) - sizeof(long), -10, IPC_NOWAIT);

        if (n == -1) {
            usleep(1000);
            continue;
        }

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
        if (!g_park->park_otwarty || ewakuacja) {
            reply.status = -1;
        }
        signal_semaphore(g_park->park_sem, 0);

        // Wyślij odpowiedź
        msgsnd(kasaId, &reply, sizeof(reply) - sizeof(long), 0);
        if (reply.status == 0) {
            clients_pids[request.pid_klienta] = reply;

            printf("Klient %d kupił bilet %s (%.2f zł), osób: %d, ważny do %02d:%02d\n",
                     request.pid_klienta, bilety[request.typ_biletu].nazwa,
                     reply.cena, request.ilosc_osob,
                     reply.end_biletu.hour, reply.end_biletu.minute);
        } else {
            printf("Klient %d ODRZUCONY (park pełny lub zamknięty)\n", request.pid_klienta);
        }
        fflush(stdout);
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


    usleep(100000);
    detach_from_shared_block(g_park);

    return 0;

}