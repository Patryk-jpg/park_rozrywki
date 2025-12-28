
#include "Kasa.h"

#include <map>

#include "park_wspolne.h"

park_wspolne* g_park = nullptr;

int update_licznik_klientow(klient_message& request) {
    bool wpuszczony = true;
    for (int i = 0; i < request.ilosc_osob; i++) {
        if (wait_semaphore(g_park->licznik_klientow, 0, IPC_NOWAIT) == -1) {
            wpuszczony = false;
            break;
        }
    }

    if (!wpuszczony || !g_park->park_otwarty) {
        // cofamy już zajęte miejsca
        for (int i = 0; i < request.ilosc_osob; i++) {
            signal_semaphore(g_park->licznik_klientow, 0);
        }
        return -1;
    } else {
       return  0;
    }
}

int main(int argc, char *argv[]) {
    float zarobki = 0.0f;
    int transakcje = 0;
    printf("KASA CZYNNA  PID: %d", getpid() );
    fflush(stdout);
    g_park = attach_to_shared_block();
    int kasaId = create_message_queue(SEED_FILENAME_QUEUE, QUEUE_SEED);
    std::map<pid_t, serwer_message> clients_pids;
    initialize_semaphore(g_park->licznik_klientow, 0, MAX_KLIENTOW_W_PARKU);
    while (g_park->park_otwarty || MAX_KLIENTOW_W_PARKU - read_semaphore(g_park->licznik_klientow, 0) != 0) {

        klient_message request{};
        serwer_message reply{};
        payment_message payment_request{};
        size_t wychodzacy = msgrcv(kasaId, &payment_request, sizeof(payment_request) - sizeof(long), 101, IPC_NOWAIT);
        if (wychodzacy != -1) {
            float total = 0.0f;
            float koszt_r = 0.0f;
            float doplata = 0.0f;
            float kwPodstawowa = clients_pids[payment_request.pid].cena;


            koszt_r = oblicz_koszt_restauracji(payment_request.czasWRestauracji) *  clients_pids[payment_request.pid].ilosc_osob;
            bool czyVIP = (clients_pids[payment_request.pid].typ_biletu == 4);

            if (!czyVIP) {
                int czasNadmiarowy = g_park->czas_w_symulacji.toMinutes() - clients_pids[payment_request.pid].end_biletu.toMinutes();
                if (czasNadmiarowy > 0) {
                    float cenaZaNadmiarowaMinute = ((float)kwPodstawowa / (bilety[clients_pids[payment_request.pid].typ_biletu].czasTrwania * 60.0f)) * 2.0f;
                    doplata =  cenaZaNadmiarowaMinute * czasNadmiarowy;
                }
            }
            if (payment_request.wiekDziecka > 2) {
                kwPodstawowa  = kwPodstawowa * clients_pids[payment_request.pid].ilosc_osob;
                doplata = doplata * clients_pids[payment_request.pid].ilosc_osob;
            }
            total = kwPodstawowa + koszt_r + doplata;
            printf("PARAGON - Klient %d\n", payment_request.pid);
            printf("========================================\n");
            printf("Koszt biletu: %.2f zł Osób: %d \n", kwPodstawowa, clients_pids[payment_request.pid].ilosc_osob);
            printf("Koszt restauracji:   %.2f zł (%d min)\n",
            koszt_r, payment_request.czasWRestauracji);
            int czasNadmiarowy = g_park->czas_w_symulacji.toMinutes() - clients_pids[payment_request.pid].end_biletu.toMinutes();
            printf("Dopłata za czas:     %.2f zł (%d min)\n", doplata, czasNadmiarowy);
            printf("----------------------------------------\n");
            printf("SUMA:                %.2f zł\n", total);
            printf("========================================\n");
            fflush(stdout);
            zarobki += total;
            transakcje++;
            payment_request.mtype = payment_request.pid;
            payment_request.suma = total;
            msgsnd(kasaId, &payment_request, sizeof(payment_request) - sizeof(long), 0);

            clients_pids.erase(payment_request.pid);

        }

        size_t n = msgrcv(kasaId, &request, sizeof(request) - sizeof(long), -10, IPC_NOWAIT);
        if (n == -1) {
            usleep(1000);
            continue;
        }
        reply.mtype = request.pid_klienta;
        reply.cena = request.ilosc_biletow * bilety[request.typ_biletu].cena;
        reply.start_biletu = g_park->czas_w_symulacji;
        reply.typ_biletu = request.typ_biletu;
        reply.end_biletu = SimTime(g_park->czas_w_symulacji.hour, g_park->czas_w_symulacji.minute) + SimTime(bilety[request.typ_biletu].czasTrwania,0);
        reply.ilosc_osob = request.ilosc_osob;

        clients_pids[request.pid_klienta] = reply;
        reply.status = update_licznik_klientow(request);
        if (!g_park->park_otwarty) {
            reply.status = -1;
        }
        msgsnd(kasaId, &reply, sizeof(reply) - sizeof(long), 0);

    }
    msgctl(kasaId, IPC_RMID, NULL);
    detach_from_shared_block(g_park);

}