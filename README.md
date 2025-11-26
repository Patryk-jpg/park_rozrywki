
# Projekt semestralny — Systemy operacyjne 2025/26

**Autor:** *Patryk Janik*
**Nr albumu:** *155183*
**Repozytorium:** [park_rozrywki](https://github.com/Patryk-jpg/park_rozrywki)

---

##   Temat 9 — Park rozrywki

W pewnej miejscowości działa park rozrywki dostępny w godzinach **Tp–Tk**. Klienci w wieku **1–90 lat** przychodzą losowo w czasie otwarcia.

W danym momencie w parku może znajdować się maksymalnie **N osób**.

Wejście odbywa się po zakupie biletu w kasie (**K**), z wyjątkiem dzieci **poniżej 2 lat**, które wchodzą bezpłatnie.

---

##   Rodzaje biletów (czasowe)

Każdy klient kupuje bilet uprawniający do pobytu w parku:

* 2h
* 4h
* 6h
* 1 dzień (1D)

Po przekroczeniu wykupionego czasu **Ti** obowiązuje dopłata:
  proporcjonalna do przekroczenia
  o **100% wyższa** niż podstawowa cena
  nie dotyczy VIP

---

##   Klienci VIP

* ok. **1% populacji odwiedzających**
* posiadają wcześniej wykupiony karnet
* mogą wejść **bez kolejki**
* **nie płacą za wejście**
* **nie mają limitu czasu**
* mogą wyjść kiedy chcą
 

**  Kasa rejestruje wszystkie wchodzące osoby (ID procesu/wątku)
oraz wszystkie osoby wychodzące. **

---

##  Atrakcje parku i ograniczenia


| Kod | Nazwa                | Limit osób | Czas     | Ograniczenia  wzrostowe       | Ograniczenia wiekowe | Inne                      |
| --- | -------------------- | ---------- | -------- | ----------------------------- | -------------------- | --------------------      |
| A1  | Wodna bitwa          | 20         | 30 min   | wzrost 120–140 cm z opiekunem |    -/-              | otwarte w sezonie letnim  | 
| A2  | Magiczna pompa       | 12         | 30 min   | wzrost 100–120 cm z opiekunem |      -/-               | otwarte w sezonie letnim  |
| A3  | Wyprawa do groty     | 16         | 25 min   |  wzrost od 120 cm             |wiek 2–13 z opiekunem |otwarte w sezonie letnim   | 
| A4  | Zatoka bambusowa     | 18         | 35 min   | wzrost 110–135 cm z opiekunem |        brak          |otwarte w sezonie letnim   | 
| A5  | Smocza przygoda      | 14         | 20 min   |  wzrost od 130 cm             | wiek 2–13 z opiekunem|                  -/-      |
| A6  | Cudowne koło         | 8          | 20 min   |      wzrost od 120 cm         | wiek 0–13 z opiekunem|   -/-                     | 
| A7  | Karuzela             | 12         | 15 min   | wzrost 130 - 190 cm           | wiek 2–13 z opiekunem| -/-                       | 
| A8  | Kolejka mała         | 24         | 15 min   | wzrost 100–120 cm z opiekunem |        -/-             |                  -/-      |
| A9  | Kolejka górska       | 5 wagoników po 4         | 35 min   |    wzrost od 120 cm           | wiek 4–12 z opiekunem|          -/-|
| A10 | Kolejka smocza       | -          | -        |wzrost od 120 cm               |wiek 4–13 z opiekunem |  -/-                      |
| A11 | Mega Roller Coaster  | 24         | 30 min   | wzrost 140–195 cm             |     -/-                |         -/-               |
| A12 | Ławka obrotowa       | 18         | 25 min   | wzrost 140–195 cm             |    -/-                 | -/-                       |
| A13 | Kosmiczny wzmacniacz | 2 kapsuły po 4        | 20 min   | wzrost 140 - 195 cm|     -/-                |        -/-                |
| A14 | Dom potwora          | 3 wagoniki po 4       | 20 min   | wzrost od 130 cm              | wiek 4–12 z opiekunem|    -/-         |
| A15 | Samochodziki         | 10 samochodów po 2    | 15 min   | wzrost od 120 cm              |       -/-              |  -/-           |
| A16 | Przygoda w dżungli   | 5 łodzi po 9          | 35 min   | wzrost 120–140 cm z opiekunem, 140-195cm bez |             -/-            | -/- | 
| A17 | Restauracja          | 50         | 5–60 min | -/-   |      -/-                | -/-  |            

---

## Restauracja (A17)

* dostępna dla wszystkich
* można wejść **z parku i spoza parku**
* **nie jest wyjściem ani wejściem do parku**
* rachunek płatny:

  *  przy wyjściu z parku — jeśli klient był w parku (**~15%**) (VIP tez) 
  *  w kasie restauracji — jeśli klient był tylko w restauracji (**~10%**)

---

## Sygnały sterujące

Przy każdej atrakcji znajduje się pracownik parku:

* **sygnał1 (pracownik atrakcji)** — natychmiastowe opuszczenie atrakcji (przez wszystkich) 
* **sygnał2 (pracownik atrakcji)** — ponowne uruchomienie atrakcji   klienci mogą ponownie wrócić do danej atrakcji.
* **sygnał3 (kierownik)** —  wszyscy klienci i pracownicy natychmiast opuszczają park
rozrywki. Przy czym osoby będące w trakcie korzystania z atrakcji od **A6 do A12**  mogą opuścić park
z opóźnieniem **1min. ** (z uwagi na konieczność zatrzymania urządzenia). Park po wyjściu wszystkich
zostaje zamknięty.
---

## Zachowania klientów

* przestrzegają regulaminu
* mogą **dobrowolnie opuścić** atrakcje A1–A5, A13–A17 (**~10%**)
* **nie mogą rezygnować** w trakcie atrakcji A6–A12
* mogą korzystać z atrakcji wielokrotnie (**~5%**)
* VIP nie płacą dopłat
* system musi zapewnić brak zakleszczeń

---

##  Zachowania pracownika atrakcji

* wpuszcza klientów zgodnie z limitami
* uruchamia daną atrakcję; kontroluje czas i bezpieczeństwo klientów;
* uruchamia atrakcję nawet przy mniejszej liczbie chętnych
* zna PID-y obecnych uczestników
* pracownik wpuszcza w danej turze określoną liczbę osób do danej atrakcji – jeżeli liczba osób
jest mniejsza niż liczba miejsc (kolejka pusta) dana atrakcja jest uruchamiana z bieżącą
ilością klientów
* usuwa rezygnujących — bez uzupełniania miejsc
* Jeżeli klient zrezygnował z danej atrakcji (A1-A5, A13-A17) w trakcie jej trwania na jego
miejsce nie jest wpuszczana nowa osoba;

---

##  koncowe polecenie

Napisz procedury Kierownik, Kasa, Kasa restauracji, Pracownik obsługi i Klient symulujące działanie
parku rozrywki. Raport z przebiegu symulacji zapisać w pliku (plikach) tekstowym.

---

# Przykladowe testy

---

### Test 1 — Maksymalna pojemnośc parku

* N = 200
* 210 klientów próbuje wejść

**Oczekiwane:**

* 200 wchodzi
* 10 czeka
* brak zakleszczeń
* nigdy > 200 osób w parku

---

###   Test 2 — VIP

* 195 zwykłych klientów w parku
* 1 VIP pojawia się w kolejce
* 10 zwykłych klientów czeka

**Oczekiwane:**

* VIP wchodzi natychmiast jako 196
* omija kolejkę
* brak dopłaty
* limit czasu nie obowiązuje

---

###  Test 3 — Ograniczenia atrakcji

* dziecko 5 lat / 110 cm → A11  (za mały wzrost)
* dziecko 6 lat bez opiekuna → A6  (wymagany opiekun)
* dorosły 180 cm → A7
  **Oczekiwane:**
* dzieci nie zostaja przepuszczone, dorosly tak

---

###   Test 4 — Rezygnacja w trakcie atrakcji

* 20 osób w A1
* 3 rezygnują po 10 min

**Oczekiwane:**

* zostają usunięci z listy
* brak nowych wejść
* pozostałe 17 kończy przejazd

---

###   Test 5 — Przekroczenie limitu biletu

* klient ma bilet 2h
* przebywa 2h 45min
* nie jest VIP

**Oczekiwane:**

* naliczona dopłata = 200% wartości przekroczenia
* klient wypuszczony dopiero po rozliczeniu
* log z rozliczeniem

---
