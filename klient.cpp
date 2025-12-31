
#include "park_wspolne.h"
#include "klient.h"
#include <pthread.h>

#include <unistd.h>

static volatile sig_atomic_t ewakuacja = 0;
void sigint_handler(int sig) {
    ewakuacja = 1;
}
park_wspolne* g_park = nullptr;
static klient g_klient;

SimTime getTime() {
    wait_semaphore(g_park->park_sem,0,0);
    SimTime curTime = g_park->czas_w_symulacji;
    signal_semaphore(g_park->park_sem,0);
    return curTime;
}

bool park_otwarty() {
    wait_semaphore(g_park->park_sem,0,0);
    bool open = g_park->park_otwarty;
    signal_semaphore(g_park->park_sem,0);
    return open;
}


bool contains(const std::vector<int>& lista, int val) {
    for (int i=0; i<lista.size(); i++) {
        if (val == lista[i]) return true;

    }
    return false;
}
int main(int argc, char* argv[]) {

    init_random();
    g_park = attach_to_shared_block();
    if (random_chance(1)) {
        g_klient.czyVIP=true;
    }
    g_klient.wiek = random_int(13, 90);
    g_klient.wzrost = g_klient.wiek<18? random_int(50, 180) : random_int(150,200);
    g_klient.pidKlienta = getpid();
    g_klient.typ_biletu = random_int(0, 3);
    g_klient.czasWRestauracji =0;
    /**printf("Klient: %d, wzrost: %d, wiek: %d, utworzono, %02d:%02d\n",
        g_klient.pidKlienta,
        g_klient.wzrost,
        g_klient.wiek,
        g_park->czas_w_symulacji.hour,
        g_park->czas_w_symulacji.minute);
        fflush(stdout);
    **/
    if (g_klient.wiek > 18) {
        g_klient.ma_dziecko = random_chance(30);
        if (g_klient.ma_dziecko) {

            g_klient.dzieckoInfo = new dziecko();
            g_klient.dzieckoInfo->wiek = random_int(1,13);
             g_klient.dzieckoInfo->wzrost = random_int(50,170);

        }
    }

    g_klient.ilosc_osob = g_klient.ma_dziecko ? 2  : 1;
    if (random_chance(5)) {

        idz_do_atrakcji(16, g_klient.pidKlienta);
        if (random_chance(95)) {
            g_klient.odwiedzone.push_back(16);
        }
    }
    wejdz_do_parku();

    if (random_chance(5)) {
        idz_do_atrakcji(16,g_klient.pidKlienta);
    }
    if (g_klient.dzieckoInfo) {
        delete g_klient.dzieckoInfo;
        g_klient.dzieckoInfo = nullptr;
    }
    detach_from_shared_block(g_park);
    _exit(0);
}
void wejdz_do_parku() {
    if (!park_otwarty()) {
        return;
    }
    klient_message k_msg{};
    serwer_message reply;
    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    if (kasaId == -1) {
        printf("Kasa zamknięta, klient %d nie wchodzi\n", g_klient.pidKlienta);
        return;
    }
    typ_biletu bilet;
    k_msg.mtype = 5;
    k_msg.ilosc_osob = g_klient.ilosc_osob;
    k_msg.typ_biletu = g_klient.typ_biletu;
    if (g_klient.czyVIP == true) {
        k_msg.mtype = 1;
        k_msg.typ_biletu = 4;
    }
    k_msg.ilosc_biletow = g_klient.ilosc_osob;
    k_msg.pid_klienta = g_klient.pidKlienta;
    printf("Klient %d - zapisuje sie do kolejki do parku\n", g_klient.pidKlienta);
    fflush(stdout);
    msgsnd(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), 0);

    msgrcv(kasaId, &reply, sizeof(reply) - sizeof(long), g_klient.pidKlienta, 0);

    printf("Klient %d wychodzi z kolejki\n",g_klient.pidKlienta);
    fflush(stdout);
    if (reply.status == -1) {
        printf("Nie udalo sie wejsc do parku, klient %d ucieka\n", g_klient.pidKlienta);
        return;
    }
    g_klient.czasWejscia = reply.start_biletu;
    g_klient.cena = reply.cena;
    g_klient.czasWyjscia = reply.end_biletu;
    wait_semaphore(g_park->park_sem,0,0);
        SimTime curTime = g_park->czas_w_symulacji;
        int wartoscLicznika = read_semaphore(g_park->licznik_klientow,0);
    signal_semaphore(g_park->park_sem,0);
    printf("%02d:%02d:Klient %d w parku z biletem %s, wyjdzie o %02d:%02d "
           "Ilosc ludzi w parku: %d \n",curTime.hour, curTime.minute,g_klient.pidKlienta, bilety[g_klient.typ_biletu].nazwa,
           g_klient.czasWyjscia.hour, g_klient.czasWyjscia.minute
           ,MAX_KLIENTOW_W_PARKU - wartoscLicznika);
    g_klient.wParku = true;
    baw_sie();

}

int idz_do_atrakcji(int nr_atrakcji, pid_t identifier) {
    if (!park_otwarty()) {
        return -3;
    }
    int atrakcja_id = g_park->pracownicy_keys[nr_atrakcji];
    if (atrakcja_id <= 0) {
        printf("Klient %d: atrakcja %d zamknięta (kolejka nie istnieje)\n",
               identifier, nr_atrakcji);
        return -1;
    }
    ACKmes mes;
    mes.mtype = 100; // 100 = "chce dolaczyc do atrakcji"
    mes.ilosc_osob = g_klient.ilosc_osob;
    mes.ack = identifier;
    mes.wagonik =0;
    msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), IPC_NOWAIT);
    if (errno == EIDRM || errno == EINVAL) {
        printf("Klient %d: atrakcja %d zamknięta (kolejka usunięta)\n",
               identifier, nr_atrakcji);
        return -1;
    }

    // printf("Klient %d czeka w kolejce do %s o godz %02d:%02d \n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //     ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);

    msgrcv(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), identifier, 0);
    // printf("Klient %d wychodzi z kolejki do %s o godz %02d:%02d \n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //      ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);

    if (mes.ack == -1 || mes.ack == -2 || mes.ack == -3) {
        return mes.ack;
    }
    int moj_wagonik = mes.wagonik;

    SimTime czas_rozpoczecia = getTime();

    printf("Klient %d bawi się na %s o godz %02d:%02d \n", identifier, atrakcje[nr_atrakcji].nazwa ,czas_rozpoczecia.hour, czas_rozpoczecia.minute);

    std::vector<int> anulowalne = {1,2,3,4,5,13,14,15,16,17};
    bool czyZrezygnuje = random_chance(10);
    if (nr_atrakcji == 16) {czyZrezygnuje = true;}
    if (contains(anulowalne, nr_atrakcji) && czyZrezygnuje) {
        SimTime timeout = czas_rozpoczecia + SimTime(0, random_int(5,atrakcje[nr_atrakcji].czas_trwania_min));

        SimTime curTime = czas_rozpoczecia;
        while (curTime <= timeout) {

            curTime = getTime();
            int mess = msgrcv(atrakcja_id, &mes , sizeof(ACKmes) - sizeof(long), identifier, IPC_NOWAIT);

            if (mess != -1 ) {return mes.ack;}
            usleep(10000);

        }
        mes.mtype = 99; // rezygnuje z atrakcji
        mes.ack = identifier;
        mes.wagonik = moj_wagonik;
        msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), 0);
        msgrcv(atrakcja_id, &mes , sizeof(ACKmes) - sizeof(long), identifier, 0);
        if (mes.ack == -1 || mes.ack == -2 || mes.ack == -3) {
            return mes.ack;
        }
    }
    else {
        msgrcv(atrakcja_id, &mes , sizeof(ACKmes) - sizeof(long), identifier, 0);
        if (mes.ack == -1 || mes.ack == -2 || mes.ack == -3) {
            return mes.ack;
        }
    }

    if (nr_atrakcji == 16) {

        SimTime czas_zakonczenia =getTime();
        int delta_hour = czas_zakonczenia.hour - czas_rozpoczecia.hour;
        int delta_min = czas_zakonczenia.minute - czas_rozpoczecia.minute;
        if (delta_min < 0) {
            delta_hour--;
            delta_min += 60;
        }
        SimTime czas_pobytu = SimTime(delta_hour, delta_min);
        g_klient.czasWRestauracji += czas_pobytu.toMinutes();

        printf("Klient %d był w restauracja %d minut (łącznie: %d min)\n",
               identifier, czas_pobytu.toMinutes(), g_klient.czasWRestauracji);
        if (!g_klient.wParku) {
            zaplac_za_restauracje_z_zewnatrz(czas_pobytu.toMinutes());
        }
    }

    //printf("Klient %d wyszedł z %s, o godzinie %02d:%02d\n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //     ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);

    return 0;
}


void baw_sie() {
    if (!park_otwarty()) {
        wyjdz_z_parku();
        return;
    }
    SimTime curTime = getTime();
    while (curTime.hour < g_klient.czasWyjscia.hour ||
       (curTime.hour == g_klient.czasWyjscia.hour &&
        curTime.minute < g_klient.czasWyjscia.minute)) {

        if (!park_otwarty()) {
            wyjdz_z_parku();
            return;
        }
        int nr_atrakcji =  random_int(0, 16);

        while (contains(g_klient.odwiedzone, nr_atrakcji) && g_klient.odwiedzone.size() < 17 && !spelniaWymagania(nr_atrakcji)) {
            nr_atrakcji =  random_int(0, 16);

        }
        int status = idz_do_atrakcji(nr_atrakcji, g_klient.pidKlienta);
        if (status == -1) {
            g_klient.odwiedzone.push_back(nr_atrakcji);
        }
        else if (status == -2) {
            continue;
        }
        else if (status == -3) {
            return;
        }
        else {
            if (random_chance(95)) {
                g_klient.odwiedzone.push_back(nr_atrakcji);

            }
        }


        usleep(100000);
        curTime = getTime();
    }

    wyjdz_z_parku();
}

void wyjdz_z_parku() {
    int kasaId = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);

    payment_message k_msg;
    k_msg.czasWRestauracji = g_klient.czasWRestauracji;
    k_msg.pid = g_klient.pidKlienta;
    k_msg.wiekDziecka = g_klient.ma_dziecko ? g_klient.dzieckoInfo->wiek : -1;
    k_msg.czasWyjscia = getTime();

    k_msg.mtype = 101;
    msgsnd(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), 0);
    msgrcv(kasaId, &k_msg, sizeof(k_msg) - sizeof(long), g_klient.pidKlienta, 0);
    SimTime curTime = getTime();
    printf("Klient %d wychodzi z parku CZAS: %02d:%02d \n", g_klient.pidKlienta, curTime.hour,curTime.minute );
    wait_semaphore(g_park->park_sem,0,0);
        for (int i = 0; i < g_klient.ilosc_osob; i++) {
            signal_semaphore(g_park->licznik_klientow, 0);
        }
    signal_semaphore(g_park->park_sem,0);
}
void  zaplac_za_restauracje_z_zewnatrz(int czas_pobytu) {

    SimTime curTime = getTime();
    printf("Klient %d idzie do kasy w restauracji zaplacic (bez wejścia do parku) o %02d:%02d\n",
             g_klient.pidKlienta,
             curTime.hour,
             curTime.minute);

    int kasa_rest_id = join_message_queue(SEED_FILENAME_QUEUE, QUEUE_REST_SEED);
    restauracja_message msg;
    msg.mtype = 1; // Żądanie płatności
    msg.pid_klienta = g_klient.pidKlienta;
    msg.czas_pobytu_min = czas_pobytu * g_klient.ilosc_osob;
    msgsnd(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), 0);
    msgrcv(kasa_rest_id, &msg, sizeof(msg) - sizeof(long), g_klient.pidKlienta, 0);
    curTime = getTime();
    printf("Klient %d wychodzi z restauracji, zapłacił %.2f zł o %02d:%02d\n",
        g_klient.pidKlienta,
        msg.kwota,
        curTime.hour,
        curTime.minute);

}

bool spelniaWymagania(int nr_atrakcji) {
    bool wchodzi = true;
    Atrakcja atrakcja = atrakcje[nr_atrakcji];
    if (!SEZON_LETNI && atrakcja.sezonowa_letnia) {return false;}
    if (g_klient.wiek > atrakcja.max_wiek || g_klient.wiek < atrakcja.min_wiek || g_klient.wiek < atrakcja.wiek_wymaga_opiekuna){return false;} //checki na wiek (wiecej niz max mniej niz min wiek,
    //i jesli klient jest niepelnoletni jako proces to znaczy ze jest bez opiekuna
    if (g_klient.wzrost > atrakcja.max_wzrost || g_klient.wzrost < atrakcja.min_wzrost || g_klient.wzrost < atrakcja.wzrost_wymaga_opiekuna){return false;} //podobnie ale na wzrost
    if (g_klient.ma_dziecko) {
        if (g_klient.dzieckoInfo->wiek > atrakcja.max_wiek || g_klient.dzieckoInfo->wiek < atrakcja.min_wiek){return false;} //jesli klient ma dziecko to znaczy ze jest opiekunem
        if (g_klient.dzieckoInfo->wzrost > atrakcja.max_wzrost || g_klient.dzieckoInfo->wzrost < atrakcja.min_wzrost){return false;}

    }
    return true;

}