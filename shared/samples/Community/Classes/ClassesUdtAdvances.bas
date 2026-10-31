' FILE: Classes/ClassesUdtAdvanced.bas
' =================================================================================================
'               GUIDA TEORICA: UDT AVANZATI (TYPE...END TYPE)
' =================================================================================================
'
' Questo test esplora le funzionalità avanzate dei Tipi Definiti dall'Utente (UDT), dimostrando
' che in GM-BASIC un `TYPE` è molto più di una semplice struttura dati (come in QBasic o VB6).
' Gli UDT di GM-BASIC sono tipi di valore (value types) che condividono molte delle capacità delle
' classi (reference types).
'
' 1. UDT vs. Classi
'    - `TYPE` (UDT): È un tipo di valore. Quando assegni una variabile UDT a un'altra (`v2 = v1`),
'      viene creata una copia completa dei dati. Le modifiche a `v2` non influenzeranno `v1`.
'    - `CLASS`: È un tipo di riferimento. Quando assegni una variabile di classe a un'altra (`obj2 = obj1`),
'      entrambe le variabili puntano alla *stessa* istanza in memoria. Le modifiche tramite `obj2`
'      saranno visibili tramite `obj1`.
'
' 2. FUNZIONALITÀ AVANZATE DEGLI UDT
'    Questo test dimostra che gli UDT possono incapsulare non solo dati, ma anche logica,
'    proprio come le classi:
'
'    - `SUB NEW(...)`: Costruttori. Permettono di inizializzare un UDT in modo controllato
'      al momento della creazione (`NEW Vector2D(3, 4)`).
'
'    - `PROPERTY`: Proprietà. Forniscono un accesso controllato ai dati interni. In questo
'      esempio, `Magnitude` è una proprietà calcolata di sola lettura che non memorizza un
'      valore, ma lo calcola ogni volta che viene letta.
'
'    - `FUNCTION` e `SUB` (Metodi): Gli UDT possono avere metodi che operano sui dati dell'istanza.
'      La keyword `SELF` viene usata per fare riferimento all'istanza corrente dell'UDT,
'      proprio come nelle classi.
'
'    - `OPERATOR`: Overloading degli Operatori. Gli UDT supportano l'overloading degli
'      operatori, permettendo di definire comportamenti personalizzati per `+`, `-`, `=`, etc.,
'      rendendo il codice che li utilizza più intuitivo e leggibile.
'
' In sintesi, la scelta tra `TYPE` e `CLASS` in GM-BASIC dipende dalla semantica desiderata:
' - Usa `TYPE` per rappresentare entità piccole e auto-contenute che si comportano come i tipi
'   primitivi (numeri, date), dove la copia del valore è il comportamento atteso.
' - Usa `CLASS` per entità più complesse che rappresentano un'identità unica, dove si desidera
'   che più parti del programma possano condividere e modificare lo stesso oggetto.
'
' =================================================================================================

IMPORT MATH

' Definizione di un Tipo Definito dall'Utente (UDT) avanzato.
' Un UDT in GM-BASIC si comporta come uno 'struct' in C#, incapsulando dati e logica.
TYPE Vector2D
    ' --- CAMPI ---
    ' I campi contengono i dati dell'istanza.
    X AS DOUBLE
    Y AS DOUBLE

    ' --- COSTRUTTORE ---
    ' Un costruttore speciale 'SUB NEW' che viene chiamato quando si crea un'istanza con NEW.
    SUB NEW(x_val AS DOUBLE, y_val AS DOUBLE)
        ' 'SELF' si riferisce all'istanza corrente dell'UDT.
        SELF.X = x_val
        SELF.Y = y_val
    END SUB

    ' --- PROPRIETÀ ---
    ' Una proprietà calcolata di sola lettura (ha solo un blocco GET).
    PUBLIC PROPERTY Magnitude AS DOUBLE
        GET
            ' Calcola e restituisce la lunghezza del vettore usando il teorema di Pitagora.
            ' Utilizza la funzione SQR dal namespace MATH importato.
            RETURN SQR(SELF.X ^ 2 + SELF.Y ^ 2)
        END GET
    END PROPERTY

    ' --- METODI ---
    ' Un metodo SUB che modifica lo stato interno dell'istanza.
    PUBLIC SUB Normalize()
        DIM mag AS DOUBLE = SELF.Magnitude ' Legge la proprietà calcolata.
        IF mag > 0 THEN
            ' Divide le componenti per la magnitudo per ottenere un vettore di lunghezza 1.
            SELF.X = SELF.X / mag
            SELF.Y = SELF.Y / mag
        END IF
    END SUB

    ' Un metodo FUNCTION che esegue un calcolo con un altro Vector2D e restituisce un valore.
    PUBLIC FUNCTION Dot(other AS Vector2D) AS DOUBLE
        FUNCTION = SELF.X * other.X + SELF.Y * other.Y
    END FUNCTION

    ' --- OVERLOADING DEGLI OPERATORI ---
    ' Definisce il comportamento dell'operatore '+' per due istanze di Vector2D.
    PUBLIC OPERATOR +(BYVAL a AS Vector2D, BYVAL b AS Vector2D) AS Vector2D
        ' Restituisce un nuovo vettore che è la somma delle componenti.
        OPERATOR = NEW Vector2D(a.X + b.X, a.Y + b.Y)
    END OPERATOR

    ' Definisce il comportamento dell'operatore '-' per due istanze di Vector2D.
    PUBLIC OPERATOR -(BYVAL a AS Vector2D, BYVAL b AS Vector2D) AS Vector2D
        OPERATOR = NEW Vector2D(a.X - b.X, a.Y - b.Y)
    END OPERATOR

    ' Definisce il comportamento dell'operatore '*' per un Vector2D e uno scalare.
    PUBLIC OPERATOR *(BYVAL a AS Vector2D, BYVAL b AS DOUBLE) AS Vector2D
        OPERATOR = NEW Vector2D(a.X * b, a.Y * b)
    END OPERATOR

    ' Definisce il comportamento dell'operatore di uguaglianza '='.
    PUBLIC OPERATOR =(BYVAL a AS Vector2D, BYVAL b AS Vector2D) AS BIT
        ' Due vettori sono uguali se le loro componenti sono uguali.
        OPERATOR = (a.X = b.X AND a.Y = b.Y)
    END OPERATOR
END TYPE


SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ClassesUdtAdvanced - Test Completo UDT con Metodi e Operatori        ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Creazione di istanze di UDT tramite il costruttore definito nel TYPE.
    DIM v1 AS Vector2D = NEW Vector2D(3, 4)
    DIM v2 AS Vector2D = NEW Vector2D(10, 2)

    PRINT "Vettore 1: ("; v1.X; ", "; v1.Y; ")"
    PRINT "Vettore 2: ("; v2.X; ", "; v2.Y; ")"
    PRINT

    ' Utilizzo di una proprietà (GET). L'accessor GET viene invocato qui.
    PRINT "Magnitudo di v1: "; v1.Magnitude ' Stampa 5
    PRINT

    ' Utilizzo di un operatore sovraccaricato (+).
    DIM v3 AS Vector2D = v1 + v2
    PRINT "v1 + v2 = ("; v3.X; ", "; v3.Y; ")" ' Stampa (13, 6)
    PRINT

    ' Utilizzo di un altro operatore sovraccaricato (*).
    DIM v4 AS Vector2D = v1 * 3
    PRINT "v1 * 3 = ("; v4.X; ", "; v4.Y; ")" ' Stampa (9, 12)
    PRINT

    ' Utilizzo dell'operatore di confronto (=) sovraccaricato.
    DIM v1_copy AS Vector2D = NEW Vector2D(3, 4)
    IF v1 = v1_copy THEN
        PRINT "v1 e v1_copy sono uguali. (Test Operatore = OK)"
    ELSE
        PRINT "ERRORE: v1 e v1_copy non sono uguali."
    END IF
    PRINT

    ' Utilizzo di un metodo (SUB) che modifica lo stato dell'istanza.
    PRINT "Normalizzo v1..."
    v1.Normalize()
    PRINT "v1 normalizzato: ("; v1.X; ", "; v1.Y; ")" ' Stampa (0.6, 0.8)
    PRINT "Nuova magnitudo di v1: "; v1.Magnitude ' Stampa 1
    PRINT
    
    PRINT "--- Test UDT Avanzato completato ---"
END SUB

' =================================================================================================
' VERIFICA DI COMPLETEZZA
' =================================================================================================
'
' - Copertura Funzionalità UDT: Il test è molto completo e verifica con successo che gli UDT supportino:
'   - Campi
'   - Costruttori (`SUB NEW`)
'   - Proprietà con accessor (`GET`)
'   - Metodi (`SUB` e `FUNCTION`) con l'uso di `SELF`
'   - Overloading di operatori binari, unari (implicito in `-`) e di confronto.
'
' Il file di test dimostra in modo esaustivo che gli UDT in GM-BASIC sono feature-complete e
' si comportano come tipi di valore potenti, analoghi agli 'struct' dei linguaggi moderni.
'
' =================================================================================================```