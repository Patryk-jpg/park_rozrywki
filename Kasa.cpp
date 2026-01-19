
#include "park_wspolne.h"
#include <map>
#include <queue>

park_wspolne* g_park = nullptr;
static volatile sig_atomic_t koniec = 0;
int logger_id = -1;
int kasaId = -1;
float zarobki = 0.0f;
int transakcje = 0;
std::map<pid_t, serwer_message> clients_pids;
std::queue<klient_message> oczekujacy;
std::queue<klient_message> oczekujacy_vip;

void sig3handler(int sig) {
    koniec = 1;
}

/**
 * Próbuje zarezerwować miejsca dla klientów w parku.
 * Zwraca 0 jeśli sukces, -1 jeśli park zamknięty i 1 jeśli pełny ale dalej otwarty.
 */



int update_licznik_klientow(klient_message& request) {
    wait_semaphore(g_park->park_sem,0,0);

    if (!g_park->park_otwarty) {
        signal_semaphore(g_park->park_sem, 0);
        return -1;
    }
    if (g_park->clients_count + request.ilosc_osob > MAX_KLIENTOW_W_PARKU ) {
        signal_semaphore(g_park->park_sem, 0);
        log_message(3, logger_id,"[KASA] [TEST-1] -  klient %d nie wchodzi bo ilosć klientów, kasa powiadomi go kiedy moze wejsc: %d\n",request.pid_klienta, g_park->clients_count );
        return 1;
    }
    g_park->clients_count += request.ilosc_osob;
    signal_semaphore(g_park->park_sem, 0);
    return 0;
}

float oblicz_doplate(const serwer_message& bilet_info, int czas_nadmiarowy) {
    if (czas_nadmiarowy <= 0) return 0.0f;

    // Cena za minutę = (cena biletu / czas biletu w min) * 2 (100% dopłata)
    int czas_biletu_min = bilety[bilet_info.typ_biletu].czasTrwania * 60;
    float cena_za_minute = (bilety[bilet_info.typ_biletu].cena / (float)czas_biletu_min) * 2.0f;

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
    // log_message(3, logger_id, "[KASA] - Klient %d , osób: %d koszt:(B+R+C|T):(%7.2f+%7.2f+%7.2f|%7.2f) czas w restauracji:%d, nadmiarowy %d\n", pid, bilet.ilosc_osob,kwota_bilet,koszt_restauracji,
    //     doplata, suma, payment.czasWRestauracji, czas_nadmiarowy );

}


void handle_enter(klient_message request) {
            kasa_message reply{0};
            reply.serwer.typ_biletu = request.typ_biletu;
            reply.serwer.ilosc_osob = request.ilosc_osob;
            reply.mtype = request.pid_klienta;
            // Oblicz cenę (VIP = 0)
            if (request.typ_biletu == BILETVIP) {
                reply.serwer.cena = 0.0f;
            } else {
                reply.serwer.cena = request.ilosc_biletow * bilety[request.typ_biletu].cena;
            }

            // Oblicz czas biletu
            wait_semaphore(g_park->park_sem, 0, 0);
            reply.serwer.start_biletu = g_park->czas_w_symulacji;
            int czas_trwania_h = bilety[request.typ_biletu].czasTrwania;
            reply.serwer.end_biletu = reply.serwer.start_biletu + SimTime(czas_trwania_h, 0);
            signal_semaphore(g_park->park_sem, 0);

            // Spróbuj zarezerwować miejsca
            reply.serwer.status = update_licznik_klientow(request);


            if (reply.serwer.status == 1) {
                if (oczekujacy.size() + oczekujacy_vip.size() < MAX_W_KOLEJCE || request.typ_biletu == BILETVIP) {
                    log_message(3, logger_id,"[KASA] - klient %d dodany do oczekujących (park pełny) VIP? %d\n", request.pid_klienta, request.typ_biletu == BILETVIP);
                    if (request.typ_biletu == BILETVIP) { oczekujacy_vip.push(request);}
                    else{oczekujacy.push(request);}
                }
                else {
                    reply.serwer.status = -1;
                }
            }
            // Wyślij odpowiedź
            while (msgsnd(g_park->kasa_reply_id, &reply, sizeof(reply) - sizeof(long), 0) == -1) {
                if (errno == EINTR) {
                    continue;
                }
                perror("mgsrcv");
                break;
            }
            // signal(g_park->msg_overflow_sem, 0);

            //log_message(3, logger_id, "[KASA MESSAGE] - confirm enter\n");

            if (reply.serwer.status == 0) {
                clients_pids[request.pid_klienta] = reply.serwer;
                wait_semaphore(g_park->park_sem, 0, 0);
                SimTime curtime = g_park->czas_w_symulacji;
                signal_semaphore(g_park->park_sem, 0);
                log_message(3, logger_id,"[KASA] - %02d:%02d - Klient %d kupił bilet %s (%.2f zł), osób: %d, ważny do %02d:%02d\n",curtime.hour,curtime.minute,
                         request.pid_klienta, bilety[request.typ_biletu].nazwa,
                         reply.serwer.cena, request.ilosc_osob,
                         reply.serwer.end_biletu.hour, reply.serwer.end_biletu.minute);
            }
            else {
                log_message(3, logger_id,"[KASA] - Klient %d ODRZUCONY (park zamknięty/kolejka oczekujących pełna %d)\n", request.pid_klienta, oczekujacy.size() + oczekujacy_vip.size());
            }
            fflush(stdout);

}

void wpusc_oczekujacych() {
    while (true) {
        if (oczekujacy_vip.empty()) break;
        klient_message request = oczekujacy_vip.front();
        wait_semaphore(g_park->park_sem, 0, 0);
        int clients_count = g_park->clients_count;
        signal_semaphore(g_park->park_sem, 0);
        if (clients_count+ request.ilosc_osob <= MAX_KLIENTOW_W_PARKU) {
            handle_enter(request);
            oczekujacy_vip.pop();
            continue;
        }
        break;
    }
    while (true) {
        if (oczekujacy.empty()) break;
        klient_message request = oczekujacy.front();
        wait_semaphore(g_park->park_sem, 0, 0);
        int clients_count = g_park->clients_count;
        signal_semaphore(g_park->park_sem, 0);
        if (clients_count+ request.ilosc_osob <= MAX_KLIENTOW_W_PARKU) {
            handle_enter(request);
            oczekujacy.pop();
            continue;
        }
        break;
    }
}

void handle_exit(const payment_message & payment_request) {
    kasa_message reply{0};
    reply.mtype = payment_request.pid;
    if (clients_pids.find(payment_request.pid) == clients_pids.end()) {
        log_message(3, logger_id,"[KASA] - BŁĄD: Brak danych biletu dla klienta %d\n", payment_request.pid);
        // signal_semaphore(g_park->msg_overflow_sem, 0);
        return;
    }
    wydrukuj_paragon(payment_request.pid, clients_pids[payment_request.pid], payment_request);

    float total = 0.0f;
    const serwer_message& bilet = clients_pids[payment_request.pid];

    float kwota_bilet = bilet.cena;
    if (payment_request.wiekDziecka > 0 && payment_request.wiekDziecka < 2) {
        kwota_bilet = 0.0f;
    }
    float koszt_rest = oblicz_koszt_restauracji(payment_request.czasWRestauracji) * bilet.ilosc_osob;
    int czas_nadmiarowy = 0;
    float doplata = 0.0f;
    if (bilet.typ_biletu != BILETVIP) {
        wait_semaphore(g_park->park_sem, 0, 0);
        czas_nadmiarowy = g_park->czas_w_symulacji.toMinutes() - bilet.end_biletu.toMinutes();
        signal_semaphore(g_park->park_sem, 0);

        if (czas_nadmiarowy > 0) {
            doplata = oblicz_doplate(bilet, czas_nadmiarowy) * bilet.ilosc_osob;
        }
    }
    total = kwota_bilet + koszt_rest + doplata;

    reply.payment = payment_request;
    reply.payment.suma = total;

    if (czas_nadmiarowy < 0) {
        czas_nadmiarowy = 0;
    }
    // daj paragon
    log_message(3, logger_id, "[KASA] - Klient %d , osób: %d koszt:(B+R+C|T):(%7.2f+%7.2f+%7.2f|%7.2f) czas w restauracji:%d, nadmiarowy %d\n", payment_request.pid, bilet.ilosc_osob,kwota_bilet,koszt_rest,
        doplata, total, payment_request.czasWRestauracji, czas_nadmiarowy );
    while (msgsnd(g_park->kasa_reply_id, &reply, sizeof(reply) - sizeof(long), 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("msgrcv");
        break;
    }
    // signal(g_park->msg_overflow_sem, 0);


    //log_message(3, logger_id, "[KASA MESSAGE] - confirm exit\n");
    // Aktualizuj statystyki
    zarobki += total;
    transakcje++;

    //zmniejsz licznik
    wait_semaphore(g_park->park_sem, 0, 0);
    g_park->clients_count -= clients_pids[payment_request.pid].ilosc_osob;
    signal_semaphore(g_park->park_sem, 0);

    wpusc_oczekujacych();

    // Usuń z mapy

    clients_pids.erase(payment_request.pid);

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

    log_message(3, logger_id,"[KASA] - CZYNNA (PID: %d)\n", getpid());
    fflush(stdout);

    kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);

    // Mapa przechowująca informacje o biletach klientów
    struct msqid_ds buf;
    msgctl(kasaId, IPC_STAT, &buf);

    while (true) {
        if (koniec) {
            log_message(3, logger_id,"[KASA] - Otrzymano sygnał zakończenia, zamykam kasę\n");
            fflush(stdout);
            break;
        }
        // msgctl(kasaId, IPC_STAT, &buf);
        // log_message(3, logger_id,"[TEST-2] [KASA] - Oczekuję na wiadomość, w kolejce: %lu wiadomości\n",
        //             buf.msg_qnum);
        kasa_message msg{0};

        ssize_t r = msgrcv(kasaId, &msg,sizeof(msg) - sizeof(long),-105,  0);
        if (r == -1) {
            if (errno == EINTR) continue;  // Sygnał przerwał - sprawdź `koniec`
            if (errno == EIDRM) break;     // Kolejka usunięta
            break;
        }
        wait_semaphore(g_park->park_sem, 0, 0);
        SimTime curTime = g_park->czas_w_symulacji;
        signal_semaphore(g_park->park_sem, 0);

        switch (msg.mtype) {
            case MSG_TYPE_VIP_TICKET:
            case MSG_TYPE_STANDARD_TICKET:
                //log_message(3, logger_id,"[TEST-2] %02d:%02d [KASA] - Odebrano wiadomość od klienta  %d, typ: %ld\n", curTime.hour, curTime.minute,msg.klient.pid_klienta, msg.mtype);
                handle_enter(msg.klient);
                break;

            case MSG_TYPE_EXIT_PAYMENT:
                handle_exit(msg.payment);
                break;
            case MSG_TYPE_CLEAR_QUEUE:
                while (!oczekujacy_vip.empty()) {
                    handle_enter(oczekujacy_vip.front());
                    oczekujacy_vip.pop();
                }
                while (!oczekujacy.empty()) {
                    handle_enter(oczekujacy.front());
                    oczekujacy.pop();
                }
            default:
                if (msg.mtype == 105) {
                    break;
                }
        }
        if (msg.mtype == 105) {  // Wiadomość zakończenia
            log_message(3, logger_id, "[KASA] - Otrzymano wiadomość zakończenia\n");
            break;
        }

        msgctl(kasaId, IPC_STAT, &buf);


        //usleep(1000);

    }

    log_message(3, logger_id,"[KASA] Zamykam kase -Liczba transakcji: %d,Suma zarobków: %.2f zł\n",transakcje, zarobki);

    fflush(stdout);


    //usleep(100000);
    detach_from_shared_block(g_park);

    return 0;

}
