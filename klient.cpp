//
// Created by janik on 11/12/2025.
//
#include "park_wspolne.h"
#include "klient.h"

#include <unistd.h>


park_wspolne* g_park = nullptr;
static klient g_klient;


int main(int argc, char* argv[]) {


    srand(time(NULL));
    g_park = attach_to_shared_block();
    if (rand()%100 ==1) {
        g_klient.czyVIP=true;
    }
    g_klient.wiek = 1 + rand()%90;
    g_klient.wzrost = 50 + rand()%151;
    g_klient.pidKlienta = getpid();
    g_klient.typ_biletu = rand() % 4;
    /**printf("Klient: %d, wzrost: %d, wiek: %d, utworzono, %02d:%02d\n",
        g_klient.pidKlienta,
        g_klient.wzrost,
        g_klient.wiek,
        g_park->czas_w_symulacji.hour,
        g_park->czas_w_symulacji.minute);
        fflush(stdout);
    **/
    wejdz_do_parku();

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

void idz_do_atrakcji() {
    int nr_atrakcji =  rand() % 17;
    int atrakcja_id = g_park->pracownicy_keys[nr_atrakcji];
    ACKmes mes;
    mes.mtype = 1; // 1= "chce dolaczyc do atrakcji"
    mes.ack = getpid();
    msgsnd(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), 0);

    //printf("Klient %d czeka w kolejce do %s o godz %02d:%02d \n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //    ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);

    msgrcv(atrakcja_id, &mes, sizeof(ACKmes) - sizeof(long), g_klient.pidKlienta, 0);
    if (mes.ack == -1){ return; }

    //printf("Klient %d bawi się na %s o godz %02d:%02d \n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //    ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);

    msgrcv(atrakcja_id, &mes , sizeof(ACKmes) - sizeof(long), g_klient.pidKlienta, 0);
    if (mes.ack == -10) {
        printf("Klient wypada z wagoniku i umiera ");
        kill(getpid(), SIGKILL);
    }
    //printf("Klient %d wyszedł z %s, o godzinie %02d:%02d\n", g_klient.pidKlienta, atrakcje[nr_atrakcji].nazwa
    //     ,g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute);


}

void baw_sie() {
    idz_do_atrakcji();
    while (g_park->czas_w_symulacji.hour <  g_klient.czasWyjscia.hour) {
        usleep(100000);
    }
    wyjdz_z_parku();
}

void wyjdz_z_parku() {

    signal_semaphore(g_park->licznik_klientow, 0);
    printf("Klient %d wychodzi z parku CZAS: %02d:%02d \n", g_klient.pidKlienta, g_park->czas_w_symulacji.hour,g_park->czas_w_symulacji.minute );

}
