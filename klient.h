//
// Created by janik on 11/12/2025.
//

#ifndef PARK_ROZRYWKI_KLIENT_H
#define PARK_ROZRYWKI_KLIENT_H
#include <time.h>
#include <park_wspolne.h>

struct klient
{
    public:
    int wiek;
    int wzrost;
    bool czyVIP;
    pid_t pidKlienta;
    SimTime czasWejscia;
    SimTime czasWyjscia;
    int typ_biletu;
    int cena;

};

void wejdz_do_parku();


#endif //PARK_ROZRYWKI_KLIENT_H