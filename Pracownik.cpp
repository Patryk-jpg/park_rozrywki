
#include "Pracownik.h"

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
    printf("Otrzymano sygnal zatrzymania atrakcji sygnał %d!\n", sig);
   zatrzymano =  1;
}

void sig2handler(int sig) {
    printf("Otrzymano sygnal ponownego uruchomienia atrakcji sygnał %d!\n", sig);
    zatrzymano = 0;
}

void sig3handler(int sig) {
    printf("Otrzymano sygnal SIGINT EWAKUACJA  sygnał %d!\n", sig);
    zatrzymano = 1;
    ewakuacja = 1;
}
int znajdzWolnyWagonik(czasy czasyJazdy[], int iloscWagonikow) {
    for (int i = 0; i < iloscWagonikow; i++) {
        if (czasyJazdy[i].zajete == false) return i;
    }

    return -1;
}
int main(int argc, char* argv[]) {

    signal(SIGUSR1, sig1handler);
    signal(SIGUSR2, sig2handler);
    signal(SIGINT, sig3handler);

    if (argc < 2) {
        fprintf(stderr, "Brak numeru atrakcji!\n");
        return 1;
    }
    int nr_atrakcji = atoi(argv[1]);
    if (nr_atrakcji < 0 || nr_atrakcji >= 17) {
        fprintf(stderr, "Niepoprawny numer atrakcji: %d\n", nr_atrakcji);
        _exit(1);
    }
    printf("[PRACOWNIK-%d] Start obsługi atrakcji: %s (PID: %d)\n",
         nr_atrakcji, atrakcje[nr_atrakcji].nazwa, getpid());
    g_park = attach_to_shared_block();
    struct msqid_ds buf{};

    ACKmes mes;
    int wejscieDoAtrakcji = g_park->pracownicy_keys[nr_atrakcji];
    msgctl(wejscieDoAtrakcji, IPC_STAT, &buf);

    int iloscWagonikow = atrakcje[nr_atrakcji].limit_osob/ atrakcje[nr_atrakcji].po_ile_osob_wchodzi;
    czasy czasyJazdy[iloscWagonikow];
    for (int i = 0; i < iloscWagonikow; i++) {
        czasyJazdy[i].czasJazdy = SimTime(0, 0);
        czasyJazdy[i].zajete =  false;

    }

    while (true) {
        msgctl(wejscieDoAtrakcji, IPC_STAT, &buf);
        wait_semaphore(g_park->park_sem, 0, 0);
            int licznik_klientow = MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0);
            bool otwarty = g_park->park_otwarty;
        signal_semaphore(g_park->park_sem, 0);
        if (licznik_klientow == 0 && !otwarty ) {
            break;
        }

        if (zatrzymano) {
            printf("zatrzymano na %d\n", nr_atrakcji);
            fflush(stdout);
            for (int i = 0; i < iloscWagonikow; i++) {
                if (czasyJazdy[i].zajete) {
                    for (auto pid : czasyJazdy[i].pids) {
                        ACKmes mes;
                        mes.mtype = pid;
                        mes.ack = 0;
                        msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);
                    }
                    czasyJazdy[i].pids.clear();
                    czasyJazdy[i].zajete = false;
                    czasyJazdy[i].czasJazdy = SimTime(0, 0);

                }

            }
            ACKmes mes_queue;
            while (msgrcv(wejscieDoAtrakcji, &mes_queue, sizeof(mes_queue) - sizeof(long), 0, IPC_NOWAIT) != -1) {
                fflush(stdout);

                mes_queue.mtype = mes_queue.ack;
                mes_queue.ack = -2;  // zatrzymano atrakcje wiec sprobuj pozniej
                msgsnd(wejscieDoAtrakcji, &mes_queue, sizeof(mes_queue) - sizeof(long), 0);
            }

            if (ewakuacja) {
                fflush(stdout);

                signal(SIGINT, SIG_DFL);
                raise(SIGINT);
            }
        }
            SimTime curTime = getTime();
            while (msgrcv(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 99, IPC_NOWAIT) != -1) {

                auto it = std::find(czasyJazdy[mes.wagonik].pids.begin(),
                    czasyJazdy[mes.wagonik].pids.end(), mes.ack);
                if (it != czasyJazdy[mes.wagonik].pids.end()) {
                    czasyJazdy[mes.wagonik].pids.erase(it);
                }
                mes.mtype = mes.ack;
                if (zatrzymano) {
                    printf("Wyrzucam z kolejki w atrakcji %d", nr_atrakcji);
                    usleep(10000);
                    mes.ack = -2;
                }
                msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0); //potwierdzenie
                //printf("Klient %d zrezygnowal z %s w wagoniku %d\n", mes.ack, atrakcje[nr_atrakcji].nazwa, mes.wagonik);
            }
            for (int i = 0; i < iloscWagonikow; i++) {
                if (czasyJazdy[i].zajete==false) {continue;}
                if (zatrzymano || czasyJazdy[i].czasJazdy.hour < curTime.hour || (czasyJazdy[i].czasJazdy.hour == curTime.hour && czasyJazdy[i].czasJazdy.minute <= curTime.minute)) {

                    czasyJazdy[i].czasJazdy = SimTime(0,0);
                    czasyJazdy[i].zajete = false;
                    for (int j=0; j < czasyJazdy[i].pids.size(); j++) {
                        mes.mtype = czasyJazdy[i].pids[j];
                        mes.ack = 0;
                        if (zatrzymano) {
                            mes.ack = -2;
                        }
                        msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);
                    }
                    czasyJazdy[i].pids.clear();

                    curTime = getTime();
                    printf("Pracownik-%d zatrzymuje atrakcje %s, o godzinie %02d:%02d w wagoniku %d\n", nr_atrakcji, atrakcje[nr_atrakcji].nazwa
                ,curTime.hour, curTime.minute, i);
                    fflush(stdout);


                }

            }
        int freeCart = znajdzWolnyWagonik(czasyJazdy, iloscWagonikow);

        if (freeCart != -1 && !zatrzymano) {
            czasy nowa_jazda;
            nowa_jazda.pids.clear();
            nowa_jazda.zajete = false;
            nowa_jazda.czasJazdy = SimTime(0, 0);
            int wolne_miejsca = atrakcje[nr_atrakcji].po_ile_osob_wchodzi;
            while (wolne_miejsca > 0 && msgrcv(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 100, IPC_NOWAIT) != -1) {
                mes.mtype = mes.ack;
                mes.wagonik = freeCart;
                if (mes.ilosc_osob <= wolne_miejsca) {
                    nowa_jazda.pids.push_back(mes.ack);
                    wolne_miejsca -= mes.ilosc_osob;
                    mes.ack = 0;
                }
                else {
                    mes.ack = -1;
                }
                msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);

            }
            if (nowa_jazda.pids.empty()) {continue;}
            curTime = getTime();
            nowa_jazda.czasJazdy = SimTime(curTime.hour, curTime.minute) + SimTime(0,atrakcje[nr_atrakcji].czas_trwania_min);
            nowa_jazda.zajete = true;
            czasyJazdy[freeCart] = nowa_jazda;
            int zajete = atrakcje[nr_atrakcji].po_ile_osob_wchodzi - wolne_miejsca;

            //printf("Pracownik-%d rozpoczyna atrakcje %s, o godzinie %02d:%02d| Ilosc osob: %d | grup %d | wagon %d\n", nr_atrakcji, atrakcje[nr_atrakcji].nazwa
        //,curTime.hour, curTime.minute,zajete, (int)nowa_jazda.pids.size(), freeCart);
        //    fflush(stdout);

        }

        usleep(10000);
        if (!g_park->park_otwarty) {
            printf("pracownik dalej pracuje %s", atrakcje[nr_atrakcji].nazwa);
            fflush(stdout);
        }
        }
    printf("Pracownik zkonczyl prace atrakcja %s \n", atrakcje[nr_atrakcji].nazwa);
    fflush(stdout);

    detach_from_shared_block(g_park);
    _exit(0);
    }

