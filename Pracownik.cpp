

#include "Pracownik.h"

#include <algorithm>
#include <park_wspolne.h>
#include <cstdio>
#include <unistd.h>
struct czasy {
    SimTime czasJazdy;
    std::vector<pid_t> pids;
    bool zajete;
};
int znajdzWolnyWagonik(czasy czasyJazdy[], int iloscWagonikow) {
    for (int i = 0; i < iloscWagonikow; i++) {
        if (czasyJazdy[i].zajete == false) return i;
    }
    return -1;
}
int main(int argc, char* argv[]) {
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
    park_wspolne* g_park = attach_to_shared_block();
    struct msqid_ds buf;
    memset(&buf, 0, sizeof(buf));
    ACKmes mes;
    int wejscieDoAtrakcji = g_park->pracownicy_keys[nr_atrakcji];
    int iloscWagonikow = atrakcje[nr_atrakcji].limit_osob/ atrakcje[nr_atrakcji].po_ile_osob_wchodzi;

    czasy czasyJazdy[iloscWagonikow];
    for (int i = 0; i < iloscWagonikow; i++) {
        czasyJazdy[i].czasJazdy = SimTime(0, 0);
        czasyJazdy[i].zajete =  false;

    }

    while (g_park->park_otwarty) {
        SimTime curTime = SimTime(g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);

            while (msgrcv(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 99, IPC_NOWAIT) != -1) {
                auto it = std::find(czasyJazdy[mes.wagonik].pids.begin(),
                    czasyJazdy[mes.wagonik].pids.end(), mes.ack);
                czasyJazdy[mes.wagonik].pids.erase(it);
                mes.mtype = mes.ack;
                msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0); //potwierdzenie
                printf("Klient %d zrezygnowal z %s w wagoniku %d\n", mes.ack, atrakcje[nr_atrakcji].nazwa, mes.wagonik);
            }
            for (int i = 0; i < iloscWagonikow; i++) {
                if (czasyJazdy[i].zajete==false) {continue;}
                if (czasyJazdy[i].czasJazdy.hour < curTime.hour || (czasyJazdy[i].czasJazdy.hour == curTime.hour && czasyJazdy[i].czasJazdy.minute <= curTime.minute)) {

                    czasyJazdy[i].czasJazdy = SimTime(0,0);
                    czasyJazdy[i].zajete = false;
                    for (int j=0; j < czasyJazdy[i].pids.size(); j++) {
                        mes.mtype = czasyJazdy[i].pids[j];
                        mes.ack = 0;
                        msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);
                    }
                    czasyJazdy[i].pids.clear();
                    printf("Pracownik-%d zatrzymuje atrakcje %s, o godzinie %02d:%02d w wagoniku %d\n", nr_atrakcji, atrakcje[nr_atrakcji].nazwa
                ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute, i);
                    fflush(stdout);


                }

            }
        int freeCart = znajdzWolnyWagonik(czasyJazdy, iloscWagonikow);
        msgctl(wejscieDoAtrakcji, IPC_STAT, &buf);

        if (freeCart != -1 && buf.msg_qnum > 0) {
            czasy nowa_jazda;
            int countPeople = atrakcje[nr_atrakcji].po_ile_osob_wchodzi;
            while (countPeople > 0 && msgrcv(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 100, IPC_NOWAIT) != -1) {
                mes.mtype = mes.ack;
                mes.wagonik = freeCart;
                msgsnd(wejscieDoAtrakcji, &mes, sizeof(mes) - sizeof(long), 0);
                nowa_jazda.pids.push_back(mes.ack);
                countPeople--;
            }
            if (nowa_jazda.pids.empty()) {continue;}
            nowa_jazda.czasJazdy = SimTime(g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute) + SimTime(0,atrakcje[nr_atrakcji].czas_trwania_min);
            nowa_jazda.zajete = true;
            czasyJazdy[freeCart] = nowa_jazda;
            printf("Pracownik-%d rozpoczyna atrakcje %s, o godzinie %02d:%02d| Ilosc osob: %d | wagon %d\n", nr_atrakcji, atrakcje[nr_atrakcji].nazwa
        ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute, (int)nowa_jazda.pids.size(), freeCart);
            fflush(stdout);

        }

        usleep(10000);

        }
    fflush(stdout);
    _exit(0);
    }

