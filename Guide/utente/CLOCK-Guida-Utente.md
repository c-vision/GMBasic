# CLOCK — Guida utente

Il dominio `CLOCK` espone **il tempo a passo fisso** del motore grafico al linguaggio BASIC.
Un programma non deve più misurare il tempo a mano (`TICKS`, `SLEEP 33`) per far avanzare una
simulazione: chiede al motore quanto dura un passo, quanto ne resta in coda e — se serve —
rallenta, ferma o riprende il tempo. È lo stesso orologio che alimenta `DOEVENTS`: una sola
sorgente di tempo per il programma e per il ciclo dei frame.

Disponibile su **tutte e quattro le gambe** (C99Native, IL, PCodeVM, VM/C), senza differenze di
parametri né di risultati.

---

## 1. Sintassi

Il dominio segue la regola del linguaggio: **il comando scrive, la funzione legge** (§1).

| Forma | Sintassi | Che cosa fa |
|---|---|---|
| Comando | `CLOCK PAUSE` | ferma la simulazione |
| Comando | `CLOCK RESUME` | riprende la simulazione |
| Comando | `CLOCK SCALE secondi` | moltiplica il tempo reale che entra |
| Comando | `CLOCK FPS frequenza` | imposta i passi di simulazione al secondo |
| Funzione | `CLOCK.DT()` | durata di un passo, in secondi |
| Funzione | `CLOCK.ALPHA()` | parte di passo rimasta in coda, 0..1 |
| Funzione | `CLOCK.FPS()` | passi nominali al secondo |
| Funzione | `CLOCK.SCALE()` | moltiplicatore del tempo attivo |

`IMPORT CLOCK` è **obbligatoria**: il dominio non è implicito. Senza l'IMPORT il programma non
compila, sia per il comando sia per le letture qualificate:

```
namespace non importato: CLOCK (aggiungi IMPORT CLOCK).
```

---

## 2. Comandi

### `CLOCK PAUSE`

Ferma la simulazione. L'accumulatore del tempo viene **azzerato**: il tempo che era in sospeso
non viene consumato quando si riprende. `CLOCK.ALPHA()` torna `0`. Il comando è idempotente:
ripetuto non cambia nulla.

### `CLOCK RESUME`

Riprende dal passo successivo, con la coda vuota e con la scala che era attiva. Non si porta
dietro il tempo passato in pausa.

### `CLOCK SCALE secondi`

Moltiplicatore del **tempo reale in ingresso**, non del passo:

- `1` — tempo reale (default);
- `0.5` — slow motion: metà del tempo reale entra nella simulazione;
- `2` — doppia velocità;
- `0` — la simulazione non avanza (la coda non cresce), senza toccare l'accumulatore come fa
  `PAUSE`.

Valori ammessi: **0..16**. Un valore fuori intervallo viene **ignorato**: lo stato resta quello
precedente, senza errore a runtime. Il passo (`DT`) non cambia: cambia quanti passi entrano per
unità di tempo reale.

### `CLOCK FPS frequenza`

Frequenza nominale dei passi, in passi al secondo (fps del passo fisso). Valori ammessi:
**1..1000**; fuori intervallo si torna a 60. Cambiare frequenza cambia `DT` (`1/FPS`) e quindi il
passo della simulazione, **non** la quantità di tempo simulato: a 120 Hz lo stesso secondo vale
120 passi più corti.

> `FPS` è l'unica parola del dominio che non è una keyword del linguaggio: i programmi esistenti
> usano `fps` come nome di variabile (`DIM fps AS INTEGER`). Il parser riconosce la parola nella
> posizione di verbo di `CLOCK` e basta, quindi quelle variabili continuano a funzionare.

---

## 3. Funzioni

### `CLOCK.DT() → DOUBLE`

Durata di **un passo** di simulazione, in secondi: `1 / CLOCK.FPS()`. È il numero da moltiplicare
per una velocità per ottenere lo spostamento di un passo. A 60 Hz vale ≈ `0.0166667`.

### `CLOCK.ALPHA() → DOUBLE`

Parte di passo rimasta nella coda, in `[0, 1)`. Serve a **interpolare** il disegno fra due passi
senza cambiare la simulazione: `0` = confine esatto di passo, `0.5` = metà del passo successivo
già maturata. È `0` in pausa e con la coda vuota.

### `CLOCK.FPS() → DOUBLE`

Passi nominali al secondo, cioè `1 / CLOCK.DT()`. È la lettura che corrisponde al comando
`CLOCK FPS` (§3.1 della direttiva: coppia scrittura/lettura dello stesso stato).

### `CLOCK.SCALE() → DOUBLE`

Moltiplicatore attivo del tempo reale. Corrisponde al comando `CLOCK SCALE`.

---

## 4. Semantica del tempo

Il motore non avanza "quanto è passato": avanza **in multipli interi di un passo**.

1. Il tempo reale trascorso (scalato) si somma in un accumulatore.
2. I passi interi che ci stanno dentro vengono eseguiti; il resto diventa `ALPHA`.
3. Un singolo frame non accoda più di **250 ms**: un blocco lungo viene tagliato, non smaltito
   tutto insieme (nessun "teletrasporto" della simulazione).
4. Al massimo **8 passi per frame**: il ritardo che resta viene scartato, non si trascina nei
   frame successivi (nessuna spirale di recupero).

Conseguenza pratica: la stessa sequenza di passi produce gli **stessi numeri** su tutte le gambe
e su macchine diverse, anche se il disegno è più lento o più veloce.

---

## 5. Esempi

### 5.1 Un ciclo a passo fisso

```basic
IMPORT CLOCK

SUB Main()
  CLOCK FPS 60
  PRINT "dt in ms: "; INT(CLOCK.DT() * 1000 + 0.5)     ' 17

  DIM i AS LONG
  DIM x AS DOUBLE
  FOR i = 1 TO 60
    x = x + 2.5 * CLOCK.DT()          ' 2,5 unità al secondo
  NEXT
  PRINT "x dopo 60 passi: "; INT(x * 100 + 0.5)        ' 250 (centesimi)
END SUB
```

### 5.2 Rallentare, fermare, riprendere

```basic
IMPORT CLOCK

SUB Main()
  CLOCK SCALE 0.5
  PRINT "scala: "; INT(CLOCK.SCALE() * 100 + 0.5)      ' 50

  CLOCK PAUSE
  PRINT "alpha in pausa: "; INT(CLOCK.ALPHA() + 0.5)   ' 0
  CLOCK RESUME

  CLOCK SCALE 1
  PRINT "scala: "; INT(CLOCK.SCALE() * 100 + 0.5)      ' 100
END SUB
```

### 5.3 Interpolazione del disegno

```basic
IMPORT CLOCK
IMPORT GRAPHICS

' alpha dice quanto siamo avanti NEL passo successivo: si disegna
' fra la posizione del passo corrente e quella del passo dopo.
DIM a AS DOUBLE
DOEVENTS
a = CLOCK.ALPHA()
```

L'esempio completo, con tutti e quattro i comandi e tutte e quattro le letture, è
`shared/samples/clock/ClockDemo.bas`.

---

## 6. Disponibilità per backend

| Backend | Comandi | Letture | Note |
|---|---|---|---|
| C99Native | ✅ | ✅ | chiamata diretta a `gmb_clock_frame_*` |
| IL | ✅ | ✅ | P/Invoke verso `libgmb_clock` |
| PCodeVM | ✅ | ✅ | P/Invoke verso `libgmb_clock` |
| VM/C | ✅ | ✅ | syscall 476..479 e builtin 3908..3911 |

Lo stato dell'orologio è **uno solo per processo** e vive nel runtime C
(`native/gmb_runtime_c/src/gmb_clock.c`); nessun backend ne tiene una copia.

---

## 7. Errori

| Situazione | Esito |
|---|---|
| `CLOCK.DT()` senza `IMPORT CLOCK` | errore di compilazione: `namespace non importato: CLOCK (aggiungi IMPORT CLOCK).` |
| `CLOCK SCALE` senza argomento | errore di compilazione (espressione attesa) |
| valore di scala fuori 0..16 | ignorato: lo stato resta quello precedente |
| frequenza fuori 1..1000 | torna al default (60) |
| verbo sconosciuto (`CLOCK AVANTI`) | errore di compilazione: `Sottocomando 'CLOCK AVANTI' non riconosciuto: usa PAUSE, RESUME, SCALE o FPS.` |

Il dominio non è polimorfico: è **deterministico e headless-safe**, identico in console, in una
finestra grafica e in un programma senza schermo.

---

## 8. MiniTest e riferimenti

- `shared/minitests/clock/clock_letture.bas` — le quattro letture allo stato iniziale.
- `shared/minitests/clock/clock_comandi.bas` — i quattro comandi verificati con le letture.
- `shared/samples/clock/ClockDemo.bas` — programma completo.
- Verifica di parità: `tools/sh/parity4 shared/minitests/clock/clock_comandi.bas` (le quattro
  gambe devono concordare).
- Contratto del motore e numeri verificati: `native/gmb_runtime_c/include/gmb_clock.h` e
  `native/gmb_runtime_c/tests/gmb_clock_selfcheck.c`.
- Capitolo del manuale sul tempo simulato: `docs/manual/capitoli/IV.21b-mondo-tempo.md`
  (dominio `WORLD`, che usa lo stesso passo fisso).
