# PARK ROZRYWKI - RAPORT (temat: 9)

### Autor: Patryk Janik 
#### nr albumu : 155183



Wymagania projektu i szczegółowy opis są dostępne w pliku : https://github.com/Patryk-jpg/park_rozrywki/blob/zmieniony_czas/README.md

--- 
### Pliki i ich działanie w projekcie

Projekt realizowano używając WSL podłączonego do Cliona, budowa i kompilacja plików za pomocą CMakeLists.txt

* **Park.cpp** - program główny, (proces "kierownika"), jest odpowiedzialny za tworzenie wszystkich procesów w projekcie, oraz struktur komunikacyjnych. Podczas symulacji ciągle tworzy klientów oraz zarządza czasem. 

* **Kasa.cpp** - Proces odpowiedzialny za wpusczanie i wypuszczanie klientów z parku oraz wszelkich niuansów z tym związanymi (paragony, zapis pidów itp...)

* **kasaRestauracji.cpp** - Proces obsługujący kasę w restauracji, jest ona używana gdy ktoś wejdzie do atrakcji nr 16 (RESTAURACJA) poza parkiem przez co nie może zapłacić w Kasie głownej

* **park_wspolne.cpp** - plik shared dla każdego z procesów, zawiera pomocnicze funkcje do komunikacji i innych rzeczy wraz z plikiem nagłówkowym **park_wspolne.h**

* **pracownik.cpp** - proces pracownika który ma za zadanie aktywnie śledzić i zakańczać aktywne jazdy na swojej atrakcji, wpuszczać i wypuszczać z niej klientów , w różnych okolicznościach (rezygnacja, zakończenie jazdy bądź awaryjne zakończenie atrakcji)


---
###  Z czym były problemy

Błędem który zajął mi najdłużej (pare dni) było określenie kolejności zamykania i zapewnienie że zostanie ona utrzymana przy różnych sygnałach (klienci -> pracownicy -> kasy -> park)

Warte uwagi problemy to naprawa busy-waitingów co w większości udało mi się zamienić na bardziej rozsądne rozwiązanie

### zauważone ( i naprawione problemy z testami)

Test sprawdzający czy klienci prawidłowo wchodzą na atrakcje na początku miał problemy okazało się że dużo wartości było sprawdzanych niepoprawnie

Kasa naliczała przez przypadek 4x dopłate czasami dla pojedyńczych klientów


### Komunikacja między-procesowa


1. Pamięć dzielona (park_wspolne) - przechowuje globalny stan parku:
    * Aktualny czas symulacji
    * Licznik klientów w parku
    * Status otwarcia parku
    * Klucze do kolejek komunikatów

2. Kolejki komunikatów (System V Message Queues):

    * Główna kolejka kasy - żądania wejścia/wyjścia
    * Kolejka odpowiedzi kasy - potwierdzenia dla klientów
    * Kolejka kasy restauracji + odpowiedzi
    * 17 kolejek dla atrakcji (żądania) + 17 kolejek odpowiedzi
    * Kolejka loggera - zapis zdarzeń 

3. Semafory (System V):

    * Semafor park_sem - synchronizacja dostępu do pamięci dzielonej

### Model synchronizacji

Dostęp do pamięci dzielonej chroniony semaforem binarnym natomiast 
Kolejki komunikatów zapewniają bezpieczną wymianę danych między procesami
Każdy klient ma unikalny identyfikator (PID) używany jako mtype w kolejkach

### Czas

Czas jest przechowywany w strukturze SimTime i jest aktualizowany przez **park.cpp** cyklicznie , początkowo polegało to na zwiększaniu o 1min w pętli z usleepem ale teraz za pomocą sprawdzania ile czasu upłynęło od włączenia symulacji (odejmując od time(NULL) czas startu symulacji i zapisując 1 sekunde prawdziwego czasu jako 1 minutę symulacji)

### Sygnały

Każdy proces w pewnym stopniu obsługuje sygnały 

* klienci, kasy i pracownicy ignorują SIGINTA, ich zamknięciem zarządza park

* pracownik posiada 2 sygnały SIGUSR1, SIGUSR2 do zatrzymywania i wznawiania atrkacji

* park obsługuje SIGINT jako sygnał3 z wymagań (wcześniejsze zakończenie programu aka ewakuacja) dodatkowo obsługuje SIGCHLD żeby czyścić na bieżąco procesy zombie

## Linki do kluczowych fragomentów

1.  Tworzenie i obsługa plików
* Tworzenie pliku logu: https://github.com/Patryk-jpg/park_rozrywki/blob/3e5322f8adc1e6dac5b4b1ccdaffa95e20f66a24/park.cpp#L22-L30
* Zapis do pliku (write):
https://github.com/Patryk-jpg/park_rozrywki/blob/3e5322f8adc1e6dac5b4b1ccdaffa95e20f66a24/park.cpp#L31-L62


2. Tworzenie procesów
* fork() - tworzenie klientów:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L402-L414
* execvp() - uruchamianie pracowników:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L141-L162
* execvp() - uruchamianie kasy:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L112-L141
* wait()/waitpid() - zbieranie procesów zombie:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L172-L176
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L221-L232

3. Tworzenie i obsługa wątków
* pthread_create() - tworzenie wątku loggera:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L296-L299
* pthread_join() - oczekiwanie na zakończenie wątku:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L455-L458

(wątek loggera nie wymaga mutexów ponieważ komunikuje się przez kolejkę komunikatów, która jest thread-safe.)

4.. Obsługa sygnałów
* sigaction() - SIGINT (ewakuacja):
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L335-L342
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L256-L261
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L166-L169

* sigaction() - SIGUSR1  i SIGUSR2 (zatrzymanie i wznowienie atrakcji):
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L34-L45

* signal() - ignorowanie SIGINT w podprocesach:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L96
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Kasa.cpp#L219
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/kasaRestauracji.cpp#L17

5. Synchronizacja procesów

* semget() - tworzenie semafora:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L49-L56
* semctl() - inicjalizacja wartości semafora:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L62-L69
* semop() - wait (P operation):
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L71-L86
* semop() - signal (V operation):
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L88-L100

6. Segmenty pamięci dzielonej

* shmget() - tworzenie/dostęp do pamięci:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L12-L19
* shmat() - attach do procesu:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L21-L35
* shmdt() - detach od procesu:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L37-L39
* shmctl() - usuwanie segmentu:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L41-L47
* Struktura pamięci dzielonej:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.h#L141-L150

7. Kolejki komunikatów

* msgget() - tworzenie kolejki:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L132-L137

* msgget() - dołączanie do istniejącej kolejki:
https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L139-L151

* msgsnd() - wysyłanie wiadomości:
    * (odpowiedź na chęć wejścia do parku) https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Kasa.cpp#L133 
    * (odpowiedź na chęć wyjścia z parku) https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Kasa.cpp#L194
    * (awaryjne czyszczenie wszystkich aktywnych jazd na atrakcji) https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L68
    ... i czyszczenie wiadomości z kolejce tych oczekujących na jazde https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L83
    * podobnie podczas zatrzymania atrakcji 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L176
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L191
    * odpowiedź na rezygnacje z atrakcji
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L222
    * odpowiedź na prośbe o wejście na atrakcje 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L274-L280
    * prośba o wejście do parku
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L135-L145
    * prośba o wejście na atrakcje 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L224-L228
    * rezygnacja z atrakcji, prośba o wcześniejsze wyjście
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L291-L296
    * poinformowanie kasy o chęci wyjścia z parku
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L409-L414
    * poinformowanie kasyRestauracji o chęci zapłaty (tylko gdy wejście z zewnątrz)
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L441-L444
    * wysyłanie wiadomości które zakończą kasy
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L82-L97
    * pomocnicze funkcje do wysyłania logów i kończenia loggera
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park_wspolne.cpp#L170-L218
    

* msgrcv() - odbieranie wiadomości:

    * odbieranie informacji czy udało się wejść do parku https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L153-L158
    * odbieranie czy udało się wejść na atrakcje 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L232-L236
    * przygotowanie do rezygnacji (zanim klient zrezygnuje to czeka X minut, za ten czas musi obserwować czy pracownik sam z siebie awaryjnie nie zakończył jazdy)
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L273-L274
    * potwierdzenie przyjęcia rezygnacji 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L298-L302
    * otrzymanie informacji o normalnym zakończeniu jazdy 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L308-L312
    * potwierdzenie przyjęcia zapłaty, wyjście z parku
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L417-L418
    * potwierdzenie zapłaty w kasie restauracji (aka wzięcie paragonu)
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/klient.cpp#L448-L453
    ---
    * odbiór wszystkich możliwych typow wiadomości do kasy po czym na bazie mtype zdecydowanie której części uniona użyć
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Kasa.cpp#L248-L273
    * przyjęcie wiadomości o chęci zapłaty w restauracji 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/kasaRestauracji.cpp#L45-L47
    ---
    * masowe zczytywanie wiadomości do czyszczenia i ewakuacji z atrakcji
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L79-L86
    i podobnie przy zatrzymaniu 
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L187-L192
    * odbiór wiadomości o chęci rezygnacji z jazdy na atrakcji
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L201-L223
    * wpuszczanie nowych klientów  na atrakcję
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/Pracownik.cpp#L262-L264
    ---
    * odbieranie logów do wpisania do pliku
    https://github.com/Patryk-jpg/park_rozrywki/blob/0a03d4e5c3942a24157beb930a2840a69c23824e/park.cpp#L40-L42


#### Totalna ilość struktur komunikacji to 1 semafor, 38 kolejek komunikatów, 1 pamięć dzielona

--- 
### Wnioski

Projekt udało się w pełni zrealizować zgodnie z wymaganiami tematu 9. Największe wyzwanie stanowiła koordynacja zamykania wielu procesów i zapewnienie braku deadlocków. System jest stabilny i prawidłowo obsługuje wszystkie scenariusze (normalne działanie, ewakuacja, zatrzymanie atrakcji).


### Dodatkowe usprawnienia

- **Optymalizacja czasu:** Zamiana `usleep()` w pętli głównej na rzeczywisty pomiar czasu z `time(NULL)` - 1 sekunda rzeczywista = 1 minuta symulacji
- **System wagonów:** Zamiast prostego licznika - rzeczywiste śledzenie kto w którym wagonie/łodzi/kapsule
- **Obsługa grup:** Klienci z dziećmi traktowani jako grupa (walidacja dla obu osób)


Poprzez ewakuację mam na myśli wcześniejsze zakończenie programu przez ctrl+c (wysłanie SIGINT)


### Testy

Testy zostały przeprowadzone z lekkimi modyfikacjami w stosunku do oryginału jako że zmiana / specjalna generacja X procesów często nie była potrzebna np. dla testu 1, zamiast generować 210 idealnie klientów wystarczy zobaczyć jak program zachowuje się normalnie

1. *Test 1*
* N = 100
 Co minute symulacji generujemy klienta który chce wejść
* Oczekiwane:
Park zaczyna odrzucać klientów przy osiągnięciu limitu
* nigdy > 100 osób w parku

* ![screen1](/screens/screen1.png)

**Założenia spełnione **

2. *Test 2* 

* 200 zwykłych klientów w parku <br>
1 VIP pojawia się w kolejce
* Oczekiwane:
VIP  jest odczytany priorytetowo 
omija kolejkę

* W ramach tego testu generuje 200 klientów, po czym jednego vipa Zapisujemy ich czasy wejścia i wyjścia z kolejek, oraz ile było aktualnie wiadomości w kolejce jak też i to czy vip został obsłużony pierwsze:|


* ![screen2](/screens/screen2.png)
* ![screen3](/screens/screen3.png)
* ![screen4](/screens/screen4.png)

* W tym momencie kończą się logi z wchodzenia, 
Jak widać na pierwszym screenie kasa zdążyła już odebrać jedną wiadomość od zwykłego klienta, więc po jego obsłużeniu następnym powinien być VIP który wysłał mtype = 10  (normalny klient wysyła 15, a kolejka jest priorytetowa)

* ![screen4](/screens/screen4.png)

3. Test 3
Prawidłowe ograniczenia atrakcji
* Założenia
Przy normalnym zachowaniu atrakcji, klienci powinni być odrzucani według zasad 

* Przeprowadzono testy dla kazdej z atrakcji, poprzez sprawdzanie logów w czasie egzekucji i ich pokrycia się z logiką odrzucania 

* ![screen5](/screens/screen5.png)

4. Test 4 rezygnacja bez dopełniania

* W normalnej symulacji pracownik powinien wyrzucać klientów z atrakcji bez uzupelniania miejsc na danym wagoniku

* ![screen6](/screens/screen6.png)
* Jak widac założenie jest spełnione, jeśli ktoś zrezygnuje nie są oni dalej w wagoniku a pracownik na końcu poprawnie wypuscza pozostałych klientów

* Test 5 przekroczenie limitu czasu 

* W normalnej symulacji należy sprawdzić kwotę zapłaconą przez klienta

* ![screen7](/screens/screen7.png)

Biorąc na przykład klienta 132061 bilet 65zł (1 dniowy)  z nadmiarowymi 30 minutami

Bilet 1 dniowy trwa 24 godziny = 1440 minut

W takim razie cena za minutę wynosi 65/1440 = 0.045,
W związku z tym za każdą nadmiarową minute klient powinien zapłacić 0.045* 2 (200%) = 0.09
30* 0.09 = 2.7 zł czyli tyle ile dopłacił

Akurat przy bilecie dziennym czas nadmiarowy może być tylko jeśli klient zwlekał i wyszedł idealnie na zakonczenie przez co czekał w kolejce aby zapłacić


Z innych obliczeń 
Bilet6h - 85zł 

85/360 = 0.23 (doplata 0.46 za minute)
 
Itp...

W przypadków problemów z wyświetleniem grafiki tutaj znajduje się wersja google docs testów

https://docs.google.com/document/d/1qc-JoQw2PQhwKuk2D18NBXy3lQHGcChE1c0F31hjelc/edit?usp=sharing



