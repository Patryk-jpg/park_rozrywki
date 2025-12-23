
#include "park_wspolne.h"
#include "klient.h"

#include <unistd.h>


park_wspolne* g_park = nullptr;
static klient g_klient;

bool contains(std::vector<int> anulowalne, int val) {
    for (int i=0; i<anulowalne.size(); i++) {
        if (val == anulowalne[i]) return true;

    }
    return false;
}
int main(int argc, char* argv[]) {

    init_random();
    g_park = attach_to_shared_block();
    if (random_chance(1)) {
        g_klient.czyVIP=true;
    }
    g_klient.wiek = random_int(1, 90);
    g_klient.wzrost = random_int(50, 200);
    g_klient.pidKlienta = getpid();
    g_klient.typ_biletu = random_int(0, 3);
    g_klient.czasWRestauracji = SimTime(0,0);
    /**printf("Klient: %d, wzrost: %d, wiek: %d, utworzono, %02d:%02d\n",
        g_klient.pidKlienta,
        g_klient.wzrost,
        g_klient.wiek,
        g_park->czas_w_symulacji.hour,
        g_park->czas_w_symulacji.minute);
        fflush(stdout);
    **/
    if (random_chance(5)) {

        idz_do_atrakcji(16);
        if (random_chance(95)) {
            g_klient.odwiedzone.push_back(16);
        }
    }
    wejdz_do_parku();

    if (random_chance(5)) {
        idz_do_atrakcji(16);
    }
    detach_from_shared_block(g_park);

}
void wejdz_do_parku() {

    klient_message k_msg;
    serwer_message reply;
    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    typ_biletu bilet;
    k_msg.mtype = 5;
    k_msg.typ_biletu = g_klient.typ_biletu;
    if (g_klient.czyVIP == true) {
        k_msg.mtype = 1;
        k_msg.typ_biletu = 4;
    }
    k_msg.ilosc_biletow = 1;
    k_msg.pid_klienta = g_klient.pidKlienta;
    //printf("Klient %d - zapisuje sie do kolejki\n", g_klient.pidKlienta);
    msgsnd(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), 0);

    msgrcv(kasaId, &reply, sizeof(reply) - sizeof(long), g_klient.pidKlienta, 0);

   // printf("Klient %d wychodzi z kolejki\n",g_klient.pidKlienta);
    if (reply.status == -1) {
        printf("Nie udalo sie wejsc do parku, klient %d ucieka\n", g_klient.pidKlienta);
        signal_semaphore(g_park->licznik_klientow, 0);
        return;
    }
    g_klient.czasWejscia = reply.start_biletu;
    g_klient.cena = reply.cena;
    g_klient.czasWyjscia = reply.end_biletu;
    //printf("Klient %d w parku z biletem %s, wychodzi o %02d:%02d "
    //       "Ilosc ludzi w parku: %d \n",g_klient.pidKlienta, bilety[g_klient.typ_biletu].nazwa,
    //       g_klient.czasWyjscia.hour, g_klient.czasWyjscia.minute
    //       ,MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow,0));
    g_klient.wParku = true;
    baw_sie();

}

void idz_do_atrakcji(int nr_atrakcji) {

    int atrakcja_id = g_park->pracownicy_keys[nr_atrakcji];
    ACKmes mes;
    mes.mtype = 100; // 100 = "chce dolaczyc do atrakcji"
    mes.ack = getpid();
    mes.wagonik =0;
    msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), 0);

    //printf("Klient %d czeka w kolejce do %s o godz %02d:%02d \n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //    ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);

    msgrcv(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), g_klient.pidKlienta, 0);
    if (mes.ack == -1){ return; }
    int moj_wagonik = mes.wagonik;
    SimTime czas_rozpoczecia;
    if (nr_atrakcji == 16) { // Restauracja
        czas_rozpoczecia = g_park->czas_w_symulacji;
    }
    printf("Klient %d bawi się na %s o godz %02d:%02d \n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
        ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);
    std::vector<int> anulowalne = {1,2,3,4,5,13,14,15,16,17};
    bool czyZrezygnuje = random_chance(10);
    if (nr_atrakcji == 16) {czyZrezygnuje = true;}
    if (contains(anulowalne, nr_atrakcji) && czyZrezygnuje) {
        SimTime timeout = g_park->czas_w_symulacji + SimTime(0, random_int(5,atrakcje[nr_atrakcji].czas_trwania_min));

        while (g_park->czas_w_symulacji <= timeout) {
            int mess = msgrcv(atrakcja_id, &mes , sizeof(ACKmes) - sizeof(long), g_klient.pidKlienta, IPC_NOWAIT);
            if (mess != -1) {break;}
            usleep(10000);

        }
        mes.mtype = 99; // rezygnuje z atrakcji
        mes.ack = g_klient.pidKlienta;
        mes.wagonik = moj_wagonik;
        msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), 0);
        msgrcv(atrakcja_id, &mes , sizeof(ACKmes) - sizeof(long), g_klient.pidKlienta, 0);
    }
    else {
        msgrcv(atrakcja_id, &mes , sizeof(ACKmes) - sizeof(long), g_klient.pidKlienta, 0);
    }
    if (mes.ack == -10) {
        printf("Klient wypada z wagoniku i umiera ");
        kill(getpid(), SIGKILL);
    }
    if (nr_atrakcji == 16) {
        SimTime czas_zakonczenia = g_park->czas_w_symulacji;
        SimTime czas_pobytu = SimTime(czas_zakonczenia.hour - czas_rozpoczecia.hour,
                          czas_zakonczenia.minute - czas_rozpoczecia.minute);
        g_klient.czasWRestauracji = g_klient.czasWRestauracji + czas_pobytu;

        printf("Klient %d był w restauracja %d minut (łącznie: %d min)\n",
               g_klient.pidKlienta, czas_pobytu.toMinutes(), g_klient.czasWRestauracji.toMinutes());
        if (!g_klient.wParku) {
            zaplac_za_restauracje_z_zewnatrz(czas_pobytu.toMinutes());
        }
    }

    //printf("Klient %d wyszedł z %s, o godzinie %02d:%02d\n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //     ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);


}

void baw_sie() {
    while (g_park->czas_w_symulacji.hour < g_klient.czasWyjscia.hour ||
       (g_park->czas_w_symulacji.hour == g_klient.czasWyjscia.hour &&
        g_park->czas_w_symulacji.minute < g_klient.czasWyjscia.minute)) {
        int nr_atrakcji =  random_int(0, 16);

        while (contains(g_klient.odwiedzone, nr_atrakcji) && g_klient.odwiedzone.size() <= 16) {
            nr_atrakcji =  random_int(0, 16);

        }
        idz_do_atrakcji(nr_atrakcji);
        if (!random_chance(95)) {
            g_klient.odwiedzone.push_back(nr_atrakcji);

        }


        usleep(100000);
    }

    wyjdz_z_parku();
}

void wyjdz_z_parku() {

    signal_semaphore(g_park->licznik_klientow, 0);
    printf("Klient %d wychodzi z parku CZAS: %02d:%02d \n", g_klient.pidKlienta, g_park->czas_w_symulacji.hour,g_park->czas_w_symulacji.minute );

}
void  zaplac_za_restauracje_z_zewnatrz(int czas_pobytu) {
    printf("Klient %d idzie do kasy w restauracji zaplacic (bez wejścia do parku) o %02d:%02d\n",
             g_klient.pidKlienta,
             g_park->czas_w_symulacji.hour,
             g_park->czas_w_symulacji.minute);


    int kasa_rest_id = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED);
    restauracja_message msg;
    msg.mtype = 1; // Żądanie płatności
    msg.pid_klienta = g_klient.pidKlienta;
    msg.czas_pobytu_min = czas_pobytu;
    msgsnd(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 0);
    msgrcv(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), g_klient.pidKlienta, 0);
    printf("Klient %d wychodzi z restauracji, zapłacił %.2f zł o %02d:%02d\n",
        g_klient.pidKlienta,
        msg.kwota,
        g_park->czas_w_symulacji.hour,
        g_park->czas_w_symulacji.minute);

}