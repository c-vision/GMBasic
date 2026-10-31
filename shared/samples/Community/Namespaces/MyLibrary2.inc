' FILE: MyLibrary2.inc
' ======================================================================================================================
' GUIDA TEORICA: ESTENSIONE DEI NAMESPACE E OVERLOADING
' ======================================================================================================================
' Questo file dimostra due concetti potenti del sistema di modularità di GM-BASIC:
'
' 1. ESTENSIONE DI NAMESPACE (PARTIAL NAMESPACES)
'    - GM-BASIC permette di definire lo stesso namespace (`Geometria`, `Calcoli`) in più file diversi.
'    - Durante la compilazione, il Linker "fonde" tutte le definizioni trovate in un unico, grande namespace.
'    - Questo significa che `MyLibrary.inc` e `MyLibrary2.inc` contribuiscono entrambi a definire il contenuto
'      finale dei namespace `Geometria` e `Calcoli`.
'    - Beneficio: Permette di organizzare grandi librerie in file più piccoli e gestibili, ognuno focalizzato
'      su un aspetto specifico, senza perdere l'unità logica del namespace.
'
' 2. OVERLOADING DI FUNZIONI
'    - È possibile definire più funzioni `PUBLIC` con lo stesso nome all'interno dello stesso namespace,
'      a patto che abbiano una "firma" diversa (un numero diverso di parametri o tipi di parametri diversi).
'    - In questo file, viene aggiunto un overload per la funzione `Somma` nel namespace `Calcoli`, che accetta
'      tre argomenti invece dei due della versione originale in `MyLibrary.inc`.
'    - Quando un programma chiama `Somma(...)`, il compilatore (Linker) seleziona automaticamente la versione
'      corretta (l'overload) in base al numero di argomenti forniti nella chiamata.
'
' 3. RISOLUZIONE DEI NOMI TRA NAMESPACE
'    - Una procedura all'interno di un namespace (es. `AreaCerchio` in `Geometria`) può accedere ai simboli
'      pubblici di un altro namespace (es. `PIGRECO` in `Calcoli`) a patto che quest'ultimo sia stato importato
'      nel file (`IMPORT Calcoli WITH ALIAS`) e che l'accesso sia qualificato (`Calcoli.PIGRECO`).
' ======================================================================================================================

' Importa il namespace built-in per funzioni come SLN
IMPORT FINANCE

' --- Estensione del namespace Geometria ---
NAMESPACE Geometria

' Importa il namespace Calcoli con un alias per permettere l'accesso qualificato
IMPORT Calcoli WITH ALIAS

' Aggiunge una nuova funzione pubblica al namespace Geometria.
' Nota: Utilizza un accesso qualificato a Calcoli.PIGRECO per testare
' la risoluzione dei nomi tra namespace definiti dall'utente.
PUBLIC FUNCTION AreaCerchio(raggio AS DOUBLE) AS DOUBLE
    ' Calcola l'area usando la costante PIGRECO dal namespace Calcoli
    FUNCTION = Calcoli.PIGRECO * raggio ^ 2
END FUNCTION

' --- Estensione del namespace Calcoli ---
NAMESPACE Calcoli

' Aggiunge un overload pubblico per la funzione Somma,
' accettando tre parametri. Il compilatore sceglierà questa versione
' se la chiamata ha tre argomenti.
PUBLIC FUNCTION Somma(a AS DOUBLE, b AS DOUBLE, c AS DOUBLE) AS DOUBLE
    FUNCTION = a + b + c
END FUNCTION

' Definizione della funzione di calcolo APR
PUBLIC FUNCTION FNAPR(NUM_PMTS AS DOUBLE, POINTS AS DOUBLE, RATE AS DOUBLE) AS DOUBLE
    ' Parametri:
    ' NUM_PMTS: Numero di pagamenti mensili (es. 360 per 30 anni)
    ' POINTS:   Punti percentuali pagati in anticipo (es. 1 per 1%)
    ' RATE:     Tasso di interesse annuale nominale (es. 10 per 10%)
    
    FACE_AMT = 100 : PMT_FREQ = 12
    POINT_AMT = FACE_AMT * (POINTS / 100)
    NET_PRIN = FACE_AMT - POINT_AMT
    RATE_FACTOR = (RATE / 100) / PMT_FREQ
    
    PMT_AMT = FACE_AMT / (1 / RATE_FACTOR - (1 / (RATE_FACTOR * (1 + RATE_FACTOR) ^ NUM_PMTS)))
    
    ' Calcolo iterativo APR con il metodo di Newton-Raphson
    I = RATE_FACTOR ' Stima iniziale
    I_OLD = 0
    
    WHILE ABS(I - I_OLD) > 0.0000001
      I_OLD = I
      ' Formula del valore attuale (PV)
      PV_CALC = PMT_AMT * (1 - (1 + I) ^ -NUM_PMTS) / I
      ' Funzione f(i) da azzerare
      F_I = PV_CALC - NET_PRIN
      
      ' Derivata della funzione PV: f'(i)
      F_PRIME_I = ( ( NUM_PMTS * PMT_AMT * (1+I)^(-NUM_PMTS-1) * I ) - ( PMT_AMT * (1-(1+I)^-NUM_PMTS) ) ) / (I^2)
      
      ' Prossima iterazione di Newton
      I = I - F_I / F_PRIME_I
    WEND
    
    ' Restituisce l'APR annualizzato
    FNAPR = I * PMT_FREQ * 100
END FUNCTION

' Definizione di una subroutine di test per l'ammortamento
PUBLIC SUB AMMORTAMENTO()
    COSTO = 10000 : RESIDUO = 1000 : VITA = 10
    ' Chiama la funzione built-in SLN (dal namespace FINANCE)
    AMMORTAMENTO_ANNUO = SLN(COSTO, RESIDUO, VITA)
    PRINT "Costo: "; COSTO; " | Residuo: "; RESIDUO; " | Vita: "; VITA; " anni"
    PRINT "Ammortamento annuale (SLN): "; AMMORTAMENTO_ANNUO; " (Atteso: 900)"
END SUB
