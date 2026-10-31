' FILE: MyLibrary.inc
' ======================================================================================================================
' GUIDA TEORICA: CREAZIONE DI LIBRERIE RIUTILIZZABILI CON I NAMESPACE
' ======================================================================================================================
' Questo file (`.inc` per "include" o `.bas`) funge da libreria, definendo un insieme di tipi e procedure
' raggruppati logicamente in "namespace". Non contiene un punto di ingresso (`SUB Main`), poiché è pensato
' per essere utilizzato da altri programmi tramite la direttiva `IMPORT`.
'
' 1. NAMESPACE `Geometria`
'    - Raggruppa entità correlate alla geometria 2D.
'    - `PUBLIC TYPE Punto`: Definisce un Tipo Definito dall'Utente (UDT) per rappresentare una coordinata.
'      La keyword `PUBLIC` lo rende visibile e utilizzabile nei moduli che importano `Geometria`.
'    - `PUBLIC FUNCTION Distanza(...)`: Una funzione che opera su tipi `Punto`. Anch'essa è `PUBLIC`
'      per essere accessibile dall'esterno.
'
' 2. NAMESPACE `Calcoli`
'    - Raggruppa costanti e funzioni matematiche di utilità generale.
'    - `PUBLIC CONST PIGRECO`: Definisce una costante pubblica.
'    - `PUBLIC FUNCTION Somma(...)`: Una semplice funzione di addizione.
'    - `PUBLIC SUB MostraRisultato(...)`: Una subroutine che non restituisce un valore, usata per l'output.
'
' 3. PRINCIPIO DI INCAPSULAMENTO
'    - Se una funzione o un tipo non fosse dichiarato `PUBLIC` (il default è `PRIVATE` a livello di modulo),
'      sarebbe utilizzabile solo all'interno dello stesso namespace e dello stesso file, ma invisibile
'      ai programmi che lo importano.
'    - Questo permette di nascondere i dettagli implementativi e di esporre solo un'API pubblica e stabile.
' ======================================================================================================================

' --- INIZIO DEFINIZIONE LIBRERIA ---

' Definizione del namespace per le utility geometriche
NAMESPACE Geometria

    ' Rende il tipo "Punto" visibile all'esterno del modulo
    PUBLIC TYPE Punto
        X AS DOUBLE
        Y AS DOUBLE
    END TYPE

    ' Rende la funzione "Distanza" visibile all'esterno del modulo
    PUBLIC FUNCTION Distanza(p1 AS Punto, p2 AS Punto) AS DOUBLE
        ' Calcola la differenza sulle coordinate x e y
        DIM dx AS DOUBLE = p2.X - p1.X
        DIM dy AS DOUBLE = p2.Y - p1.Y
        ' Applica il teorema di Pitagora per calcolare la distanza
        FUNCTION = MATH.SQR(dx * dx + dy * dy)
    END FUNCTION

' Definizione del namespace per le utility di calcolo
NAMESPACE Calcoli

    ' Rende la costante "PIGRECO" visibile all'esterno
    PUBLIC CONST PIGRECO = 3.14159

    ' Rende la funzione "Somma" visibile all'esterno
    PUBLIC FUNCTION Somma(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
        FUNCTION = a + b
    END FUNCTION

    ' Rende la subroutine "MostraRisultato" visibile all'esterno
    PUBLIC SUB MostraRisultato(prefisso AS STRING, valore AS DOUBLE)
        PRINT prefisso + ": " + CSTR(valore)
    END SUB
   