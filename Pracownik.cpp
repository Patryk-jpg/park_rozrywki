#include "park_wspolne.h"
#include <algorithm>
int logger_id = -1;

struct czasy {
    SimTime czasJazdy;           // Kiedy atrakcja się kończy
    std::vector<pid_t> pids;     // Lista klientów w wagoniku
    bool zajete;                 // Czy wagonik w użyciu
};

std::vector<int> anulowalne = {0, 1, 2, 3, 4, 12, 13, 14, 15, 16}; // Indeksy A1-A5, A13-A17

bool contains(const std::vector<int>& lista, int val) {
    for (int i=0; i<lista.size(); i++) {
        if (val == lista[i]) return true;

    }
    return false;
}

static volatile sig_atomic_t ewakuacja = 0;
static volatile sig_atomic_t zatrzymano = 0;
park_wspolne* g_park = nullptr;

SimTime getTime() {
    wait_semaphore(g_park->park_sem,0,0);
    SimTime curTime = g_park->czas_w_symulacji;
    signal_semaphore(g_park->park_sem,0);
    return curTime;
}

void sig1handler(int sig) {
   zatrzymano =  1;
}

void sig2handler(int sig) {
    zatrzymano = 0;
}

void sig3handler(int sig) {
    zatrzymano = 1;
    ewakuacja = 1;
}

int znajdzWolnyWagonik(czasy czasyJazdy[], int iloscWagonikow) {
    for (int i = 0; i < iloscWagonikow; i++) {
        if (!czasyJazdy[i].zajete) {
            return i;
        }
    }
    return -1;
}
void ewakuuj_wszystkich(int wejscieDoAtrakcji, czasy czasyJazdy[], int iloscWagonikow, int nr_atrakcji) {
    log_message(logger_id,"EWAKUACJA atrakcji %s - wyrzucam wszystkich klientów\n", atrakcje[nr_atrakcji].nazwa);

    // Wyrzuć wszystkich z wagoników
    for (int i = 0; i < iloscWagonikow; i++) {
        if (czasyJazdy[i].zajete) {
            for (auto pid : czasyJazdy[i].pids) {
                ACKmes mes;
                mes.mtype = pid;
                mes.ack = -3; // -3 = ewakuacja
                if (!contains(anulowalne, nr_atrakcji)){usleep(MINUTA);}
                msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);
            }
            czasyJazdy[i].pids.clear();
            czasyJazdy[i].zajete = false;
            czasyJazdy[i].czasJazdy = SimTime(0, 0);
        }
    }

    // Odrzuć wszystkich z kolejki
    ACKmes mes_queue;
    while (msgrcv(wejscieDoAtrakcji, &mes_queue, sizeof(mes_queue) - sizeof(long), 0, IPC_NOWAIT) != -1) {
        if (mes_queue.mtype == MSG_TYPE_JOIN_ATTRACTION || mes_queue.mtype == MSG_TYPE_QUIT_ATTRACTION) {
            mes_queue.mtype = mes_queue.ack;
            mes_queue.ack = -3; // -3 = ewakuacja
            msgsnd(wejscieDoAtrakcji, &mes_queue, sizeof(mes_queue) - sizeof(long), IPC_NOWAIT);
        }
    }
}


int main(int argc, char* argv[]) {

    struct sigaction sa_int{};
    sa_int.sa_handler = sig3handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, nullptr);

    struct sigaction sa_usr1{};
    sa_usr1.sa_handler = sig1handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, nullptr);

    struct sigaction sa_usr2{};
    sa_usr2.sa_handler = sig2handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;
    sigaction(SIGUSR2, &sa_usr2, nullptr);

    if (argc < 2) {
        PRINT_ERROR("Brak numeru atrakcji!\n");
        return 1;
    }

    int nr_atrakcji = atoi(argv[1]);

    if (nr_atrakcji < 0 || nr_atrakcji >= LICZBA_ATRAKCJI) {
        PRINT_ERROR("Niepoprawny numer atrakcji: %d\n");
        return(1);
    }
    g_park = attach_to_shared_block();
    logger_id = g_park->logger_id;


    log_message(logger_id,"[PRACOWNIK-%d] Start obsługi atrakcji: %s (PID: %d)\n",
         nr_atrakcji, atrakcje[nr_atrakcji].nazwa, getpid());


    
    int wejscieDoAtrakcji = g_park->pracownicy_keys[nr_atrakcji];

    int iloscWagonikow = atrakcje[nr_atrakcji].limit_osob / atrakcje[nr_atrakcji].po_ile_osob_wchodzi;
    if (iloscWagonikow < 1) iloscWagonikow = 1;

    czasy* czasyJazdy = new czasy[iloscWagonikow];
    for (int i = 0; i < iloscWagonikow; i++) {
        czasyJazdy[i].czasJazdy = SimTime(0, 0);
        czasyJazdy[i].zajete = false;
        czasyJazdy[i].pids.clear();
    }
    struct msqid_ds buf{};
    int uruchomione_atrakcje = 0;

    while (true) {

        msgctl(wejscieDoAtrakcji, IPC_STAT, &buf);

        wait_semaphore(g_park->park_sem, 0, 0);
        int licznik_klientow = g_park->clients_count;
        bool otwarty = g_park->park_otwarty;
        signal_semaphore(g_park->park_sem, 0);

        if (licznik_klientow == 0 && !otwarty) {
            log_message(logger_id,"Atrakcja %s: park zamknięty, brak klientów, kończę pracę\n",
                         atrakcje[nr_atrakcji].nazwa);
            break;
        }

        if (zatrzymano) {
            if (ewakuacja) {
                log_message(logger_id,"EWAKUACJA na atrakcji %s!\n", atrakcje[nr_atrakcji].nazwa);
                ewakuuj_wszystkich(wejscieDoAtrakcji, czasyJazdy, iloscWagonikow, nr_atrakcji);
                break;
            }
            log_message(logger_id,"ZATRZYMANIE atrakcji %s\n", atrakcje[nr_atrakcji].nazwa);

            fflush(stdout);
            // Wyrzuć klientów z wagoników
            for (int i = 0; i < iloscWagonikow; i++) {
                if (czasyJazdy[i].zajete) {
                    for (auto pid : czasyJazdy[i].pids) {
                        ACKmes mes;
                        mes.mtype = pid;
                        mes.ack = -2; // -2 = tymczasowo zatrzymano
                        msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);
                    }
                    czasyJazdy[i].pids.clear();
                    czasyJazdy[i].zajete = false;
                    czasyJazdy[i].czasJazdy = SimTime(0, 0);
                }
            }

            // Odrzuć nowych z kolejki
            ACKmes mes_queue;
            while (msgrcv(wejscieDoAtrakcji, &mes_queue, sizeof(mes_queue) - sizeof(long), 0, IPC_NOWAIT) != -1) {
                if (mes_queue.mtype == MSG_TYPE_JOIN_ATTRACTION || mes_queue.mtype == MSG_TYPE_QUIT_ATTRACTION) {
                    mes_queue.mtype = mes_queue.ack;
                    mes_queue.ack = -2; // -2 = spróbuj później
                    msgsnd(wejscieDoAtrakcji, &mes_queue, sizeof(mes_queue) - sizeof(long), IPC_NOWAIT);
                }
            }

            usleep(10000);
            continue;

        }
            SimTime curTime = getTime();
            ACKmes mes;
            while (msgrcv(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 99, IPC_NOWAIT) != -1) {
                // Klient rezygnuje z wagonika
                if (mes.wagonik >= 0 && mes.wagonik < iloscWagonikow) {
                    auto it = std::find(czasyJazdy[mes.wagonik].pids.begin(),
                                       czasyJazdy[mes.wagonik].pids.end(),
                                       mes.ack);

                    if (it != czasyJazdy[mes.wagonik].pids.end()) {
                        czasyJazdy[mes.wagonik].pids.erase(it);
                        SimTime curtime = getTime();
                        log_message(logger_id,"%02d:%02d,Klient %d zrezygnował z %s (wagonik %d)\n", curtime.hour, curtime.minute,
                                     mes.ack, atrakcje[nr_atrakcji].nazwa, mes.wagonik);
                        fflush(stdout);
                    }
                }
                // Potwierdź rezygnację
                mes.mtype = mes.ack;
                mes.ack = 0;
                msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);
            }

            // ===== SPRAWDZANIE ZAKOŃCZONYCH JAZD =====

            for (int i = 0; i < iloscWagonikow; i++) {
                if (czasyJazdy[i].zajete==false) {continue;}

                if (curTime >= czasyJazdy[i].czasJazdy || !g_park->park_otwarty) {
                    for (pid_t pid : czasyJazdy[i].pids) {
                        ACKmes mes_out;
                        mes_out.mtype = pid;
                        mes_out.ack = 0; // Sukces
                        msgsnd(wejscieDoAtrakcji, &mes_out, sizeof(mes_out) - sizeof(long), 0);
                    }

                    int ilosc_klientow = czasyJazdy[i].pids.size();
                    log_message(logger_id,"%02d:%02d - Atrakcja %s (wagonik %d) zakończona, wypuszczono %d klientów\n",
                                 curTime.hour, curTime.minute,
                                 atrakcje[nr_atrakcji].nazwa, i, ilosc_klientow);
                    // Zwolnij wagonik
                    czasyJazdy[i].pids.clear();
                    czasyJazdy[i].zajete = false;
                    czasyJazdy[i].czasJazdy = SimTime(0, 0);
                }
            }
        // ===== WPUSZCZANIE NOWYCH KLIENTÓW =====
        int freeCart = znajdzWolnyWagonik(czasyJazdy, iloscWagonikow);

        if (freeCart != -1 && !zatrzymano) {
            czasy nowa_jazda;
            nowa_jazda.pids.clear();
            nowa_jazda.zajete = false;
            nowa_jazda.czasJazdy = SimTime(0, 0);

            int wolne_miejsca = atrakcje[nr_atrakcji].po_ile_osob_wchodzi;

            while (wolne_miejsca > 0) {
                ACKmes request;
                ssize_t result = msgrcv(wejscieDoAtrakcji, &request,
                                       sizeof(request) - sizeof(long),
                                       MSG_TYPE_JOIN_ATTRACTION, IPC_NOWAIT);
                if (result == -1) break; // Kolejka pusta
                if (request.ilosc_osob <= wolne_miejsca) {
                    nowa_jazda.pids.push_back(request.ack);
                    wolne_miejsca -= request.ilosc_osob;
                    // Potwierdź wejście
                    request.mtype = request.ack;
                    request.ack = 0; // Sukces
                    request.wagonik = freeCart;
                    msgsnd(wejscieDoAtrakcji, &request, sizeof(request) - sizeof(long), 0);
                } else {
                    // Brak miejsca
                    request.mtype = request.ack;
                    request.ack = -1; // Brak miejsca
                    msgsnd(wejscieDoAtrakcji, &request, sizeof(request) - sizeof(long), 0);
                }
            }
            // Jeśli ktoś wszedł - uruchom atrakcję
            if (!nowa_jazda.pids.empty()) {
                curTime = getTime();
                nowa_jazda.czasJazdy = curTime + SimTime(0, atrakcje[nr_atrakcji].czas_trwania_min);
                nowa_jazda.zajete = true;
                czasyJazdy[freeCart] = nowa_jazda;

                int zajete = atrakcje[nr_atrakcji].po_ile_osob_wchodzi - wolne_miejsca;
                uruchomione_atrakcje++;

                log_message(logger_id,"%02d:%02d - URUCHOMIENIE %s (wagonik %d): %d osób w %d grupach, koniec o %02d:%02d\n",
                             curTime.hour, curTime.minute,
                             atrakcje[nr_atrakcji].nazwa, freeCart,
                             zajete, (int)nowa_jazda.pids.size(),
                             nowa_jazda.czasJazdy.hour, nowa_jazda.czasJazdy.minute);
                fflush(stdout);
            }
        }
        usleep(5000);
    }

    // ===== ZAKOŃCZENIE PRACY =====
    if (ewakuacja) {
        ewakuuj_wszystkich(wejscieDoAtrakcji, czasyJazdy, iloscWagonikow, nr_atrakcji);
    }
    log_message(logger_id,"KONIEC pracy na atrakcji %s (uruchomiono %d razy)\n",
                     atrakcje[nr_atrakcji].nazwa, uruchomione_atrakcje);
    fflush(stdout);
    delete[] czasyJazdy;
    detach_from_shared_block(g_park);
    return 0;
    }

