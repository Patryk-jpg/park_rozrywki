
#include "klient.h"
#include <pthread.h>
#include <unistd.h>

// GLOBAL

static volatile sig_atomic_t ewakuacja = 0;
park_wspolne* g_park = nullptr;
static klient g_klient;
std::vector<int> anulowalne = {0, 1, 2, 3, 4, 12, 13, 14, 15, 16}; // Indeksy A1-A5, A13-A17
int logger_id = -1;
// SIG HANDLERS

void sigint_handler(int sig) {
    ewakuacja = 1;
}

// FUNCTIONS

SimTime getTime() {
    wait_semaphore(g_park->park_sem,0,0);
    SimTime curTime = g_park->czas_w_symulacji;
    signal_semaphore(g_park->park_sem,0);
    return curTime;
}

bool park_otwarty() {
    wait_semaphore(g_park->park_sem,0,0);
    bool open = g_park->park_otwarty && !ewakuacja;
    signal_semaphore(g_park->park_sem,0);
    return open;
}

bool contains(const std::vector<int>& lista, int val) {
    for (int i=0; i<lista.size(); i++) {
        if (val == lista[i]) return true;

    }
    return false;
}

// MAIN

int main(int argc, char* argv[]) {

    // Obsługa sygnałów
    struct sigaction sa_int{};
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, nullptr);

    init_random();
    g_park = attach_to_shared_block();
    logger_id = g_park->logger_id;

    if (random_chance(1)) {
        g_klient.czyVIP=true;
    }
    g_klient.wiek = random_int(13, 90);
    g_klient.wzrost = g_klient.wiek<18? random_int(50, 180) : random_int(150,200);
    g_klient.pidKlienta = getpid();
    g_klient.typ_biletu = random_int(0, 3);
    g_klient.czasWRestauracji =0;

    if (g_klient.wiek > 18 && random_chance(30)) {

        g_klient.ma_dziecko = true;
        g_klient.dzieckoInfo = std::make_unique<dziecko>();  // ← DODAJ!
        g_klient.dzieckoInfo->wiek = random_int(1, 13);
        g_klient.dzieckoInfo->wzrost = random_int(50, 170);
    }

    g_klient.ilosc_osob = g_klient.ma_dziecko ? 2  : 1;

    if (random_chance(5)) {
        SimTime curtime = getTime();
        log_message(logger_id,"%02d:%02d - Klient %d idzie do restauracji PRZED wejściem do parku\n",curtime.hour,curtime.minute,g_klient.pidKlienta);
        fflush(stdout);
        idz_do_atrakcji(16, g_klient.pidKlienta);
        if (random_chance(95)) {
            g_klient.odwiedzone.push_back(16);
        }
    }
    // Wejście do parku
    wejdz_do_parku();
    if (random_chance(5) &&g_park->park_otwarty) {
        SimTime curtime = getTime();
        log_message(logger_id,"%02d:%02d -Klient %d idzie do restauracji PO wyjściu z parku\n", curtime.hour,curtime.minute, g_klient.pidKlienta);
        idz_do_atrakcji(16, g_klient.pidKlienta);
    }

    detach_from_shared_block(g_park);
    return 0;
}
void wejdz_do_parku() {
    if (!park_otwarty()) {
        log_message(logger_id,"Klient %d: park zamknięty, nie wchodzi\n", g_klient.pidKlienta);
        return;
    }

    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    if (kasaId == -1) {
        log_message(logger_id,"Kasa zamknięta, klient %d nie wchodzi\n", g_klient.pidKlienta);
        return;
    }
    // Przygotowanie wiadomości do kasy
    klient_message k_msg{};
    k_msg.ilosc_osob = g_klient.ilosc_osob;
    k_msg.typ_biletu = g_klient.typ_biletu;
    k_msg.ilosc_biletow = g_klient.ilosc_osob;
    k_msg.pid_klienta = g_klient.pidKlienta;

    // VIP ma priorytet (mtype=1) i darmowy bilet
    SimTime curTime = getTime();
    if (g_klient.czyVIP) {
        k_msg.mtype = MSG_TYPE_VIP_TICKET;
        k_msg.typ_biletu = BILETVIP;
        log_message(logger_id,"%02d:%02d - Klient %d (VIP) wchodzi do kolejki priorytetowej\n", curTime.hour,curTime.minute, g_klient.pidKlienta);
    } else {
        k_msg.mtype = MSG_TYPE_STANDARD_TICKET;
        log_message(logger_id,"%02d:%02d - Klient %d wchodzi do kolejki (bilet: %s, osób: %d)\n",curTime.hour, curTime.minute,
                   g_klient.pidKlienta, bilety[g_klient.typ_biletu].nazwa, g_klient.ilosc_osob);
    }

    msgsnd(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), 0);

    serwer_message reply;

    while (true) {
        ssize_t result = msgrcv(kasaId, &reply, sizeof(reply) - sizeof(long),
                                g_klient.pidKlienta, IPC_NOWAIT);
        if (result != -1) {
            break; // Otrzymano odpowiedź
        }
        if (ewakuacja) {
            log_message(logger_id,"Klient %d: ewakuacja podczas oczekiwania w kolejce\n", g_klient.pidKlienta);
            wyjdz_z_parku();
            return;
        }
        if (!g_park->park_otwarty) {
            log_message(logger_id,"Klient %d: przestaje czekać na wejście do parku bo park się zamknął\n", g_klient.pidKlienta);
            return;
        }
        usleep(50000);
    }
    curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d wychodzi z kolejki do kasy\n",curTime.hour,curTime.minute, g_klient.pidKlienta);
    fflush(stdout);

    if (reply.status == -1) {
        log_message(logger_id,"Nie udalo sie wejsc do parku, klient %d ucieka\n", g_klient.pidKlienta);
        return;
    }

    // OD TERAZ KLIENT W PARKU

    g_klient.czasWejscia = reply.start_biletu;
    g_klient.cena = reply.cena;
    g_klient.czasWyjscia = reply.end_biletu;
    g_klient.wParku = true;


    wait_semaphore(g_park->park_sem,0,0);
    curTime = g_park->czas_w_symulacji;
    int ludzi_w_parku = g_park->clients_count;
    signal_semaphore(g_park->park_sem,0);

    log_message(logger_id,"%02d:%02d:Klient %d w parku z biletem %s, wyjdzie o %02d:%02d\n"
           "Ilosc ludzi w parku: %d \n",curTime.hour, curTime.minute,g_klient.pidKlienta, bilety[g_klient.typ_biletu].nazwa,
           g_klient.czasWyjscia.hour, g_klient.czasWyjscia.minute
           ,ludzi_w_parku);

    baw_sie();

}





int idz_do_atrakcji(int nr_atrakcji, pid_t identifier) {
    SimTime czas_zakonczenia;
    SimTime czas_rozpoczecia = getTime();
    if (!park_otwarty()) {
        log_message(logger_id,"Klient %d: park zamknięty, nie może iść na atrakcję\n", identifier);
        return -3;
    }
    int atrakcja_id = g_park->pracownicy_keys[nr_atrakcji];
    if (atrakcja_id <= 0) {
        log_message(logger_id,"Klient %d: atrakcja %s zamknięta (kolejka nie istnieje)\n",
                   identifier, atrakcje[nr_atrakcji].nazwa);
        return -1;
    }

    // Żądanie wejścia
    ACKmes mes;
    mes.mtype = MSG_TYPE_JOIN_ATTRACTION;
    mes.ilosc_osob = g_klient.ilosc_osob;
    mes.ack = identifier;
    mes.wagonik = 0;

    if (msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), IPC_NOWAIT) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            log_message(logger_id,"Klient %d: atrakcja %s zamknięta (kolejka usunięta)\n",
                       identifier, atrakcje[nr_atrakcji].nazwa);
            return -1;
        }
    }

    while (true) {
        ssize_t result = msgrcv(atrakcja_id, &mes, sizeof(mes) - sizeof(long),
                                g_klient.pidKlienta, IPC_NOWAIT);
        if (result != -1) {
            break; // Otrzymano odpowiedź
        }
        if (ewakuacja) {
            log_message(logger_id,"Klient %d: ewakuacja podczas oczekiwania na %s\n",identifier, atrakcje[nr_atrakcji].nazwa);
            return -3;
        }
        usleep(50000);
    }

    if (mes.ack == -1) {
        log_message(logger_id,"Klient %d: brak miejsca na %s\n", identifier, atrakcje[nr_atrakcji].nazwa);
        return -1;
    }
    if (mes.ack == -2) {
        log_message(logger_id,"Klient %d: %s tymczasowo zatrzymana\n", identifier, atrakcje[nr_atrakcji].nazwa);
        return -2;
    }
    if (mes.ack == -3) {
        log_message(logger_id,"Klient %d: ewakuacja z %s\n", identifier, atrakcje[nr_atrakcji].nazwa);
        return -3;
    }

    int moj_wagonik = mes.wagonik;
    czas_rozpoczecia = getTime();

    log_message(logger_id,"%02d:%02d - Klient %d rozpoczyna zabawę na %s (wagonik %d)\n",
                  czas_rozpoczecia.hour, czas_rozpoczecia.minute,
                  identifier, atrakcje[nr_atrakcji].nazwa, moj_wagonik);

    // Decyzja czy zrezygnować (10% szans dla dozwolonych atrakcji)
    bool czyZrezygnuje = atrakcje[nr_atrakcji].mozna_opuscic && random_chance(10);

    if (nr_atrakcji == 16) {czyZrezygnuje = true;}

    if (contains(anulowalne, nr_atrakcji) && czyZrezygnuje) {

        // Losowy czas rezygnacji
        int czas_do_rezygnacji = random_int(5, atrakcje[nr_atrakcji].czas_trwania_min);
        SimTime timeout = czas_rozpoczecia + SimTime(0, czas_do_rezygnacji);
        SimTime curTime = czas_rozpoczecia;
        while (curTime <= timeout && !ewakuacja) {
            curTime = getTime();
            // Sprawdź czy pracownik nie przerwał atrakcji
            ssize_t mess = msgrcv(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long),
                            identifier, IPC_NOWAIT);
            if (mess != -1) {
                log_message(logger_id,"Klient %d: %s zakończona przez pracownika\n",
                           identifier, atrakcje[nr_atrakcji].nazwa);
                return mes.ack;
            }
            usleep(10000);
        }

        czas_zakonczenia = getTime();
        log_message(logger_id,"%02d:%02d - Klient %d REZYGNUJE z %s po %d min\n", curTime.hour, curTime.minute,
                   identifier, atrakcje[nr_atrakcji].nazwa, czas_do_rezygnacji);

        fflush(stdout);
        mes.mtype = MSG_TYPE_QUIT_ATTRACTION;
        mes.ack = identifier;
        mes.wagonik = moj_wagonik;
        msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), 0);
        while (true) {
            ssize_t result = msgrcv(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long),
                                    g_klient.pidKlienta, IPC_NOWAIT);
            if (result != -1) break;
            if (ewakuacja) return -3;
            usleep(50000);
        }

    }else {
        // Normalne zakończenie atrakcji
        while (true) {
            ssize_t result = msgrcv(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long),
                                    g_klient.pidKlienta, IPC_NOWAIT);
            if (result != -1) break;
            if (ewakuacja) return -3;
            usleep(50000);
        }
    }
    if (nr_atrakcji == 16) {
        int czas_pobytu = czas_zakonczenia.toMinutes() - czas_rozpoczecia.toMinutes();
        if (czas_pobytu < 0) czas_pobytu = 0; // Zabezpieczenie

        g_klient.czasWRestauracji += czas_pobytu;
        SimTime curTime = getTime();
        log_message(logger_id,"%02d:%02d - Klient %d: był w restauracji %d min (łącznie: %d min)\n",curTime.hour,curTime.minute,
                   identifier, czas_pobytu, g_klient.czasWRestauracji);
        fflush(stdout);
        // Jeśli poza parkiem - płać od razu
        if (!g_klient.wParku) {
            zaplac_za_restauracje_z_zewnatrz(czas_pobytu);
        }
    }

    return mes.ack;
}


void baw_sie() {
    if (!park_otwarty()) {
        log_message(logger_id,"Baw-sie klient wychodzi z parku");
        wyjdz_z_parku();
        return;
    }
    SimTime curTime = getTime();

   while (curTime < g_klient.czasWyjscia) {

       if (!park_otwarty()) {
           log_message(logger_id,"Klient %d: park został zamknięty, wychodzi\n", g_klient.pidKlienta);
           wyjdz_z_parku();
           return;
       }

       int nr_atrakcji = random_int(0, 16);
       int max_proby = 50; // Zabezpieczenie przed nieskończoną pętlą

       while ((contains(g_klient.odwiedzone, nr_atrakcji) || !spelniaWymagania(nr_atrakcji))
               && g_klient.odwiedzone.size() < 17
               && max_proby-- > 0) {
                nr_atrakcji = random_int(0, 16);
        }

       if (max_proby <= 0) {
           //log_message(logger_id,"Klient %d: brak dostępnych atrakcji, czeka\n", g_klient.pidKlienta);
           usleep(100000); // Czekaj 100ms
           curTime = getTime();
           continue;
       }

        int status = idz_do_atrakcji(nr_atrakcji, g_klient.pidKlienta);

       if (status == -1) {
           // Atrakcja zamknięta/pełna - oznacz jako odwiedzoną
           g_klient.odwiedzone.push_back(nr_atrakcji);
       } else if (status == -2) {
           // Atrakcja tymczasowo zatrzymana - spróbuj później
            continue;
        } else if (status == -3) {
            // Ewakuacja
            log_message(logger_id,"Klient %d: ewakuacja podczas zabawy\n", g_klient.pidKlienta);
            wyjdz_z_parku();
            return;
        } else {
            // 5% szans że wróci ponownie
            if (random_chance(95)) {
                g_klient.odwiedzone.push_back(nr_atrakcji);
            }
        }

        usleep(1000);
        curTime = getTime();
    }

    usleep(1000);
    log_message(logger_id,"Klient %d: upłynął czas biletu, wychodzi\n", g_klient.pidKlienta);
    wyjdz_z_parku();
}

void wyjdz_z_parku() {
    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);

    payment_message k_msg;
    k_msg.czasWRestauracji = g_klient.czasWRestauracji;
    k_msg.pid = g_klient.pidKlienta;
    k_msg.wiekDziecka = g_klient.ma_dziecko ? g_klient.dzieckoInfo->wiek : -1;
    k_msg.czasWyjscia = getTime();
    k_msg.mtype = MSG_TYPE_EXIT_PAYMENT;
    SimTime curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d idzie do kasy zapłacić przy wyjściu\n",curTime.hour,curTime.minute, g_klient.pidKlienta);

    msgsnd(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), 0);

    while (true) {
        ssize_t result = msgrcv(kasaId, &k_msg, sizeof(k_msg) - sizeof(long),
                                g_klient.pidKlienta, IPC_NOWAIT);
        if (result != -1) break;
        if (ewakuacja) return;
        if (!g_park->park_otwarty) {
            log_message(logger_id,"%02d:%02d - Klient %d WYCHODZI z parku bez czekania na paragon\n",
               curTime.hour, curTime.minute, g_klient.pidKlienta);
            return;
        }
        usleep(50000);
    }

    curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d WYCHODZI z parku, zapłacił %.2f zł\n",
               curTime.hour, curTime.minute, g_klient.pidKlienta, k_msg.suma);

}

void  zaplac_za_restauracje_z_zewnatrz(int czas_pobytu) {

    SimTime curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d idzie do kasy restauracji (bez wejścia do parku)\n",
               curTime.hour, curTime.minute, g_klient.pidKlienta);


    int kasa_rest_id = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED);

    restauracja_message msg;
    msg.mtype = 1;
    msg.pid_klienta = g_klient.pidKlienta;
    msg.czas_pobytu_min = czas_pobytu * g_klient.ilosc_osob;

    msgsnd(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 0);

    while (true) {
        ssize_t result = msgrcv(kasa_rest_id, &msg, sizeof(msg) - sizeof(long),
                                g_klient.pidKlienta, IPC_NOWAIT);
        if (result != -1) break;
        if (ewakuacja) return;
        // if (!g_park->park_otwarty) {
        //     return;
        // }
        usleep(50000);
    }

    curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d wychodzi z restauracji, zapłacił %.2f zł\n",
               curTime.hour, curTime.minute, g_klient.pidKlienta, msg.kwota);

}

bool spelniaWymagania(int nr_atrakcji) {
    const Atrakcja& atrakcja = atrakcje[nr_atrakcji];

    if (!SEZON_LETNI && atrakcja.sezonowa_letnia) {
        return false;
    }
    if (atrakcja.max_wiek != 999 && g_klient.wiek > atrakcja.max_wiek) {
        return false;
    }
    if (g_klient.wiek < atrakcja.min_wiek) {
        return false;
    }
    if (atrakcja.max_wzrost != 999 && g_klient.wzrost > atrakcja.max_wzrost) {
        return false;
    }
    if (g_klient.wzrost < atrakcja.min_wzrost) {
        return false;
    }
    if (g_klient.wiek < 18 && !g_klient.ma_dziecko) {
        // Ten klient jest dzieckiem bez opiekuna
        if (atrakcja.wiek_wymaga_opiekuna != -1 && g_klient.wiek < atrakcja.wiek_wymaga_opiekuna) {
            return false; // Za młody bez opiekuna
        }
    }
    if (g_klient.wiek < 18 && !g_klient.ma_dziecko) {
        // Ten klient jest dzieckiem bez opiekuna
        if (atrakcja.wzrost_wymaga_opiekuna != -1 && g_klient.wzrost < atrakcja.wzrost_wymaga_opiekuna) {
            return false; // Za niski bez opiekuna
        }
    }
    if (g_klient.ma_dziecko) {
        if (atrakcja.max_wiek != 999 && g_klient.dzieckoInfo->wiek > atrakcja.max_wiek) {
            return false;
        }
        if (g_klient.dzieckoInfo->wiek < atrakcja.min_wiek) {
            return false;
        }
        if (atrakcja.max_wzrost != 999 && g_klient.dzieckoInfo->wzrost > atrakcja.max_wzrost) {
            return false;
        }
        if (g_klient.dzieckoInfo->wzrost < atrakcja.min_wzrost) {
            return false;
        }
    }
    return true;

}