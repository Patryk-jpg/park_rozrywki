//
// Created by janik on 11/12/2025.
//
#ifndef PARK_ROZRYWKI_PARK_H
#define PARK_ROZRYWKI_PARK_H
#include <ctime>
#include <string>
#define MAX_KLIENTOW_W_PARKU 100
#define CZAS_OTWARCIA 8
#define CZAS_ZAMKNIECIA 20
#define PROCENT_VIP 1
#define BILET_2H 120
#define BILET_4H 240
#define BILET_6H 360
#define BILET_1D 720
#define PARK_SEED "A"
#define SEED_FILENAME "/tmp/sharedfile"



#define IPC_ERROR (-1)
class Park {
    public:
    int liczba_klientow_w_parku;
    int max_klientow;
    bool park_otwarty;
    time_t czas_start_symulacji = time(nullptr);
    pid_t pid_parku;


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

static int get_shared_block_id();
Park* attach_to_shared_block();
bool detach_from_shared_block(Park* block);
bool destroy_shared_block(char* filename);



#endif //PARK_ROZRYWKI_PARK_H