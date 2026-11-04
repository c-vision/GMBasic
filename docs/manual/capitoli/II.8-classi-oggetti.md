# II.8 — Classi e oggetti

## Cosa ottieni da questo capitolo

La differenza fra un `TYPE` e una `CLASS`, come si dichiara un oggetto, come si eredita una
classe astratta e perché l'array di tipo base che contiene oggetti derivati è il cuore del
polimorfismo.

## 1. `TYPE` o `CLASS`: si copia o si condivide

| | `TYPE` (UDT) | `CLASS` |
|---|---|---|
| Semantica | valore: l'assegnazione **copia** | riferimento: l'assegnazione **condivide** |
| Ereditarietà | no | sì (`INHERITS`) |
| Polimorfismo | no | sì (`VIRTUAL` / `OVERRIDE`) |
| Incapsulamento | parziale | `PUBLIC` / `PRIVATE` / `PROTECTED` / `FRIEND` |
| Costruttore | `SUB NEW` | `SUB NEW` |

La differenza che conta si vede subito:

```basic
TYPE PuntoT
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

CLASS PuntoC
    PUBLIC DIM X AS DOUBLE
    PUBLIC DIM Y AS DOUBLE
END CLASS

DIM a AS PuntoT
a.X = 1
DIM b AS PuntoT = a
b.X = 99
PRINT a.X                 ' 1: copia indipendente

DIM c AS PuntoC = NEW PuntoC()
c.X = 1
DIM c2 AS PuntoC = c
c2.X = 99
PRINT c.X                 ' 99: è lo stesso oggetto
```

**Regola pratica:** `TYPE` per i record (righe di tabella, coordinate, pacchetti di dati),
`CLASS` quando identità e comportamento devono viaggiare insieme o quando serve ereditarietà.

## 2. Dichiarare una classe

```basic
PUBLIC CLASS Player
    PUBLIC    DIM Nome AS STRING * 50
    PRIVATE   DIM Health AS INTEGER = 100
    PROTECTED DIM Mana AS INTEGER = 50
    FRIEND    DIM Exp AS LONG

    PUBLIC SUB NEW(nome AS STRING)
        SELF.Nome = nome
    END SUB

    PUBLIC FUNCTION Vivo() AS BIT
        RETURN SELF.Health > 0
    END FUNCTION
END CLASS
```

| Accesso | Visibile da |
|---|---|
| `PUBLIC` | ovunque |
| `PRIVATE` | solo dalla classe (default) |
| `PROTECTED` | dalla classe e dalle derivate |
| `FRIEND` | da tutto il namespace/modulo |

L'istanza si crea con `NEW`, che alloca ed esegue il costruttore:

```basic
DIM p AS Player = NEW Player("Aragorn")
```

## 3. Membri che non sono scalari

Un campo può essere un **array**, un **array di UDT** o un **UDT annidato**:

```basic
CLASS Portfolio
    PUBLIC DIM holdings(4) AS Holding      ' array di UDT, per-istanza
    PUBLIC DIM somma AS INTEGER
END CLASS
```

Regole:

- l'array membro è **per istanza**: due oggetti non condividono il buffer, e `NEW` lo alloca;
- dentro i metodi si accede nudo (`holdings(i).shares`) o qualificato (`SELF.holdings(i).shares`):
  sono equivalenti;
- un `DIM x(3)` **dentro un metodo** è un locale della procedura, non stato dell'oggetto: è la
  distinzione che fa la differenza fra un campo e una variabile temporanea;
- nei `TYPE`, una `STRING` dinamica non è ammessa: serve `STRING * N`.

## 4. Ereditarietà, astrattezza, override

```basic
PUBLIC ABSTRACT CLASS Figura
    PUBLIC DIM Nome AS STRING * 30

    PUBLIC SUB NEW(nome AS STRING)
        SELF.Nome = nome
    END SUB

    PUBLIC ABSTRACT FUNCTION Area() AS DOUBLE   ' contratto: nessun corpo
END CLASS

PUBLIC CLASS Cerchio INHERITS Figura
    PUBLIC DIM Raggio AS DOUBLE

    PUBLIC SUB NEW(nome AS STRING, r AS DOUBLE)
        BASE.NEW(nome)                          ' costruttore della base
        SELF.Raggio = r
    END SUB

    PUBLIC OVERRIDE FUNCTION Area() AS DOUBLE
        RETURN 3.14159 * SELF.Raggio ^ 2
    END FUNCTION
END CLASS
```

- Una classe `ABSTRACT` **non si istanzia**: `NEW Figura(...)` è un errore di compilazione.
- Un metodo `ABSTRACT` non ha corpo ed è implicitamente `VIRTUAL`.
- Una classe concreta che eredita **deve** implementare tutti i metodi astratti con `OVERRIDE`.
- `BASE.NEW(...)` incatena il costruttore della classe base.

## 5. Polimorfismo

Il punto di arrivo: un array del tipo base contiene oggetti derivati, e la chiamata esegue
l'implementazione giusta.

```basic
DIM figure(1) AS Figura
figure(0) = NEW Cerchio("Cerchio Grande", 10)
figure(1) = NEW Rettangolo("Rettangolo Blu", 5, 8)

FOR EACH f IN figure
    f.Disegna()               ' quale Disegna? quella dell'oggetto, non del tipo dichiarato
    PRINT f.Area()
NEXT
```

Questo è ciò che `TYPE` non può fare: la scelta dell'implementazione avviene a runtime, in base
all'oggetto contenuto.

## 6. Esempio eseguito

`shared/samples/Classes/ClassesAbstract.bas`, backend C99Native:

```
--- 1. Test Polimorfismo su Array di tipo Astratto ---
Questa figura è un/una: Cerchio Grande
Disegno un cerchio di raggio  10
   Area:  314.159

Questa figura è un/una: Rettangolo Blu
Disegno un rettangolo di dimensioni  5 x 8
   Area:  40
```

Cosa dimostrano le quattro righe:

1. **`MostraNome` è concreto e ereditato**: stampa il nome impostato dal costruttore della
   classe base (`BASE.NEW`), quindi l'incatenamento funziona.
2. **`Disegna` è astratto e risolto a runtime**: per il cerchio stampa il raggio, per il
   rettangolo le due dimensioni — due implementazioni diverse dietro la stessa chiamata.
3. **`Area` usa `OVERRIDE`**: `314.159` è `3.14159 × 10²`, `40` è `5 × 8`. Il tipo dichiarato
   dell'elemento è `Figura`, il comportamento è quello della classe derivata.

L'ultima sezione del sample è un test negativo commentato: decommentando `NEW Figura(...)` il
programma non compila, che è la prova che la classe astratta non è istanziabile.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `NEW` su una classe astratta non compila | la classe è `ABSTRACT` | istanzia una derivata concreta |
| "manca l'implementazione" di un metodo | la derivata non ha `OVERRIDE` per un astratto | implementa tutti i metodi astratti |
| una modifica a un oggetto "si propaga" | è una `CLASS`: riferimento condiviso | se vuoi una copia, usa un `TYPE` o clona esplicitamente |
| due istanze condividono i dati di un array | l'array è dichiarato in un metodo, non nella classe | sposta il `DIM` nel corpo della classe |
| il campo stringa mostra spazi in coda | è `STRING * N` | usa `TRIM` in lettura |
| `SELF` non riconosciuto | sei fuori dal corpo della classe | qualifica con il nome dell'istanza |

## 8. Dove andare adesso

- **II.5** — scope e tipi, per capire il ciclo di vita dei campi.
- **II.9** — array e collezioni di oggetti.
- **IV.20** — i controlli utente sono classi: proprietà, metodi, eventi.
- **IV.23** — mappare una classe su una tabella.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Classes-User-Manual

*Fonte: `Classes-User-Manual`*


## 1. Perché le classi? Introduzione alla OOP

### 1.1 Classi vs UDT (`TYPE`)
| | `TYPE` (UDT) | `CLASS` |
|---|---|---|
| Semantica | **value-type** (si copia per valore) | **reference-type** (si condivide per riferimento) |
| Ereditarietà | no (solo campi/metodi/operatori in un UDT) | sì (`INHERITS`) |
| Polimorfismo | no | sì (`VIRTUAL`/`OVERRIDE`) |
| Incapsulamento | parziale (`PUBLIC`/`PRIVATE`) | completo (`PUBLIC`/`PRIVATE`/`PROTECTED`/`FRIEND`) |
| Costruttore | `SUB NEW` | `SUB NEW` |
| Distruttore | `SUB DESTRUCTOR` | `SUB DESTRUCTOR` |

TYPE PuntoT
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

CLASS PuntoC
    PUBLIC DIM X AS DOUBLE
    PUBLIC DIM Y AS DOUBLE
END CLASS

SUB Main()
    ' UDT: copia per valore → modificare b NON tocca a
    DIM a AS PuntoT
    a.X = 1
    DIM b AS PuntoT = a
    b.X = 99


## 2. Definire una classe

[PUBLIC] CLASS NomeClasse
    ' membri: campi, metodi, proprietà, costruttori, distruttore, operatori
END CLASS


DIM p AS Player = NEW Player()
DIM p2 AS Player = NEW Player("Aragorn")   ' con argomenti al costruttore


## 3. Membri di una classe

### 3.1 Campi (`DIM`)

CLASS Player
    PUBLIC  DIM Nome AS STRING * 50     ' visibile ovunque
    PRIVATE DIM Health AS INTEGER = 100 ' visibile solo dentro la classe (default)
    PROTECTED DIM Mana AS INTEGER = 50  ' visibile nella classe e nelle derivate
    FRIEND  DIM Exp AS LONG             ' visibile nel namespace/modulo
END CLASS

| Accesso | Visibile da |
|---|---|
| `PUBLIC` | ovunque |
| `PRIVATE` | solo dalla classe stessa (**default**) |
| `PROTECTED` | dalla classe e dalle classi derivate |
| `FRIEND` | da tutto il namespace/modulo |

### 3.1-bis Array come campo (membro di classe)

[PUBLIC|PRIVATE|PROTECTED|FRIEND] DIM nome(N) AS tipo


CLASS Punto
    PUBLIC DIM storico(4) AS INTEGER
    PUBLIC DIM somma AS INTEGER

    PUBLIC SUB NEW()
        SELF.somma = 0
    END SUB

    SUB Push(v AS INTEGER)
        storico(somma) = v             ' scrittura NUDa (indice = campo somma)
        SELF.somma = SELF.somma + 1    ' scrittura qualificata
    END SUB

    FUNCTION Media() AS DOUBLE
        DIM i AS INTEGER
        DIM t AS DOUBLE


TYPE Holding
    shares AS INTEGER
    cost AS DOUBLE
END TYPE

CLASS Portfolio
    PUBLIC DIM holdings(4) AS Holding

    SUB Set(i AS INTEGER, sh AS INTEGER, c AS DOUBLE)
        holdings(i).shares = sh            ' campo dell'elemento (NUDO)
        SELF.holdings(i).cost = c          ' campo dell'elemento (qualificato)
    END SUB

    FUNCTION Shares(i AS INTEGER) AS INTEGER
        Shares = SELF.holdings(i).shares   ' lettura del campo dell'elemento
    END FUNCTION


### 3.1-ter Campo di tipo UDT (annidato) e stringhe a lunghezza fissa
| Forma | Dove | Significato |
|---|---|---|
| `r.campo` | dentro un metodo della classe | campo `campo` del campo UDT `r` (≡ `SELF.r.campo`) |
| `SELF.r.campo` | dentro un metodo | forma qualificata esplicita |
| `istanza.r.campo` | fuori dalla classe | sul campo UDT di un'istanza |
| `a.b.c` | ovunque | catena annidata a più livelli (ogni segmento intermedio è un campo UDT), **anche esterna** (`istanza.a.b.c`) |
| `istanza.arr(i,j).campo` | fuori dalla classe | campo di un elemento di un array-campo di UDT (1-D/2-D), su un'istanza |

TYPE Coordinate
    Lat AS DOUBLE
    Lon AS DOUBLE
END TYPE

CLASS Luogo
    PUBLIC DIM coord AS Coordinate
    PUBLIC SUB NEW()
        coord.Lat = 45.07              ' accesso nudo (≡ SELF.coord.Lat)
        SELF.coord.Lon = 9.18          ' accesso qualificato
    END SUB
    FUNCTION Desc() AS STRING
        Desc = "lat=" + STR(coord.Lat) + " lon=" + STR(coord.Lon)
    END FUNCTION
END CLASS


TYPE Riga
    Codice AS STRING * 8      ' sempre 8 caratteri (riempito con spazi)
    Qta AS INTEGER
END TYPE


### 3.2 Costruttori (`SUB NEW`)

CLASS Player
    PUBLIC  DIM Nome AS STRING * 50
    PRIVATE DIM Health AS INTEGER

    PUBLIC SUB NEW(nomeIniziale AS STRING)
        SELF.Nome = nomeIniziale
        SELF.Health = 100
    END SUB
END CLASS

DIM p AS Player = NEW Player("Aragorn")


### 3.3 Metodi (`SUB` e `FUNCTION`)

CLASS Player
    ' ... campi ...
    PUBLIC SUB TakeDamage(damage AS INTEGER)
        SELF.Health = SELF.Health - damage
        IF SELF.Health < 0 THEN SELF.Health = 0
    END SUB

    PUBLIC FUNCTION IsAlive() AS BIT
        FUNCTION = (SELF.Health > 0)     ' forma canonica di return
    END FUNCTION
END CLASS

' uso
p.TakeDamage(30)
IF p.IsAlive() THEN PRINT "vivo" ELSE PRINT "morto"


PUBLIC FUNCTION GetName() AS STRING
    GetName = SELF.Nome        ' equivalente: FUNCTION = SELF.Nome
END FUNCTION


### 3.4 Proprietà (`PROPERTY`)

CLASS Player
    PRIVATE DIM _level AS INTEGER = 1

    PUBLIC PROPERTY Level AS INTEGER
        GET
            RETURN SELF._level          ' il GET a blocco usa RETURN
        END GET
        SET(BYVAL value AS INTEGER)
            IF value > 0 AND value <= 100 THEN
                SELF._level = value
            END IF
        END SET
    END PROPERTY
END CLASS

' Equivalente a corpo di espressione (=>):


### 3.5 Operatori (`OPERATOR`)

CLASS Vector2D
    PUBLIC DIM X AS DOUBLE
    PUBLIC DIM Y AS DOUBLE

    PUBLIC SUB NEW(x AS DOUBLE, y AS DOUBLE)
        SELF.X = x
        SELF.Y = y
    END SUB

    PUBLIC OPERATOR +(BYVAL a AS Vector2D, BYVAL b AS Vector2D) AS Vector2D
        OPERATOR = NEW Vector2D(a.X + b.X, a.Y + b.Y)
    END OPERATOR
END CLASS

DIM v AS Vector2D = NEW Vector2D(1, 2) + NEW Vector2D(3, 4)
PRINT v.X; ", "; v.Y      ' 4 , 6


### 3.6 Distruttori (`SUB DESTRUCTOR`)

CLASS Risorsa
    PUBLIC DIM Handle AS LONG

    PUBLIC SUB NEW()
        SELF.Handle = OpenResource()
    END SUB

    PUBLIC SUB DESTRUCTOR
        CloseResource(SELF.Handle)     ' rilascio automatico
    END SUB
END CLASS


## 4. Ereditarietà (`INHERITS`)

CLASS Animale
    PUBLIC DIM Nome AS STRING

    PUBLIC SUB NEW(nome AS STRING)
        SELF.Nome = nome
    END SUB

    PUBLIC SUB Parla()
        PRINT "..."
    END SUB
END CLASS

CLASS Cane INHERITS Animale
    PUBLIC SUB NEW(nome AS STRING)
        BASE.NEW(nome)                 ' obbligatorio: chiama il costruttore base
    END SUB


### 4.1 Base qualificata o da un altro file (fix Z3)

IMPORT MVC

PUBLIC CLASS UsersController INHERITS MVC.HttpController
    PUBLIC SUB GetAll()
        SELF.Response.SendJSON(utenti)   ' usa i membri ereditati dalla base
    END SUB
END CLASS


## 5. Polimorfismo (`VIRTUAL`, `OVERRIDE`, `ABSTRACT`)

### 5.1 `VIRTUAL` e `OVERRIDE`

CLASS Animale
    PUBLIC VIRTUAL SUB Parla()
        PRINT "..."
    END SUB
END CLASS

CLASS Cane INHERITS Animale
    PUBLIC OVERRIDE SUB Parla()
        PRINT "Bau!"
    END SUB
END CLASS


SUB Main()
    DIM a AS Animale = NEW Cane()
    a.Parla()          ' stampa "Bau!" (OVERRIDE di Cane), non "..."
END SUB


### 5.2 Upcast nel RETURN (fix Z1)

FUNCTION CreaAnimale() AS Animale
    CreaAnimale = NEW Cane()        ' upcast derivata→base nel RETURN
END FUNCTION

SUB Main()
    DIM a AS Animale = CreaAnimale()
    a.Parla()                       ' "Bau!" — dispatch dinamico sull'OVERRIDE
END SUB


### 5.3 Classi e metodi astratti (`ABSTRACT`)

PUBLIC ABSTRACT CLASS Figura
    PUBLIC ABSTRACT FUNCTION CalcolaArea() AS DOUBLE
    PUBLIC ABSTRACT SUB Disegna()
END CLASS

PUBLIC CLASS Cerchio INHERITS Figura
    PUBLIC DIM Raggio AS DOUBLE
    PUBLIC SUB NEW(r AS DOUBLE)
        SELF.Raggio = r
    END SUB
    PUBLIC OVERRIDE FUNCTION CalcolaArea() AS DOUBLE
        FUNCTION = MATH.PI * SELF.Raggio ^ 2
    END FUNCTION
    PUBLIC OVERRIDE SUB Disegna()
        PRINT "cerchio"
    END SUB


## 6. Interfacce (`INTERFACE` / `IMPLEMENTS`)

PUBLIC INTERFACE ISerializzabile
    PUBLIC FUNCTION ToJson() AS STRING
END INTERFACE

PUBLIC CLASS Utente IMPLEMENTS ISerializzabile
    PUBLIC DIM Nome AS STRING

    PUBLIC FUNCTION ToJson() AS STRING
        FUNCTION = "{ \"nome\": \"" + SELF.Nome + "\" }"
    END FUNCTION
END CLASS


## 7. Membri statici (`STATIC`)

CLASS Contatore
    PUBLIC STATIC DIM _totale AS LONG = 0

    PUBLIC SUB Incrementa()
        SELF._totale = SELF._totale + 1
    END SUB

    PUBLIC FUNCTION Totale() AS LONG
        FUNCTION = SELF._totale
    END FUNCTION
END CLASS

DIM a AS Contatore = NEW Contatore()
DIM b AS Contatore = NEW Contatore()
a.Incrementa()
a.Incrementa()


## 8. Classi in namespace e moduli

' --- modulo MvcResults.inc ---
NAMESPACE MVC

PUBLIC CLASS IActionResult
    PUBLIC VIRTUAL FUNCTION GetStatusCode() AS LONG
        FUNCTION = 0
    END FUNCTION
END CLASS

PUBLIC CLASS JsonResult INHERITS IActionResult
    PUBLIC DIM StatusCode AS LONG
    PUBLIC SUB NEW()
        SELF.StatusCode = 200
    END SUB
    PUBLIC OVERRIDE FUNCTION GetStatusCode() AS LONG
        FUNCTION = SELF.StatusCode


' --- programma cliente ---
IMPORT MVC

FUNCTION Json() AS MVC.IActionResult
    Json = NEW MVC.JsonResult()
END FUNCTION

SUB Main()
    DIM r AS MVC.IActionResult = Json()
    PRINT r.GetStatusCode()     ' 200 (OVERRIDE, dispatch dinamico)
END SUB


## 9. Guida alla scelta: `TYPE` o `CLASS`?

## 10. Buone pratiche

## 11. Limiti e gotcha noti

## 12. Riferimento rapido (sintassi)

CLASS NomeClasse                 ' definizione
END CLASS

[PUBLIC|PRIVATE|PROTECTED|FRIEND] DIM campo AS Tipo [= default]
[PUBLIC|PRIVATE|PROTECTED|FRIEND] DIM nome(N) AS Tipo   ' ARRAY-membro (N=limite sup.; accesso nome(i)/SELF.nome(i))
[PUBLIC|PRIVATE|PROTECTED|FRIEND] DIM nome(N,M) AS Tipo ' ARRAY-membro MULTI-DIM (accesso nome(i,j)/SELF.nome(i,j))
DIM nome(N) AS Tipo                                     ' array di UDT GLOBALE (fuori da classe)
DIM nome(N,M) AS Tipo                                   ' array globale multi-dim
nome(i)  /  SELF.nome(i)                                ' elemento (nudo o qualificato)
nome(i).campo  /  SELF.nome(i).campo                    ' campo di un elemento UDT
nome(i,j)  /  SELF.nome(i,j)                            ' elemento 2-D
nome(i,j).campo  /  SELF.nome(i,j).campo                ' campo di un elemento UDT 2-D
campo.campo2  /  SELF.campo.campo2                      ' campo UDT ANNIDATO (a qualunque livello)
nome AS STRING * N                                      ' stringa a lunghezza fissa (N caratteri, pad a N)
[PUBLIC] SUB NEW(...)            ' costruttore (con o senza parametri)
[PUBLIC] SUB DESTRUCTOR          ' distruttore (SENZA parentesi)


## 13. Esempio completo: gestione biblioteca

IMPORT MATH

' --- Contratto: un oggetto stampabile ---
PUBLIC INTERFACE IStampa
    PUBLIC FUNCTION Descrizione() AS STRING
END INTERFACE

' --- Classe base ---
PUBLIC ABSTRACT CLASS Risorsa IMPLEMENTS IStampa
    PRIVATE DIM _titolo AS STRING

    PUBLIC SUB NEW(titolo AS STRING)
        SELF._titolo = titolo
    END SUB

    PUBLIC PROPERTY Titolo AS STRING


Libro 'Il Nome della Rosa' (512 pag.)
Film 'Interstellar' (169 min)


### Class-Static-Fields-Guida-Utente

*Fonte: `Class-Static-Fields-Guida-Utente`*


## Panoramica

## Sintassi

PUBLIC CLASS <Nome>
    PUBLIC STATIC DIM <Campo> AS <Tipo> [= <valore iniziale>]
    ...
END CLASS


### Esempio — contatore di istanze vive

PUBLIC CLASS Risorsa
    PUBLIC DIM Nome AS STRING
    PUBLIC STATIC DIM Aperte AS LONG = 0

    PUBLIC SUB NEW(nome AS STRING)
        SELF.Nome = nome
        Risorsa.Aperte = Risorsa.Aperte + 1      ' accesso allo STATIC: NomeClasse.Campo
    END SUB

    PUBLIC SUB DESTRUCTOR
        Risorsa.Aperte = Risorsa.Aperte - 1
    END SUB
END CLASS

DIM r = NEW Risorsa("db")
PRINT Risorsa.Aperte   ' 1 — condiviso, non un campo dell'istanza r


## Come funziona (per backend)

## Parità Backend
| Backend | Lettura `Cls.Campo` | Scrittura `Cls.Campo = ...` | Condivisione tra istanze | Parità |
|---------|:-:|:-:|:-:|:-:|
| IL | ✓ | ✓ | ✓ | 5/5 |
| PCodeVM | ✓ | ✓ | ✓ | 5/5 |
| VM/C | ✓ | ✓ | ✓ | 5/5 |
| C99Native | ✓ | ✓ | ✓ | 5/5 |

## Storia (bug risolto)

## MiniTest

tools/sh/test_minitests_parity.sh constructs/types_class_ctor   # IL/PCode/VM-C (xUnit)
# [storico] gate C99Native rimosso il 2026-10-25 — oggi: tools/sh/test_minitests_c99native
GMB_MINI_ONLY="constructs/types_class_ctor" tools/sh/test_minitests_c99native


## Sample

## Riferimenti

<!-- END SURFACE -->
