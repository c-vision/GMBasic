# II.7 — Controllo di flusso

## Cosa ottieni da questo capitolo

Le forme di decisione e di ciclo, con le regole che generano gli errori più frequenti: quando un
corpo va su riga nuova, perché un ciclo non entra, e perché il contatore "sparisce" dopo il
`NEXT`.

## 1. `IF`

Tre forme, tutte valide:

```basic
' 1. blocco
IF x > 10 THEN
    PRINT "grande"
ELSEIF x > 5 THEN
    PRINT "medio"
ELSE
    PRINT "piccolo"
END IF

' 2. riga singola
IF x > 10 THEN PRINT "grande" ELSE PRINT "piccolo"

' 3. senza ELSE su riga singola
IF pronto THEN avvia
```

La forma su riga singola funziona perché `THEN` ed `ELSE` fanno da separatori. Questa è
l'eccezione: per `SELECT CASE` la regola è l'opposta (§2).

## 2. `SELECT CASE`

```basic
SELECT CASE valore
    CASE 1                  ' uguaglianza
    CASE 2, 3, 4            ' lista (OR)
    CASE 5 TO 10            ' intervallo inclusivo
    CASE IS > 20            ' confronto esplicito
    CASE ELSE               ' fallback
END SELECT
```

`CASE IS` accetta `=`, `<>`, `>`, `<`, `>=`, `<=` e funziona anche sulle stringhe, con confronto
lessicografico.

> **Regola di coerenza.** Il corpo di un `CASE` va **sempre** su righe successive: `CASE IS > 20:`
> seguito dal corpo sulla stessa riga è un errore (`Expected newline after CASE conditions.`) su
> tutti i backend. La forma `CASE <valore> => <corpo>` esiste, ma è un'estensione del backend C
> nativa in stile Clipper, separata da questa regola.

La ragione della regola: `:` dopo la condizione rende ambiguo dove finisce il test e comincia il
corpo, e storicamente un backend lo accettava mentre le altre lo rifiutavano. Il linguaggio ha
scelto di rifiutarlo ovunque.

## 3. Cicli

### `FOR … NEXT`

```basic
FOR i = 1 TO 10 STEP 2
    totale = totale + i
NEXT
```

- I **bound si valutano una volta**, all'ingresso: cambiarli nel corpo non allunga il ciclo.
- `STEP` negativo conta all'indietro; se il bound è già oltre, il corpo **non gira affatto**.
- `EXIT FOR` esce subito, `CONTINUE FOR` salta all'incremento.
- Il contatore è **block-scoped**: dopo `NEXT` torna visibile l'eventuale variabile omonima
  dichiarata fuori. Senza una `DIM` esterna il contatore muore con il ciclo, e usarlo dopo non è
  specificato.

### `DO … LOOP`

```basic
DO WHILE condizione      ' pre-test: può non girare
LOOP

DO                        ' post-test: gira almeno una volta
LOOP UNTIL condizione
```

`EXIT DO` esce subito; `CONTINUE` salta alla valutazione della condizione. `WHILE … WEND` è la
forma storica equivalente al `DO WHILE`, e resta accettata.

### `FOR EACH`

Per scorrere array, matrici, liste e stringhe senza gestire l'indice:

```basic
FOR EACH nome IN nomi
    PRINT nome
NEXT
```

## 4. Uscite non locali

| Costrutto | Uso | Nota |
|---|---|---|
| `EXIT SUB` / `EXIT FUNCTION` | uscita anticipata da una procedura | preferito a `GOTO` |
| `RETURN` | valore di una `FUNCTION` | in una `SUB` termina la procedura |
| `GOTO etichetta` | salto | legacy, accettato; le etichette sono righe `nome:` |
| `GOSUB … RETURN` | chiamata di sottoprogramma | legacy; per il codice nuovo usare `SUB` |
| `ON ERROR` | gestione errori legacy | vedi III.14; per il codice nuovo `TRY … CATCH` |

## 5. Esempio eseguito (decisioni)

`shared/samples/Branches/BranchesSelectCase.bas`, backend C99Native:

```
--- Testando SELECT CASE con Valore = 3 ---
  Trovato: Due, Tre o Quattro (Lista di Valori)

--- Testando SELECT CASE con Valore = 7 ---
  Trovato: Un valore tra 5 e 10 (Intervallo)

--- Testando SELECT CASE con Valore = 25 ---
  Trovato: Un valore maggiore di 20 (Comparazione)

--- Testando SELECT CASE con Valore = 15 ---
  Trovato: Altro Valore (Caso non gestito)

--- Test di EXIT FOR ---
  Iterazione FOR: 1
  Iterazione FOR: 2
  Iterazione FOR: 3
```

Le prime quattro sezioni mostrano le quattro forme di `CASE` sullo stesso programma: lista,
intervallo, comparazione, fallback. L'ultima mostra `EXIT FOR` che ferma il ciclo: le iterazioni
stampate sono tre, non dieci.

## 6. Esempio eseguito (cicli)

`shared/samples/Loops/LoopsBasics.bas`, backend C99Native:

```
--- TEST 1: DO...LOOP UNTIL (Post-Test) ---
Iterazione DO...LOOP UNTIL:  1
Iterazione DO...LOOP UNTIL:  2
Iterazione DO...LOOP UNTIL:  3
Fine del loop.

--- TEST 2: DO...LOOP WHILE (Post-Test) ---
Iterazione DO...LOOP WHILE:  1
Iterazione DO...LOOP WHILE:  2
Fine del loop.

--- TEST 3: DO WHILE...LOOP (Pre-Test) ---
Iterazione DO WHILE (Pre-Test):  1
Iterazione DO WHILE (Pre-Test):  2
```

Il confronto fra i primi due test è la lezione: **post-test** (`LOOP UNTIL`/`LOOP WHILE`) esegue
il corpo almeno una volta e controlla dopo; **pre-test** (`DO WHILE`) controlla prima e può non
eseguire nulla. Il terzo test stampa le iterazioni in ordine diverso proprio perché la condizione
è valutata in testa.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `Expected newline after CASE conditions.` | corpo del `CASE` sulla stessa riga | vai a capo dopo la condizione |
| il ciclo non entra mai | bound già superato, o variabile del bound non ancora assegnata | controlla `inizio`/`fine`/`STEP` |
| il ciclo gira troppe volte | il bound è stato modificato dentro il corpo | i bound si leggono all'ingresso: usa una variabile separata |
| dopo `NEXT` la variabile omonima sembra persa | contatore block-scoped | dichiara la variabile fuori e usa un altro nome per il contatore |
| `EXIT FOR` non compila | è scritto dentro un `DO` | `EXIT DO` per i cicli `DO` |

## 8. Dove andare adesso

- **II.5** — scope e ciclo di vita delle variabili usate nelle condizioni.
- **II.9** — `FOR EACH` su collezioni, matrici, stringhe.
- **III.13** — `DOEVENTS` e cicli nei programmi concorrenti: un ciclo lungo deve cedere il turno.
- **III.14** — `TRY … CATCH` e `ON ERROR` in dettaglio.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Loops-FOR-Guida-Utente

*Fonte: `Loops-FOR-Guida-Utente`*


## Sintassi

FOR contatore = inizio TO fine [STEP passo]
    ' corpo del ciclo
NEXT [contatore]

| Elemento | Tipo | Obbligatorio | Descrizione |
|---|---|---|---|
| `contatore` | variabile numerica | sì | Riceve il valore corrente a ogni iterazione. Può essere `INTEGER`, `LONG`, `SINGLE`, `DOUBLE`. Non serve dichiararla prima (la crea il ciclo), ma `DIM` è la forma consigliata. |
| `inizio` | numero o espressione | sì | Valore del contatore alla prima iterazione. |
| `fine` | numero o espressione | sì | Limite: il ciclo termina quando il contatore lo supera nel verso del passo. |
| `STEP passo` | numero o espressione | no (default `1`) | Incremento a ogni `NEXT`. Negativo = conteggio all'indietro. |
| `NEXT [contatore]` | — | sì | Chiude il ciclo. Il nome dopo `NEXT` è facoltativo; se scritto, deve essere il contatore che si sta chiudendo. |

## Semantica

## Cicli annidati

FOR r = 1 TO 3
    FOR c = 1 TO 3
        cell = cell + "#"          ' ciclo interno
    NEXT
    rows = rows + cell + ";"       ' ciclo esterno
NEXT


## Pattern di accumulazione

acc = 0
FOR i = 1 TO 10
    acc = acc + i                  ' somma = 55
NEXT

p = 1
FOR i = 1 TO 5
    p = p * i                      ' prodotto (fattoriale) = 120
NEXT


s$ = ""
FOR i = 1 TO 4
    s$ = s$ + "ab"                 ' "abababab"
NEXT

t$ = ""
FOR i = 1 TO 3
    t$ = t$ + STR$(i)              ' " 1 2 3"  (STR$ prepende lo spazio ai positivi)
NEXT


## Fusione del ciclo (ottimizzazione interna — PCodeVM e VM/C)
| `term_mode` | Forma (numeri) | `term_mode` | Forma (stringhe) |
|---|---|---|---|
| 0 | `acc = acc OP f(I) * m` | 0 | `s$ = s$ + "letterale"` |
| 1 | `acc = acc OP f(I)` | 1 | `s$ = s$ + STR$(I)` / `CSTR(I)` |
| 2 | `acc = acc OP I * I` | — | — |
| 3 | `acc = acc OP I * m` | — | — |
| 4 | `acc = acc OP I` | — | — |
| 5 | `acc = acc + f(I) + g(I)` | — | — |

## Diagnosi
| Sintomo | Causa probabile |
|---|---|
| Il corpo gira **una volta sola** e il contatore finisce subito al valore finale | Fusione applicata a un corpo con più statement (risolto 2026-09-29 con il guard "corpo unico" — `Bugs/(solved) - PCode-For-In-Function-Corpo-Loop.md`). |
| Un **termine sparisce** dal totale, solo su PCodeVM/VM/C | Chain a 3 operandi accettata dal matcher (risolto 2026-09-29: la chain non è più fusa). |
| Il ciclo non entra mai | `fine` già oltre `inizio` con passo concorde, oppure la variabile usata come bound non era ancora stata assegnata (i bound si valutano all'ingresso). |
| Dopo il ciclo la variabile omonima sembra "persa" | Il contatore è block-scoped: fuori dal ciclo vale la `DIM` esterna. |

## Verificare la parità di un ciclo

tools/sh/parity4 shared/samples/Loops/LoopsAccumulatorPatterns.bas   # 4 backend, output a confronto


### Goto-Gosub-OnError-Guide

*Fonte: `Goto-Gosub-OnError-Guide`*


## 1. GOTO / GOSUB con label vs numeri di linea

### Top-level (fuori da SUB/FUNCTION)
| Costrutto | Supportato | Note |
|-----------|------------|------|
| `GOTO 100` | ✅ | Numero di linea |
| `GOTO label` | ✅ | Etichetta testuale |
| `GOSUB 100` | ✅ | Chiama subroutine, RETURN torna indietro |
| `GOSUB label` | ✅ | Etichetta testuale |
| `10 PRINT "A"` | ✅ | I numeri di linea sono validi solo al top-level |

### Dentro SUB/FUNCTION
| Costrutto | Supportato | Note |
|-----------|------------|------|
| `GOTO label` | ✅ | Solo etichette testuali — i numeri di linea sono **bannati** |
| `GOSUB label` | ✅ | Etichetta testuale |
| `GOTO 100` | ❌ | `Line numbers are not allowed inside a FUNCTION or SUB block` |
| `10 PRINT "A"` | ❌ | Stesso errore |

### Regola di scoping

SUB Alpha()
    targetA:        ← visibile solo dentro Alpha()
END SUB

SUB Main()
    GOTO targetA    ← ERRORE: Etichetta non definita
END SUB


## 2. ON ERROR — tutte le varianti

### Tabella riassuntiva
| Costrutto | Top-level | Dentro SUB | Note |
|-----------|-----------|------------|------|
| `ON ERROR GOTO 0` | ✅ | ✅ | Disabilita gestione errori |
| `ON ERROR GOTO 100` | ✅ | ❌ | Solo top-level (numeri bannati in SUB) |
| `ON ERROR GOTO label` | ✅ | ✅ | Salta al handler, **RESUME NEXT** continua dopo RESUME |
| `ON ERROR GOSUB 100` | ✅ | ❌ | Solo top-level (numeri bannati in SUB) |
| `ON ERROR GOSUB label` | ✅ | ✅ | **Dopo fix 2026-08-06**: prima bloccato dal parser |
| `ON ERROR RESUME NEXT` | ✅ | ✅ | Ignora errore e continua |

### Differenza GOTO vs GOSUB per ON ERROR
| | ON ERROR GOTO | ON ERROR GOSUB |
|---|---|---|
| Salto al handler | Saltando (GOTO) | Chiamando (GOSUB) |
| RESUME NEXT | Continua da dopo il RESUME | **Torna all'istruzione dopo l'errore** |
| RESUME | Ritenta l'handler | **Torna all'istruzione dell'errore (retry)** |
| IpStack | Non usato | Salva IP prima del salto, RESUME ripristina |
| Implementazione VM | `OP_SET_ERR_HANDLER_GOTO` | `OP_SET_ERR_HANDLER_GOSUB` + IpStack push/pop |

### Esempio ON ERROR GOSUB dentro SUB

SUB Main()
    ON ERROR GOSUB handler
    PRINT 1/0           ' ← causa errore, salva IP, salta a handler
    PRINT "RESUMED"     ' ← RESUME NEXT torna qui
    EXIT SUB
handler:
    PRINT "IN HANDLER"
    RESUME NEXT         ' ← pop IpStack, salta istruzione errore
END SUB


## 3. Fix implementati (2026-08-06)
| # | Commit legacy (GMBasicAI) | Commit legacy (GM-Basic) | Descrizione |
|---|---------------------------|--------------------------|-------------|
| 1 | `225794e` | `7f097e16` | Label scoping: rimosso fallback `GetAllLabelsDebug` che bucava lo scope |
| 2 | `76d641f` | `c321859e` | ON ERROR GOSUB: salva return IP su IpStack, RESUME NEXT/RESUME funzionanti |
| 3 | `2294d44` | `2c03d672` | Unificato GOTO/GOSUB: ON ERROR GOSUB ora accetta label in SUB/FUNCTION |

## 4. File toccati

### SelectCase-IS-Comparazione-Guida-Utente

*Fonte: `SelectCase-IS-Comparazione-Guida-Utente`*


## Panoramica

## Sintassi

SELECT CASE <espressione>
    CASE <valore>                  ' uguaglianza semplice
    CASE <v1>, <v2>, <v3>          ' lista di valori (OR)
    CASE <lo> TO <hi>              ' intervallo inclusivo
    CASE IS > <valore>             ' comparazione esplicita
    CASE IS <= <valore>
    CASE ELSE                      ' fallback
END SELECT


### Esempio

DIM r AS STRING
SELECT CASE 25
    CASE 1
        r = "uno"
    CASE 2, 3, 4
        r = "lista"
    CASE 5 TO 10
        r = "range"
    CASE IS > 20
        r = "gt"          ' 25 > 20 → questo ramo
    CASE ELSE
        r = "altro"
END SELECT
PRINT r                    ' gt


SELECT CASE 25
    CASE IS = 25   ' vero
    CASE IS <> 25  ' falso
    CASE IS > 20   ' vero
    CASE IS < 20   ' falso
    CASE IS >= 25  ' vero
    CASE IS <= 24  ' falso
END SELECT


SELECT CASE "mario"
    CASE IS < "n"
        PRINT "prima di n"   ' "mario" < "n" → questo ramo
    CASE ELSE
        PRINT "da n in poi"
END SELECT


## Regola di coerenza: il corpo di un CASE è SEMPRE su righe successive

' NON VALIDO su nessun backend — errore di parse "Expected newline after CASE conditions."
CASE IS > 20: r = "gt"


' VALIDO — corpo su riga successiva
CASE IS > 20
    r = "gt"


SELECT CASE x
    CASE 1 => y = "uno"
    CASE IS > 20 => y = "gt"
    CASE ELSE => y = "altro"
END SELECT


## Parità Backend
| Backend | `CASE IS` (tutti gli operatori) | Rifiuta `CASE ...:` | Parità |
|---------|:-:|:-:|:-:|
| IL | ✓ | ✓ | 5/5 |
| PCodeVM | ✓ | ✓ | 5/5 |
| VM/C | ✓ | ✓ | 5/5 |
| C99Native | ✓ (fix 2026-09-21) | ✓ (fix 2026-09-21) | 5/5 |

## Storia (bug risolti)

## MiniTest

tools/sh/test_minitests_parity.sh constructs/branching_selectcase   # IL/PCode/VM-C (xUnit)
GMB_MINI_ONLY=constructs/branching_selectcase tools/sh/test_minitests_c99native
# [storico] gate C99Native rimosso il 2026-10-25 — oggi: tools/sh/test_minitests_c99native
GMB_MINI_ONLY="constructs/types_class_ctor" tools/sh/test_minitests_c99native   # backend C99Native


## Sample

## Riferimenti

<!-- END SURFACE -->
