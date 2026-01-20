#pragma once
#define PRINT_ERROR(msg) print_error_impl(__FILE__, __LINE__, __func__, msg)



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
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <iostream>
#define LOG_MSG   1
#define STOP_MSG  2
// STAŁE KONFIGURACYJNE
#define MAX_KLIENTOW_W_PARKU 1000
#define CZAS_OTWARCIA 8
#define CZAS_ZAMKNIECIA 20
#define PROCENT_VIP 1
#define LICZBA_ATRAKCJI 17
#define MINUTA 10000
// CZASY BILETÓW (w minutach)
#define BILET_2H 120
#define BILET_4H 240
#define BILET_6H 360
#define BILET_1D 720

// KLUCZE IPC
#define PARK_SEED 'A'
#define SEED_FILENAME_PARK "/tmp/sharedfile"
#define SEED_FILENAME_QUEUE "/tmp/queuefile"
#define SEED_FILENAME_SEMAPHORES "/tmp/semaphores"
#define LOG_FILE "raport.txt"
#define QUEUE_SEED 'Q'
#define SEM_SEED 'S'
#define QUEUE_REST_SEED 'R'
#define IPC_ERROR (-1)
#define TIME_SCALE 1

// ===== TYPY KOMUNIKATÓW =====
#define MSG_TYPE_VIP_TICKET 10           // VIP - priorytet
#define MSG_TYPE_STANDARD_TICKET 15      // Zwykły bilet
#define MSG_TYPE_JOIN_ATTRACTION 100    // Wejście do atrakcji
#define MSG_TYPE_QUIT_ATTRACTION 99     // Rezygnacja z atrakcji
#define MSG_TYPE_EXIT_PAYMENT 1       // Płatność przy wyjściu
#define MSG_TYPE_END_QUEUE 105 //zakoncz kolejke
#define MSG_TYPE_CLEAR_QUEUE 104 //zakoncz kolejke
#define MAX_W_KOLEJCE 512
#define SEZON_LETNI true


static_assert(MAX_KLIENTOW_W_PARKU > 0, "maks klientów nie moze byc ujemna");
static_assert(LICZBA_ATRAKCJI >= 17, "program obsługuje maksymalnie 17 atrakcji");
static_assert(CZAS_OTWARCIA < CZAS_ZAMKNIECIA, "czas otwarcia wiekszy od zamkniecia");
static_assert(PROCENT_VIP  < 100, "procent nie moze byc wiekszy od 100%");

// STRUCTY
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
    int wzrost_wymaga_opiekuna;
    bool mozna_opuscic;
    bool sezonowa_letnia;
};



void print_error_impl(const char* file, int line, const char* func, const std::string& msg);

struct SimTime {
    time_t start_time = 0;
    int hour = 0;
    int minute = 0;

    SimTime() = default;
    SimTime(int h, int m) : hour(h), minute(m) {}

    void increment_minute() {
        minute++;
        if (minute >= 60) {
            minute = 0;
            hour++;
            if (hour >= 24) hour = 0;
        }
    }
    void update() {
        time_t now = time(nullptr) - start_time;
        int now_sim = now * TIME_SCALE;
        int total_minutes = now_sim * TIME_SCALE;
        hour = CZAS_OTWARCIA + total_minutes / 60;
        minute = total_minutes % 60;
        if (hour  >= CZAS_ZAMKNIECIA) {
            hour  = CZAS_ZAMKNIECIA;
            minute = 0;
        }
    }
    void print() const {
        printf("%02d:%02d\n", hour, minute);
    }

    int toMinutes() const {
        return hour * 60 + minute;
    }

    SimTime operator+(const SimTime& other) const;

    bool operator<=(const SimTime& other) const {
        return toMinutes() <= other.toMinutes();
    }

    bool operator>=(const SimTime& other) const {
        return toMinutes() >= other.toMinutes();
    }

    bool operator<(const SimTime& other) const {
        return toMinutes() < other.toMinutes();
    }

    bool operator>(const SimTime& other) const {
        return toMinutes() > other.toMinutes();
    }
};

struct park_wspolne {
    bool park_otwarty;
    SimTime czas_w_symulacji;
    int pracownicy_keys[LICZBA_ATRAKCJI + LICZBA_ATRAKCJI];
    int park_sem;
    int clients_count;
    int logger_id;
    int kasa_reply_id;
    int kasa_rest_reply_id;
};

// Parametry atrakcji

const Atrakcja atrakcje[17] = {
    // A1: Wodna bitwa
    {0, "Wodna bitwa", 20,20, 30, 0, 999, 120, 999, -1,140, true, true},
    // A2: Magiczna pompa
    {1, "Magiczna pompa", 12,12, 30, 0, 999, 100, 999, -1,120, true, true},
    // A3: Wyprawa do groty
    {2, "Wyprawa do groty",16, 16, 25, 2, 999, 120, 999, 13, -1,true, true},
    // A4: Zatoka bambusowa
    {3, "Zatoka bambusowa", 18,18, 35, 0, 999, 110, 999, -1,135, true, true},
    // A5: Smocza przygoda
    {4, "Smocza przygoda",14, 14, 20, 2, 999, 130, 999, 13,-1, true, false},
    // A6: Cudowne koło
    {5, "Cudowne kolo", 8,8, 20, 0, 999, 120, 999, 13,-1,false, false},
    // A7: Karuzela
    {6, "Karuzela",12, 12, 15, 2, 999, 130, 190, 13, -1, false, false},
    // A8: Kolejka mała
    {7, "Kolejka mala", 24,24, 15, 0, 999, 100, 999, -1, 120, false, false},
    // A9: Kolejka górska
    {8, "Kolejka gorska", 1,MAX_KLIENTOW_W_PARKU, 30, 4, 999, 120, 999, 12, -1, false, false},
    // A10: Kolejka smocza
    {9, "Kolejka smocza", 20,20, 30, 4, 999, 120, 999, 13, -1, false, false},
    // A11: Mega Roller Coaster
    {10, "Mega Roller Coaster", 24,24, 30, 0, 999, 140, 195, -1, -1, false, false},
    // A12: Ławka obrotowa
    {11, "Lawka obrotowa", 18,18, 25, 0, 999, 140, 195, -1, -1,false, false},
    // A13: Kosmiczny wzmacniacz
    {12, "Kosmiczny wzmacniacz", 4,8, 20, 0, 999, 140, 195, -1, -1,true, false},
    // A14: Dom potwora
    {13, "Dom potwora", 4,12, 20, 4, 999, 130, 999, 12,-1, true, false},
    // A15: Samochodziki
    {14, "Samochodziki", 2,20, 15, 0, 999, 120, 999, -1, -1,true, false},
    // A16: Przygoda w dżungli
    {15, "Przygoda w dzungli", 9,45, 35, 0, 999, 120, 195, -1,140, true, false},
{16, "Restauracja",1, 50, 60, 0, 999, 0, 999, -1,-1, true, false}

};

// ===== BILETY =====
enum typ_biletu {
    BILET2H,
    BILET4H,
    BILET6H,
    BILET1D,
    BILETVIP
};

struct biletInfo {
    int cena;
    int czasTrwania;  // w godzinach
    char nazwa[50];
};

const biletInfo bilety[5] = {
    {50, 2, "BILET2H"},
    {100, 4, "BILET4H"},
    {85, 6, "BILET6H"},
    {65, 24, "BILET1D"},
    {0, 24, "BILETVIP"},
};

// ===== KOMUNIKATY IPC =====

// Wiadomość od klienta do kasy (zakup biletu)
struct klient_message {
    //long mtype;                    // 1=VIP, 5=normalny
    int typ_biletu;
    int ilosc_biletow;
    int ilosc_osob;
    pid_t pid_klienta;
};

// Odpowiedź od kasy do klienta
struct serwer_message {
    //long mtype;                    // PID klienta
    SimTime start_biletu;
    SimTime end_biletu;
    float cena;
    int typ_biletu;
    int status;                    // 0=OK, -1=błąd
    int ilosc_osob;
};

// Płatność przy wyjściu
struct payment_message {
    //long mtype;                    // 101=żądanie, PID=odpowiedź
    pid_t pid;
    int czasWRestauracji;
    float suma;
    int wiekDziecka;              // -1 jeśli brak dziecka
    SimTime czasWyjscia;
};

// Komunikat do/z atrakcji
struct ACKmes {
    long mtype;                    // 100=wejście, 99=rezygnacja, PID=odpowiedź
    int ack;                       // Status: 0=OK, -1=brak miejsca, -2=zatrzymano, -3=ewakuacja
    int wagonik;                   // Numer przydzielonego wagonika
    int ilosc_osob;
};

// Komunikat restauracji
struct restauracja_message {
    long mtype;                    // 1=żądanie, PID=odpowiedź
    pid_t pid_klienta;
    int czas_pobytu_min;
    float kwota;
};

struct LogMessage {
    long mtype;
    char message[128];
};

struct kasa_message {
    long mtype;
    union {
        klient_message klient;
        serwer_message serwer;
        payment_message payment;
    };
};

// ===== FUNKCJE POMOCNICZE =====

void print_error_impl(const char* file, int line, const char* func, const std::string& msg);

// IPC - pamięć współdzielona
int get_shared_block_id();
park_wspolne* attach_to_shared_block();
bool detach_from_shared_block(park_wspolne* block);
bool destroy_shared_block(char* filename);

// IPC - kolejki komunikatów
int create_message_queue(const char* filename, int seed, int msgflg);
int join_message_queue(const char* filename, int seed);

// IPC - semafory
int allocate_semaphore(key_t key, int number, int flag);
int free_semaphore(int SemId, int number);
void initialize_semaphore(int SemId, int number, int val);
int wait_semaphore(int semId, int number, int flags);
void signal_semaphore(int semId, int number);
int read_semaphore(int semId, int number);

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

// Obliczanie kosztów
float oblicz_koszt_restauracji(int czas_min);

// Generator liczb losowych
#include <random>
extern std::mt19937 rng;

 void init_random();

inline int random_int(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

inline bool random_chance(int percent) {
    return random_int(0, 99) < percent;
}

// Sprawdzanie poprawności
void error_check(int id, const std::string& message);
void log_message(int color,int logger_id,const char* format, ...);
void end_logger(int logger_id);

inline const char* ANSI_COLORS[] = {
    "\033[30m", // czarny
    "\033[36m", // cyan
    "\033[32m", // zielony
    "\033[33m", // żółty
    "\033[34m", // niebieski
    "\033[35m", // magenta
    "\033[37m", // biały
};
