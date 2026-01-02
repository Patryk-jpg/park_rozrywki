
#pragma once

#include "park_wspolne.h"


struct dziecko {
    int wiek;
    int wzrost;
    float czasWRestauracji;
};
struct klient {
public:
    int wiek;
    int wzrost;
    bool czyVIP;
    pid_t pidKlienta;
    SimTime czasWejscia;
    SimTime czasWyjscia;
    int czasWRestauracji;
    int typ_biletu;
    int cena;
    bool wParku = false;
    std::vector<int> odwiedzone;
    bool ma_dziecko = false;
    dziecko* dzieckoInfo;
    int ilosc_osob;
};
bool spelniaWymagania(int nr_atrakcji);

void wejdz_do_parku();
void wyjdz_z_parku();
void baw_sie();
int idz_do_atrakcji(int nr_atrakcji, pid_t identifier);
void zaplac_za_restauracje_z_zewnatrz(int minutes);