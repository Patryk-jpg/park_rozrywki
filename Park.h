//
// Created by janik on 11/12/2025.
//
#ifndef PARK_ROZRYWKI_PARK_H
#define PARK_ROZRYWKI_PARK_H
#include <string>


class Park {


};

class Atrakcja {

    char kod[2] = {};
    int limit_osob = -1;
    int czas_trwania = 0;
    int min_wiek = -1;
    int wiek_z_opiekunem = -1;
    int min_wzrost = -1;
    int wzrost_z_opiekunem = -1;
    bool czySezonowa = false;
    std::string nazwa = "NULL";



};
#endif //PARK_ROZRYWKI_PARK_H