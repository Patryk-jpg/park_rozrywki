
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
    g_klient.wiek = random_int(2, 90);
    g_klient.wzrost = g_klient.wiek<18? random_int(50, 180) : random_int(150,200);
    g_klient.pidKlienta = getpid();
    g_klient.typ_biletu = random_int(0, 3);
    g_klient.czasWRestauracji =0;

    if (g_klient.wiek > 18 && random_chance(30)) {

        g_klient.ma_dziecko = true;
        g_klient.dzieckoInfo = std::make_unique<dziecko>();
        g_klient.dzieckoInfo->wiek = random_int(1, 13);
        g_klient.dzieckoInfo->wzrost = random_int(50, 170);
        //log_message(logger_id, "[TEST-3] - %s Dziecko klienta %d: wiek: %d : wzrost %d \n", g_klient.pidKlienta, g_klient.dzieckoInfo->wiek, g_klient.dzieckoInfo->wzrost);
    }

    g_klient.ilosc_osob = g_klient.ma_dziecko ? 2  : 1;
    //log_message(logger_id, "[TEST-3] - %s  klient %d: wiek: %d : wzrost %d \n", g_klient.pidKlienta, g_klient.wiek, g_klient.wzrost);

    // if (true) {
    //     int czyVIP = atoi(argv[1]);
    //     g_klient.czyVIP = (bool)czyVIP;
    //
    // }

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
    // Przygotowanie wiadomośc  i do kasy
    kasa_message k_msg{0};
    k_msg.klient.ilosc_osob = g_klient.ilosc_osob;
    k_msg.klient.typ_biletu = g_klient.typ_biletu;
    k_msg.klient.ilosc_biletow = g_klient.ilosc_osob;
    k_msg.klient.pid_klienta = g_klient.pidKlienta;

    // VIP ma priorytet (mtype=1) i darmowy bilet
    SimTime curTime = getTime();
    if (g_klient.czyVIP) {
        k_msg.mtype = MSG_TYPE_VIP_TICKET;
        k_msg.klient.typ_biletu = BILETVIP;
        log_message(logger_id,"[TEST-2] %02d:%02d - Klient %d (VIP) wchodzi do kolejki priorytetowej\n", curTime.hour,curTime.minute, g_klient.pidKlienta);
    } else {
        k_msg.mtype = MSG_TYPE_STANDARD_TICKET;
        log_message(logger_id,"[TEST-2] %02d:%02d - Klient %d wchodzi do kolejki (bilet: %s, osób: %d)\n",curTime.hour, curTime.minute,
                   g_klient.pidKlienta, bilety[g_klient.typ_biletu].nazwa, g_klient.ilosc_osob);
    }
    // wait_semaphore(g_park->msg_overflow_sem, 0,0);
    while (msgsnd(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), IPC_NOWAIT) == -1) {
        //log_message(logger_id,"%02d:%02d - Klient %d ERROR msgsnd\n", curTime.hour,curTime.minute, g_klient.pidKlienta);
        if (errno == EINTR || errno == EAGAIN) {
            if (!g_park->park_otwarty) return;
            continue;
        } else if (errno == EIDRM || errno == EINVAL) {
            return;
        }
    // JEŚLI KOLEJKA JEST JUŻ PEŁNA I WYSTĄPIL EAGAIN to w zasadzie nie ma sensu żeby klient czekał
        return;
    }


    //log_message(logger_id, "[KASA MESSAGE] - enter\n");


    kasa_message reply{0};
    while (msgrcv(g_park->kasa_reply_id, &reply, sizeof(reply) - sizeof(long),
                                g_klient.pidKlienta, 0) == -1) {
        log_message(logger_id, "Klient %d nie udalo sie wejsc- sygnal wyrzucil z kolejki do parku\n");
        if (errno == EINTR) {continue;}
        if (errno == EIDRM || errno == EINVAL) {break;}
        return;
    }
    curTime = getTime();
    log_message(logger_id,"[TEST-2] %02d:%02d - Klient %d wychodzi z kolejki do kasy: VIP? : %d\n",curTime.hour,curTime.minute, g_klient.pidKlienta, g_klient.czyVIP);
    fflush(stdout);

    if (reply.serwer.status == -1) {
        log_message(logger_id,"Nie udalo sie wejsc do parku, klient %d ucieka\n", g_klient.pidKlienta);
        return;
    }

    // OD TERAZ KLIENT W PARKU

    g_klient.czasWejscia = reply.serwer.start_biletu;
    g_klient.cena = reply.serwer.cena;
    g_klient.czasWyjscia = reply.serwer.end_biletu;
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
    int atrakcja_reply_id = g_park->pracownicy_keys[nr_atrakcji + LICZBA_ATRAKCJI];
    if (atrakcja_id <= 0) {
        log_message(logger_id,"Klient %d: atrakcja %s zamknięta (kolejka nie istnieje)\n",
                   identifier, atrakcje[nr_atrakcji].nazwa);
        return -1;
    }
    // struct msqid_ds buf;
    // msgctl(atrakcja_id, IPC_STAT, &buf);
    // if ((float)buf.msg_cbytes / buf.msg_qbytes > 0.5) {
    //     return -2;
    // }


    // Żądanie wejścia
    ACKmes mes;
    mes.mtype = MSG_TYPE_JOIN_ATTRACTION;
    mes.ilosc_osob = g_klient.ilosc_osob;
    mes.ack = identifier;
    mes.wagonik = 0;
    log_message(logger_id,"Klient %d: atrakcja %s probuje wyslac aby wejsc \n",
           identifier, atrakcje[nr_atrakcji].nazwa);

    if (msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), IPC_NOWAIT) == -1) {
        if (errno == EIDRM || errno == EINVAL || errno == EINTR || errno == EAGAIN) {
            log_message(logger_id,"Klient %d: atrakcja %s zamknięta (kolejka usunięta) lub pełna kolejka\n",
                       identifier, atrakcje[nr_atrakcji].nazwa);
            return -3;
        }
    }

    while (msgrcv(atrakcja_reply_id, &mes, sizeof(mes) - sizeof(long),
                                g_klient.pidKlienta, 0) == -1) {
        if (errno == EINTR) continue;
        return -3;
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
        log_message(logger_id,"Klient %d: wyjscie z %s i z parku\n", identifier, atrakcje[nr_atrakcji].nazwa);
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
            ssize_t mess = msgrcv(atrakcja_reply_id, &mes, sizeof(ACKmes) - sizeof(long),
                            identifier, IPC_NOWAIT);
            if (mess != -1) {
                log_message(logger_id,"Klient %d: %s zakończona przez pracownika\n",
                           identifier, atrakcje[nr_atrakcji].nazwa);
                return mes.ack;
            }
            //usleep(10000);
        }

        czas_zakonczenia = getTime();
        log_message(logger_id,"%02d:%02d - Klient %d REZYGNUJE z %s po %d min\n", curTime.hour, curTime.minute,
                   identifier, atrakcje[nr_atrakcji].nazwa, czas_do_rezygnacji);

        fflush(stdout);
        mes.mtype = MSG_TYPE_QUIT_ATTRACTION;
        mes.ack = identifier;
        mes.wagonik = moj_wagonik;
        while (msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), 0) == -1) {
            if (errno == EINTR) {
                    continue;
            }
            if (errno == EAGAIN || errno == EIDRM) {return -3;}
        }

        while (msgrcv(atrakcja_reply_id, &mes, sizeof(ACKmes) - sizeof(long),
                                    g_klient.pidKlienta, 0) == -1) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EIDRM) {return -3;}
        }


    }else {
        // Normalne zakończenie atrakcji

         while (msgrcv(atrakcja_reply_id, &mes, sizeof(ACKmes) - sizeof(long),
                                    g_klient.pidKlienta, 0) == -1) {
             if (errno== EINTR) continue;
             if (errno == EAGAIN || errno == EIDRM) {return -3;}
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
        log_message(logger_id,"Baw-sie klient %d wychodzi z parku\n", g_klient.pidKlienta);
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
           //usleep(100000); // Czekaj 100ms
           curTime = getTime();
           break;
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

    kasa_message payment_msg{0};
    payment_msg.payment.czasWRestauracji = g_klient.czasWRestauracji;
    payment_msg.payment.pid = g_klient.pidKlienta;
    payment_msg.payment.wiekDziecka = g_klient.ma_dziecko ? g_klient.dzieckoInfo->wiek : -1;
    payment_msg.payment.czasWyjscia = getTime();
    payment_msg.mtype = MSG_TYPE_EXIT_PAYMENT;
    SimTime curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d idzie do kasy zapłacić przy wyjściu\n",curTime.hour,curTime.minute, g_klient.pidKlienta);

    // wait_semaphore(g_park->msg_overflow_sem,0,0);

    while (msgsnd(kasaId, &payment_msg, sizeof(payment_msg) - sizeof(long), 0) == -1) {
        if (errno == EAGAIN) {continue;}
        if(errno == EINTR) {continue;}
        if (errno == EIDRM) {break;}

    }
    //log_message(logger_id, "[KASA MESSAGE] - exit\n");

    msgrcv(g_park->kasa_reply_id, &payment_msg, sizeof(payment_msg) - sizeof(long),
                                g_klient.pidKlienta, 0);


    curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d WYCHODZI z parku, zapłacił %.2f zł\n",
               curTime.hour, curTime.minute, g_klient.pidKlienta, payment_msg.payment.suma);

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

    while (msgsnd(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
        if (errno == EAGAIN) {continue;}
        if(errno == EINTR) {continue;}
        if (errno == EIDRM) {break;}
    }


    while (msgrcv(g_park->kasa_rest_reply_id, &msg, sizeof(msg) - sizeof(long),
                                g_klient.pidKlienta, 0) == -1) {
        if (errno == EAGAIN) {continue;}
        if(errno == EINTR) {continue;}
        if (errno == EIDRM) {break;}
    }

    curTime = getTime();
    log_message(logger_id,"%02d:%02d - Klient %d wychodzi z restauracji, zapłacił %.2f zł\n",
               curTime.hour, curTime.minute, g_klient.pidKlienta, msg.kwota);

}

bool spelniaWymagania(int nr_atrakcji) {
    const Atrakcja& atrakcja = atrakcje[nr_atrakcji];
    bool spelnia = true;
    if (!SEZON_LETNI && atrakcja.sezonowa_letnia) {
        log_message(logger_id,"[TEST-3] - %s - wymagania nie spełnione (to nie sezon letni)\n");
        return false;
    }
    if (atrakcja.max_wiek != 999 && g_klient.wiek > atrakcja.max_wiek) {
        log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione max wiek: %d, wiek klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.max_wiek, g_klient.wiek);
        g_klient.odwiedzone.push_back(nr_atrakcji);
        return false;
    }
    if (g_klient.wiek < atrakcja.min_wiek) {
        log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione minimalny wiek: %d, wiek klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.min_wiek, g_klient.wiek);
        g_klient.odwiedzone.push_back(nr_atrakcji);
        return false;
    }
    if (atrakcja.max_wzrost != 999 && g_klient.wzrost > atrakcja.max_wzrost) {
        log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione maks wzrost %d, wzrost klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.max_wzrost, g_klient.wzrost);
        g_klient.odwiedzone.push_back(nr_atrakcji);
        return false;
    }
    if (g_klient.wzrost < atrakcja.min_wzrost) {
        log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione minimalny wzrost %d, wzrost klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.min_wzrost, g_klient.wzrost);
        g_klient.odwiedzone.push_back(nr_atrakcji);
        return false;
    }
    if (g_klient.wiek < 18 && !g_klient.ma_dziecko) {
        // Ten klient jest dzieckiem bez opiekuna
        if (atrakcja.wiek_wymaga_opiekuna != -1 && g_klient.wiek < atrakcja.wiek_wymaga_opiekuna) {
            log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione, atrakcja wymaga opiekuna do lat %d klient (wiek %d)\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.wiek_wymaga_opiekuna, g_klient.wiek);
            g_klient.odwiedzone.push_back(nr_atrakcji);
            return false;// Za młody bez opiekuna
        }
    }
    if (g_klient.wiek < 18 && !g_klient.ma_dziecko) {
        // Ten klient jest dzieckiem bez opiekuna
        if (atrakcja.wzrost_wymaga_opiekuna != -1 && g_klient.wzrost < atrakcja.wzrost_wymaga_opiekuna) {
            log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione, atrakcja wymaga opiekuna do wzrostu %d (wzrost %d)\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.wzrost_wymaga_opiekuna, g_klient.wzrost);
            g_klient.odwiedzone.push_back(nr_atrakcji);
            return false; // Za niski bez opiekuna
        }
    }
    if (g_klient.ma_dziecko) {
        if (atrakcja.max_wiek != 999 && g_klient.dzieckoInfo->wiek > atrakcja.max_wiek) {
            log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione dla dziecka max wiek: %d, wiek klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.max_wiek, g_klient.dzieckoInfo->wiek);
            g_klient.odwiedzone.push_back(nr_atrakcji);
            return false;
        }
        if (g_klient.dzieckoInfo->wiek < atrakcja.min_wiek) {
            log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione dla dziecka minimalny wiek: %d, wiek klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.min_wiek, g_klient.dzieckoInfo->wiek);
            g_klient.odwiedzone.push_back(nr_atrakcji);
            return false;
        }
        if (atrakcja.max_wzrost != 999 && g_klient.dzieckoInfo->wzrost > atrakcja.max_wzrost) {
            log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie dla dziecka spełnione maks wzrost %d, wzrost klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.max_wzrost, g_klient.dzieckoInfo->wzrost);
            g_klient.odwiedzone.push_back(nr_atrakcji);
            return false;
        }
        if (g_klient.dzieckoInfo->wzrost < atrakcja.min_wzrost) {
            log_message(logger_id,"[TEST-3] - %s - klient: %d wymagania nie spełnione dla dziecka minimalny wzrost %d, wzrost klienta %d\n",atrakcja.nazwa,g_klient.pidKlienta,atrakcja.min_wzrost, g_klient.dzieckoInfo->wzrost);
            g_klient.odwiedzone.push_back(nr_atrakcji);
            return false;
        }
    }

    return true;

}