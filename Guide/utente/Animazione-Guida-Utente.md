# Animazione dei controlli — guida utente

Il runtime sa animare il **rect** di un controllo (posizione e dimensione) senza
thread, timer di sistema né attese bloccanti: l'avanzamento avviene nel **frame**,
con il tempo realmente trascorso. Vale identico sulla TUI a celle e sul percorso
grafico/pixel, quindi le 4 gambe (IL, PCodeVM, VM/C, C99Native) si comportano allo
stesso modo.

## 1. Sintassi

L'animazione è una **proprietà** di `CONTROL SET`, non una clausola dello `STYLE`
(non cambia il controllo nel tempo in modo implicito: è il programma a chiederla).

```basic
CONTROL SET nome, "anim", 0, "proprietà:valore:durata_ms[:easing]"
```

| Campo | Obbligatorio | Valore |
|---|---|---|
| `proprietà` | sì | `left`/`x`, `top`/`y`, `width`/`w`, `height`/`h` |
| `valore` | sì | valore finale, nell'unità del controllo (celle in TUI, pixel in grafica) |
| `durata_ms` | sì | millisecondi; **0 = salto immediato** al valore finale |
| `easing` | no | `linear` (default), `out`, `inout` |

Esempi:

```basic
' il pannello scorre da sinistra in 400 ms, con decelerazione finale
CONTROL SET "pannello", "anim", 0, "left:300:400:out"

' la finestra si allarga in 250 ms (easing lineare)
CONTROL SET "finestra", "anim", 0, "width:80:250"

' salto immediato: serve a "posizionare" con la stessa superficie
CONTROL SET "badge", "anim", 0, "top:24:0"
```

La partenza è sempre il **valore corrente** della proprietà: non si dichiara.

## 2. Semantica

- **Un solo movimento per proprietà**: avviare una nuova animazione sulla stessa
  proprietà di un controllo sostituisce quella in corso (non si accodano).
- **La durata è il tempo totale**: a `durata_ms` trascorsi il valore è **esattamente**
  il target, e nello stesso frame il controllo viene ridisegnato.
- **Un'app ferma non produce un salto a metà**: al primo frame dopo una pausa
  l'animazione si chiude sul target.
- **`anim` vince sul reflow del layout**: il valore animato è applicato dopo
  `EXPAND`/`ANCHOR`/`ALIGN` e dopo WrapBox/Grid. Un controllo **animato** va quindi
  usato senza `EXPAND` sulla stessa proprietà: le due cose si contenderebbero il rect.
- **Un controllo distrutto o rinominato** non lascia slot orfani: lo slot viene
  dimenticato al primo frame in cui l'handle non esiste più.

## 3. Accessibilità: `ui.reduce_motion`

Se il flag `ui.reduce_motion` è attivo, **nessuna animazione parte**: il valore va
subito al target, esattamente come con `durata_ms = 0`. Il programma non cambia una
riga.

```bash
# dal registro opzioni (non è una variabile d'ambiente)
AI SET OPTION "ui.reduce_motion", "1"
```

È lo stesso flag che spegne il blink del cursore e i ridisegni periodici
(vedi `Guide/utente/STYLE-Ruoli-e-Layout-Guida-Utente.md` §7).

## 4. Perché non è un secondo esecutore (GPM)

L'animazione è **stato**, non un thread:

1. `CONTROL SET … "anim"` registra uno slot (`da`, `a`, `durata`, `easing`);
2. a ogni frame il runtime avanza lo slot con il delta del clock e scrive il valore
   nel controllo;
3. il disegno avviene solo finché qualcosa si muove.

Nessun callback esterno, nessuna attesa: il worker del GPM non viene mai bloccato e
nessun `SUB` BASIC viene invocato da un thread di sistema.

## 5. Disponibilità e verifiche

| Gamba | `anim` |
|---|---|
| IL | sì (parità per costruzione: proprietà dati) |
| PCodeVM | sì |
| VM/C | sì |
| C99Native | sì |

- Motore: `GMBasic/native/gmb_runtime_c/include/gmb_anim.h`,
  `src/gmb_anim.c`.
- Selfcheck deterministico (easing, durata, snap): target CMake
  `gmb_anim_selfcheck`.
- MiniTest a 4 gambe: `shared/minitests/controls/layout_anim.bas`
  (verifica il caso deterministico `durata_ms = 0` e che con durata > 0 il valore
  non salti nell'istante della `SET`).

## 6. Riferimenti

- Layout e ruoli del tema: `Guide/utente/STYLE-Ruoli-e-Layout-Guida-Utente.md`.
- Flag di interfaccia: `GMBasic/native/gmb_runtime_c/include/gmb_a11y.h`.
