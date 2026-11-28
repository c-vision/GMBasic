/* gmb_audio_selfcheck.c — il motore audio a 8 voci, in OFFLINE.
 *
 * Perché offline. La fase 1 dell'ADR toglie il driver audio finto imposto per ambiente: il
 * selfcheck non ha bisogno di mentire all'ambiente, chiede `gmb_audio_set_mode(0)` e
 * rende i campioni in un buffer esplicito. Stesso motore del device (gmb_synth.c), quindi
 * gli oracoli valgono anche quando il device c'è.
 *
 * Cosa fissa (M-AUD-*): ampiezza di una voce, indipendenza dal blocco, pan lineare,
 * mappa P6 (slot-1) mod 8, nessuna rivendicazione della voce da parte del live update,
 * master, contatore clip, clock, eventi MML, filtro che cambia davvero il suono,
 * round trip di modalità (set_mode(0) chiudendo il mixer non lascia "device" stantio).
 *
 * Eseguito da: ctest -R gmb_audio_selfcheck
 */
#include "gmb_audio.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int g_fails = 0;

static void check(int cond, const char* what, int line)
{
    if (!cond) { printf("FAIL riga %d: %s\n", line, what); g_fails++; }
}

#define CHECK(c, w) check((c) != 0, (w), __LINE__)

static int16_t g_buf[8192 * 2];

static void silence(void)
{
    gmb_audio_control(0, -1);       /* STOP su tutte le voci */
    gmb_audio_render(g_buf, 64);    /* pubblica lo stato vuoto */
}

static double voice(int v, int what)
{
    double x = -1.0;
    if (!gmb_audio_voice_read(v, what, &x)) return -1.0;
    return x;
}

static double status(int what)
{
    double x = -1.0;
    gmb_audio_status(what, &x);
    return x;
}

int main(void)
{
    /* ── 0. Modalità: offline, senza toccare l'ambiente ─────────────────────────────── */
    printf("audio fase1: modalità offline, nessuna variabile d'ambiente... ");
    gmb_audio_set_mode(0);
    CHECK(gmb_audio_mode() == 0, "gmb_audio_set_mode(0) dà modalità offline");
    CHECK(status(4) == 0.0, "AUDIO.STATUS(4) = 0 in offline");
    printf("%s\n", g_fails ? "KO" : "OK");

    /* ── 0bis. Round trip: set_mode(0) → set_mode(1) non deve mentire sul device ──────
     * Il mixer chiuso resta chiuso finché non lo si riapre: se la modalità dicesse
     * "device" con hook staccato, il clock non avanzerebbe (silenzio). */
    printf("audio fase1: set_mode(0) → set_mode(1) riapre davvero (o resta offline)... ");
    {
        int before = g_fails;
        gmb_audio_set_mode(1);
        int with_device = gmb_audio_mode() == 1;
        gmb_audio_set_mode(0);
        CHECK(gmb_audio_mode() == 0, "dopo set_mode(0) la modalità è offline");
        CHECK(gmb_audio_render(g_buf, 64) == 64, "col mixer chiuso il render è esplicito");
        gmb_audio_set_mode(1);
        CHECK(gmb_audio_mode() == with_device,
              "secondo set_mode(1): stessa disponibilità del primo (nessuno stato stantio)");
        if (gmb_audio_mode() == 1)
        {
            double c0 = status(0), c1 = c0;
            clock_t t0 = clock();
            while (c1 <= c0 && (clock() - t0) < CLOCKS_PER_SEC / 4)
                c1 = status(0);
            CHECK(c1 > c0, "in device mode il hook avanza il clock (mixer aperto davvero)");
        }
        else
        {
            CHECK(gmb_audio_render(g_buf, 64) == 64, "senza device il render resta esplicito");
        }
        gmb_audio_set_mode(0);
        printf("%s%s\n", g_fails > before ? "KO" : "OK",
               with_device ? " (device)" : " (offline: nessun device su questa macchina)");
    }

    /* ── 1. Ampiezza di UNA voce: quadra a volume pieno = 16000/32767 ───────────────── */
    printf("audio fase1: ampiezza voce (quadra, vol 100) = 0,4883... ");
    {
        int before = g_fails;
        silence();
        gmb_audio_sound_voice(0, 440.0, 0.5, 100, 1, 0.0, 0.0, 1.0, 0.0, 0.25);
        CHECK(gmb_audio_render(g_buf, 512) == 512, "il render offline ritorna i frame chiesti");
        double peak = voice(0, 0);
        CHECK(fabs(peak - 16000.0 / 32767.0) < 1e-3, "picco voce = 16000/32767");
        CHECK(fabs(voice(0, 1) - 16000.0 / 32767.0) < 1e-3, "RMS quadra = 16000/32767");
        CHECK(voice(0, 2) == 1.0, "la voce risulta occupata");
        CHECK(voice(0, 3) == 440.0, "AUDIO.VOICE(0,3) = frequenza");
        CHECK(voice(0, 4) > 0.0, "AUDIO.VOICE(0,4) = campioni rimanenti");
        if (g_fails > before) printf("  (peak misurato %.5f)  ", peak);
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 2. Indipendenza dal blocco: 1×1000 == 10×100 ───────────────────────────────── */
    printf("audio fase1: 1×1000 campioni == 10×100 (stato per voce)... ");
    {
        int before = g_fails;
        static int16_t a[1000 * 2], b[1000 * 2];
        silence();
        gmb_audio_sound_voice(0, 440.0, 0.5, 80, 0, 0.10, 0.0, 1.0, 0.20, 0.25);
        gmb_audio_render(a, 1000);
        silence();
        gmb_audio_sound_voice(0, 440.0, 0.5, 80, 0, 0.10, 0.0, 1.0, 0.20, 0.25);
        for (int i = 0; i < 10; i++) gmb_audio_render(b + i * 200, 100);
        CHECK(memcmp(a, b, sizeof a) == 0, "blocchi diversi, stesso segnale");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 3. Pan lineare: pan -1 azzera il canale destro ─────────────────────────────── */
    printf("audio fase1: pan -1 lascia solo il canale sinistro... ");
    {
        int before = g_fails;
        silence();
        gmb_audio_sound_panned_slot(1, 300.0, 0.2, 0.5, -1.0);
        gmb_audio_render(g_buf, 256);          /* le letture sono lo snapshot dell'ultimo blocco */
        CHECK(voice(0, 2) == 1.0, "lo slot 1 suona sulla voce 0");
        int right_zero = 1, left_sound = 0;
        for (int i = 0; i < 256; i++)
        {
            if (g_buf[i * 2 + 1] != 0) right_zero = 0;
            if (g_buf[i * 2] != 0) left_sound = 1;
        }
        CHECK(right_zero, "canale destro a zero con pan -1");
        CHECK(left_sound, "canale sinistro non muto");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 4. P6: voce = (slot-1) mod 8 ───────────────────────────────────────────────── */
    printf("audio fase1: P6 mappa lo slot (slot-1) mod 8... ");
    {
        int before = g_fails;
        silence();
        gmb_audio_sound_panned_slot(9, 300.0, 0.2, 0.5, 0.0);
        gmb_audio_render(g_buf, 64);
        CHECK(voice(0, 2) == 1.0, "slot 9 → voce 0");
        silence();
        gmb_audio_sound_panned_slot(3, 300.0, 0.2, 0.5, 0.0);
        gmb_audio_render(g_buf, 64);
        CHECK(voice(2, 2) == 1.0, "slot 3 → voce 2");
        CHECK(voice(0, 2) == 0.0, "slot 3 non tocca la voce 0");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 5. D-9: il live update non reclama una voce libera ─────────────────────────── */
    printf("audio fase1: update_panned non rivendica voci libere... ");
    {
        int before = g_fails;
        silence();
        gmb_audio_update_panned(3, 1.0, 1.0);
        gmb_audio_render(g_buf, 64);
        CHECK(voice(2, 2) == 0.0, "voce 2 ancora libera dopo update_panned(3)");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 6. Master: volume 0 azzera il mix, non il picco della voce ─────────────────── */
    printf("audio fase1: master 0 azzera il mix... ");
    {
        int before = g_fails;
        silence();
        gmb_audio_sound_voice(0, 440.0, 0.5, 100, 1, 0.0, 0.0, 1.0, 0.0, 0.25);
        gmb_audio_set_volume(0, -1);
        gmb_audio_render(g_buf, 128);
        int all_zero = 1;
        for (int i = 0; i < 256; i++) if (g_buf[i] != 0) all_zero = 0;
        CHECK(all_zero, "con master 0 il mix è muto");
        CHECK(voice(0, 0) > 0.4, "il picco della voce resta pre-master");
        gmb_audio_set_volume(100, -1);
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 7. Clip: tre voci quadre a piena ampiezza superano ±1,0 ────────────────────── */
    printf("audio fase1: tre voci sommano oltre il fondo scala... ");
    {
        int before = g_fails;
        silence();
        double clips0 = status(2);
        for (int v = 0; v < 3; v++)
            gmb_audio_sound_voice(v, 100.0, 0.5, 100, 1, 0.0, 0.0, 1.0, 0.0, 0.25);
        gmb_audio_render(g_buf, 256);
        CHECK(status(2) > clips0, "il contatore clip cresce");
        CHECK(status(1) >= 0.999, "il picco del mix satura a 1,0");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 8. Clock: avanza solo di ciò che si rende ──────────────────────────────────── */
    printf("audio fase1: il clock conta i campioni resi... ");
    {
        int before = g_fails;
        silence();
        double c0 = status(0);
        gmb_audio_render(g_buf, 128);
        CHECK(status(0) == c0 + 128.0, "STATUS(0) avanza di 128");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 9. Filtro: cambia davvero il segnale reso ──────────────────────────────────── */
    printf("audio fase1: il filtro cambia il segnale della voce... ");
    {
        int before = g_fails;
        static int16_t a[512 * 2], b[512 * 2];
        silence();
        gmb_audio_set_filter(4, 0, 800.0, 0.0);
        gmb_audio_sound_voice(4, 200.0, 0.5, 100, 1, 0.0, 0.0, 1.0, 0.0, 0.25);
        gmb_audio_render(a, 512);
        silence();
        gmb_audio_set_filter(4, 1, 300.0, 0.0);
        gmb_audio_sound_voice(4, 200.0, 0.5, 100, 1, 0.0, 0.0, 1.0, 0.0, 0.25);
        gmb_audio_render(b, 512);
        CHECK(memcmp(a, b, sizeof a) != 0, "filtro passa-basso: campioni diversi");
        gmb_audio_set_filter(4, 0, 800.0, 0.0);
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 10. MML: eventi in campioni, leggibili da AUDIO.EVENT ─────────────────────── */
    printf("audio fase1: PLAY produce eventi con start in campioni... ");
    {
        int before = g_fails;
        silence();
        double c0 = status(0);
        gmb_audio_play_mml("T120 L4 C D");
        gmb_audio_render(g_buf, 64);            /* pubblica lo snapshot (letture headless) */
        CHECK(status(3) >= 2.0, "STATUS(3) conta gli eventi");
        double start = -1, freq = -1, dur = -1, kind = -1;
        CHECK(gmb_audio_event(0, 0, &start) && start == c0, "primo evento allo start corrente");
        CHECK(gmb_audio_event(0, 1, &freq) && fabs(freq - 261.63) < 0.5, "primo evento = Do4");
        CHECK(gmb_audio_event(0, 2, &dur) && dur > 0, "durata in campioni");
        CHECK(gmb_audio_event(1, 0, &start) && start > c0, "secondo evento dopo il primo");
        CHECK(gmb_audio_event(1, 4, &kind) && kind == 0.0, "tipo 0 = nota");
        printf("%s\n", g_fails > before ? "KO" : "OK");
    }

    /* ── 11. WAV = chunk del mixer: testabile solo col device (fase 1) ──────────────── */
    printf("audio fase1: WAV (chunk del mixer)... ");
    if (gmb_audio_mode() == 1)
    {
        int h = gmb_audio_load_sound("non-esiste.wav");
        CHECK(h == 0, "un file inesistente non carica");
        printf("OK (device: caricamento rifiutato correttamente)\n");
    }
    else
    {
        printf("PARCHEGGIATO (offline: nessun chunk, fase 2 porta il decoder)\n");
    }

    if (g_fails) { printf("FALLITI: %d\n", g_fails); return 1; }
    printf("TUTTO OK\n");
    return 0;
}
