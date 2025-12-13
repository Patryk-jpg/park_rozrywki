//
// Created by janik on 11/12/2025.
//
#ifndef PARK_ROZRYWKI_PARK_H
#define PARK_ROZRYWKI_PARK_H
#include <ctime>
#include <string>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <sys/msg.h>
#define MAX_KLIENTOW_W_PARKU 100
#define CZAS_OTWARCIA 8
#define CZAS_ZAMKNIECIA 20
#define PROCENT_VIP 1
#define BILET_2H 120
#define BILET_4H 240
#define BILET_6H 360
#define BILET_1D 720
#define PARK_SEED 'A'
#define SEED_FILENAME_PARK "/tmp/sharedfile"
#define SEED_FILENAME_QUEUE "/tmp/queuefile"
#define SEED_FILENAME_SEMAPHORES "/tmp/semaphores"
#define SEZON_LETNI true
#define QUEUE_SEED 'Q'
#define SEM_SEED 'S'
#define QUEUE_REST_SEED 'R'

#define IPC_ERROR (-1)
struct Atrakcja {
    int nr;
    char nazwa[50];
    int po_ile_osob_wchodzi;
    int limit_osob;
    int czas_trwania_min;
    int min_wiek;
    int max_wiek;
    int min_wzrost;
    int max_wzrost;
    int wiek_wymaga_opiekuna;
    bool mozna_opuscic;
    bool sezonowa_letnia;
};

struct SimTime {
    int hour = 0;
    int minute = 0;
    SimTime() = default; // default constructor
    SimTime(int h, int m) : hour(h), minute(m) {}
    void increment_minute() {
        minute++;
        if (minute >= 60) {
            minute = 0;
            hour++;
            if (hour >= 24) hour = 0;
        }
    }
    void print() const {
        printf("%02d:%02d\n", hour, minute);
    }
    SimTime operator+(const SimTime& other) const {
        SimTime result;
        result.minute = minute + other.minute;
        result.hour = hour + other.hour + result.minute / 60;
        if (result.hour >= CZAS_ZAMKNIECIA) {
            result.hour = CZAS_ZAMKNIECIA;
            if (result.minute > 0) {
                result.minute = 0;
            }
        }
        return result;
    }
};

class park_wspolne {
    public:
    int liczba_klientow_w_parku;
    int max_klientow;
    bool park_otwarty;
    SimTime czas_w_symulacji;
    pid_t pid_parku;

    void uruchom_pracownikow();
    void uruchom_kase();
    void uruchom_kase_restauracji();
};
// Parametry atrakcji


const Atrakcja atrakcje[17] = {
    // A1: Wodna bitwa
    {0, "Wodna bitwa", 20,20, 30, 0, 999, 120, 999, 120, true, true},
    // A2: Magiczna pompa
    {1, "Magiczna pompa", 12,12, 30, 0, 999, 100, 999, 120, true, true},
    // A3: Wyprawa do groty
    {2, "Wyprawa do groty",16, 16, 25, 2, 13, 120, 999, 13, true, true},
    // A4: Zatoka bambusowa
    {3, "Zatoka bambusowa", 18,18, 35, 0, 999, 110, 999, 135, true, true},
    // A5: Smocza przygoda
    {4, "Smocza przygoda",14, 14, 20, 2, 13, 130, 999, 13, true, false},
    // A6: Cudowne koło
    {5, "Cudowne kolo", 8,8, 20, 0, 13, 120, 999, 13, false, false},
    // A7: Karuzela
    {6, "Karuzela",12, 12, 15, 2, 13, 130, 190, 13, false, false},
    // A8: Kolejka mała
    {7, "Kolejka mala", 24,24, 15, 0, 999, 100, 999, 120, false, false},
    // A9: Kolejka górska
    {8, "Kolejka gorska", 4,20, 35, 4, 12, 120, 999, 12, false, false},
    // A10: Kolejka smocza
    {9, "Kolejka smocza", 20,20, 30, 4, 13, 120, 999, 13, false, false},
    // A11: Mega Roller Coaster
    {10, "Mega Roller Coaster", 24,24, 30, 0, 999, 140, 195, -1, false, false},
    // A12: Ławka obrotowa
    {11, "Lawka obrotowa", 18,18, 25, 0, 999, 140, 195, -1, false, false},
    // A13: Kosmiczny wzmacniacz
    {12, "Kosmiczny wzmacniacz", 4,8, 20, 0, 999, 140, 195, -1, true, false},
    // A14: Dom potwora
    {13, "Dom potwora", 4,12, 20, 4, 12, 130, 999, 12, true, false},
    // A15: Samochodziki
    {14, "Samochodziki", 2,20, 15, 0, 999, 120, 999, -1, true, false},
    // A16: Przygoda w dżungli
    {15, "Przygoda w dzungli", 9,45, 35, 0, 999, 120, 999, 140, true, false},
{16, "Restauracja",1, 50, 30, 0, 999, 0, 999, -1, true, false}

};

static int get_shared_block_id();
park_wspolne* attach_to_shared_block();
bool detach_from_shared_block(park_wspolne* block);
bool destroy_shared_block(char* filename);

int allocate_semaphore(key_t key, int number, int flag);
int free_semaphore(int SemId, int number);
void initialize_semaphore(int SemId, int number, int val);
int wait_semaphore(int semId, int number, int flags);
void signal_semaphore(int semId, int number);
int read_semaphore(int semId, int number);

void handler_zamknij_park(int sig);

int create_message_queue(const char* filename, int seed);
void add_message_to_message_queue();
void load_message_from_message_queue();

enum typ_biletu {
    BILET2H,
    BILET4H,
    BILET6H,
    BILET1D,
    BILETVIP
};
struct klient_message{
    long mtype;
    int typ_biletu;
    int ilosc_biletow;
    pid_t pid_klienta;
};

struct serwer_message {
    long mtype;
    SimTime start_biletu;
    SimTime end_biletu;
    float cena;
    int status;
};

struct biletInfo {
    int cena;
    int czasTrwania;
    char nazwa[50];
};
const biletInfo bilety[5] = {
{50,2, "BILET2H"},
    {65, 4, "BILET4H"},
{85, 6, "BILET6H"},
{65, 24, "BILET1D"},
{0, 24, "BILETVIP"},
};
void error_check(int id, const std::string& message);
#endif //PARK_ROZRYWKI_PARK_H