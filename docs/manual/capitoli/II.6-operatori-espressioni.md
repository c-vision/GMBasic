# II.6 — Operatori ed espressioni

## Cosa ottieni da questo capitolo

La tabella di precedenza, la differenza fra `AND`/`OR` logici e le forme bitwise, e le due
trappole che producono risultati "strani" senza errori: il valore vero (`-1`, non `1`) e la
semantica a valore di `AND`.

## 1. Chi lega più forte

Dal più debole al più forte:

| Livello | Operatori | Note |
|---|---|---|
| 1 | `OR`, `XOR`, `AND` | `AND` lega più stretto di `XOR`, che lega più stretto di `OR` |
| 2 | `=` `<>` `<` `<=` `>` `>=` | producono `-1` (vero) o `0` (falso) |
| 3 | `+` `-` | binari; il `-` unario ha precedenza sul binario |
| 4 | `*` `/` `MOD` | |
| 5 | `^` | elevamento a potenza |
| 6 | parentesi | sempre risolte per prime |

Esempio che si sbaglia spesso: `8 XOR 1 AND 3` vale `11`, perché prima si calcola `1 AND 3` e
solo dopo lo XOR con `8`.

## 2. `AND` e `OR` non sono bitwise

Sono operatori **logici con short-circuit**, con una semantica a *valore*:

| Espressione | Risultato | Perché |
|---|---|---|
| `6 AND 3` | `3` | il sinistro è vero, quindi vale il **valore del destro** |
| `0 AND 5` | `0` | il sinistro è falso: il destro **non viene valutato** |
| `6 OR 0` | `-1` | il sinistro è vero: il risultato è il vero canonico |
| `0 OR 5` | `5` | il sinistro è falso: vale il valore del destro |

Il vero canonico è **`-1`**, non `1`: è la convenzione BASIC, e vale anche per i confronti.

Lo short-circuit non è un dettaglio di ottimizzazione, è semantica: protegge le espressioni che
non devono essere valutate.

```basic
IF den <> 0 AND (100 / den) > 1 THEN PRINT "ok"
```

## 3. Le forme bitwise

Quando servono i bit e non la verità:

| Operatore | Nome | Esempio |
|---|---|---|
| `&&` | AND bit a bit | `5 && 3` → `1` |
| `\|\|` | OR bit a bit | `5 \|\| 2` → `7` |
| `^^` | XOR bit a bit | `5 ^^ 3` → `6` |
| `!` | complemento | `!1` → `-2` |

Restano distinti anche `XOR` (bitwise, scritto per esteso) e `NOT` (negazione logica). La
presenza di due famiglie è voluta: `AND` risponde "è vero?", `&&` risponde "quali bit?".

## 4. Confronti e concatenazione

- Un confronto vale `-1` o `0`, anche dentro `AND`/`OR`/`XOR`.
- Il confronto fra stringhe è **lessicografico byte a byte**, case-sensitive: `"Gamma" < "gamma"`
  è vero, e `"10" > "9"` è **falso** perché si confrontano i caratteri, non i numeri.
- La concatenazione è `+`. Il carattere `&` **non** concatena: è l'operatore macro (§II.10).
- Incremento e decremento `++`/`--`, prefisso e postfisso, sono trattati in I.3, con la
  differenza fra `x = i++` e `x = ++i`. Le forme compatte `+=` e simili non fanno parte del
  linguaggio: si scrive `x = x + 1`.

## 5. `SWAP`

`SWAP a, b` scambia **in place** due variabili dello stesso tipo (numeri o stringhe) senza
variabile temporanea:

```basic
DIM a AS LONG : DIM b AS LONG
a = 10 : b = 27
SWAP a, b        ' a=27, b=10
```

## 6. Esempio eseguito

`shared/samples/Operators/XorSwapShortCircuit.bas`, backend C99Native:

```
xor= 5
xor_neg=-6
prec1= 2
prec2= 11
prec3=-1
swap_num= 27 , 10
swap_str=secondo,primo
sc= 0 ,-1 ,chiamate= 0
div=protetta
checksum= 15
```

Riga per riga:

- `xor= 5` è `6 XOR 3`: XOR bit a bit, non "uno dei due è vero".
- `xor_neg=-6` è `5 XOR -1`: lo XOR con tutti i bit accesi dà il complemento a uno.
- `prec2= 11` è `8 XOR 1 AND 3`, la prova che `AND` lega più stretto di `XOR`.
- `swap_num` e `swap_str` mostrano che `SWAP` funziona su numeri e su stringhe.
- `sc= 0 ,-1 ,chiamate= 0` è la prova dello short-circuit: il lato destro non è stato eseguito
  (`chiamate= 0`) e il risultato è comunque corretto.
- `div=protetta` è lo short-circuit usato come protezione: la divisione per zero non è mai
  avvenuta.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| un `IF` composto dà risultati inattesi | si è assunto `AND` bitwise | ricorda la semantica a valore; usa `&&` per i bit |
| `PRINT (1 = 1)` stampa `-1` e sembra sbagliato | il vero canonico è `-1` | confronta con `<> 0`, non con `= 1` |
| `"10" > "9"` è falso | confronto lessicografico | converti con `VAL()` prima di confrontare |
| `"a" & "b"` non concatena | `&` è la macro | usa `+` |
| una divisione esplode dentro un `AND` | manca la protezione sul lato sinistro | `den <> 0 AND (...)` |

## 8. Dove andare adesso

- **II.7** — come queste espressioni diventano condizioni e cicli.
- **II.9** — `+` su array e collezioni non concatena: ogni tipo ha le sue operazioni.
- **II.10** — `&` come macro, `EVAL` e code block.
- **V.27** — le funzioni matematiche che si compongono con questi operatori.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### XOR-AND-OR-SWAP-Guida-Utente

*Fonte: `XOR-AND-OR-SWAP-Guida-Utente`*


## 1. `XOR` — OR esclusivo bitwise

### Sintassi

risultato = a XOR b


### Parametri
| Operando | Tipo | Descrizione |
|---|---|---|
| `a` | numerico | operando sinistro (convertito a intero a 64 bit) |
| `b` | numerico | operando destro (convertito a intero a 64 bit) |

### Semantica

### Esempi

PRINT 6 XOR 3          ' 5
PRINT 12 XOR 10        ' 6
PRINT 255 XOR 15       ' 240
PRINT 5 XOR -1         ' -6
PRINT 8 XOR 1 AND 3    ' 11  (AND più stretto: 1 AND 3 = 3 -> 8 XOR 3)


## 2. `AND` / `OR` — logici con short-circuit

### Sintassi

risultato = a AND b
risultato = a OR  b


### Parametri
| Operando | Tipo | Descrizione |
|---|---|---|
| `a` | qualsiasi | operando sinistro (valutato per primo) |
| `b` | qualsiasi | operando destro (**valutato solo se il sinistro non decide**) |

### Semantica (canone condiviso da tutti i backend)

### Esempi

PRINT 6 AND 3      ' 3   (sinistra vera -> valore destro)
PRINT 0 AND 5      ' 0   (sinistra falsa -> 0, destra non valutata)
PRINT 6 OR 0       ' -1  (sinistra vera -> TRUE)
PRINT 0 OR 5       ' 5   (sinistra falsa -> valore destro)

' short-circuit: il ramo destro non viene eseguito
IF den <> 0 AND (100 / den) > 1 THEN PRINT "ok"


### Operandi confronto e BIT (canone booleano -1/0, parità 5/5 dal 2026-09-30)

PRINT (2 <> 0) AND (3 <> 0)    ' -1
PRINT (2 = 0) OR (3 <> 0)      ' -1
PRINT (2 <> 0) XOR (3 = 0)     ' -1
PRINT ("a" = "a") AND (1 <> 0) ' -1


DIM f AS BIT = -1
DIM g AS BIT = 0
PRINT (2 <> 0) AND f    ' -1
PRINT f XOR g           ' -1


### Verifica

## 3. `SWAP` — scambia due variabili

### Sintassi

SWAP a, b


### Parametri
| Parametro | Descrizione |
|---|---|
| `a` | variabile sinistra (numerica o stringa) |
| `b` | variabile destra (dello stesso tipo di `a`) |

### Semantica

DIM a AS LONG
DIM b AS LONG
a = 10 : b = 27
SWAP a, b            ' ora a=27, b=10

DIM s1 AS STRING
DIM s2 AS STRING
s1 = "primo" : s2 = "secondo"
SWAP s1, s2          ' ora s1="secondo", s2="primo"


## Note di parità (4 backend)

<!-- END SURFACE -->
