' FILE: ParserStressTest.bas
' ======================================================================================================================
' GUIDA TEORICA: STRESS TEST DEL PARSER DI FASE 1 (AST PURO)
' ======================================================================================================================
' Questo file funge da banco di prova esaustivo per la "Fase 1" del compilatore GM-BASIC, ovvero il Parsing Puro.
' In questa fase, il compilatore deve trasformare il flusso di token in un Abstract Syntax Tree (AST) sintatticamente
' corretto senza avere accesso alla Symbol Table completa (che viene costruita successivamente).
'
' OBIETTIVI DEL TEST:
' 1.  GESTIONE SINTASSI MISTA (LEGACY vs MODERN):
'     Il parser deve gestire correttamente la coesistenza di codice "spaghetti" (numeri di riga, GOTO, GOSUB,
'     continuazioni di riga con `_`, istruzioni multiple con `:`) e codice strutturato moderno (`SUB`, `FUNCTION`,
'     blocchi `TRY/CATCH`, `SELECT CASE`).
'
' 2.  DISAMBIGUAZIONE SINTATTICA (LOOKAHEAD):
'     Molti costrutti in BASIC sono ambigui senza contesto. Il parser deve usare il lookahead per distinguere:
'     - `Nome(...)`: Chiamata di funzione vs Accesso ad array. In Fase 1, se non è una funzione built-in nota,
'       viene generato un nodo ponte (`ArrayAccessExpr`) che il Linker risolverà successivamente.
'     - `Identificatore`: Inizio di assegnazione (`LET` implicito) vs Chiamata a `SUB`.
'     - `DBF USE`: Comando specifico del namespace DBF vs sintassi generica.
'
' 3.  ESPRESSIONI COMPLESSE E CHAINING:
'     Verifica il parsing di espressioni con operatori di accesso a catena (`.`, `[]`) su oggetti, dizionari e UDT.
'     Esempio: `myDict["key"].property[index]`.
'
' 4.  PRE-PROCESSOR:
'     Verifica che le direttive `#DEF`, `#IFDEF`, `#ELSE`, `#ENDIF` vengano gestite correttamente prima o durante
'     il parsing, includendo o escludendo blocchi di codice.
' ======================================================================================================================

' --- Namespace richiesti per testare tutti i tipi di comandi ---
IMPORT DBF
IMPORT ISAM
IMPORT SPEED
IMPORT LEDGER
IMPORT HTTP
IMPORT NET
IMPORT JSON
IMPORT COLLECTIONS
IMPORT STRINGS

' --- Direttive di Pre-Processore ---
#DEF PRODUCTION
#DEF USER_LEVEL = "ADMIN"

' ======================================================================
' Sezione 1: Codice con Numeri di Riga e Sintassi Complessa
' ======================================================================

10 REM Inizio del codice numerato
' Test di continuazione di riga esplicita (_) con commenti interposti.
' Il parser deve unire queste righe fisiche in un'unica istruzione logica.
20 DIM A = 10 + _ ' Continuazione di riga esplicita
    20 ' Commento a fine riga di continuazione
    : REM Continuazione con colon e commento REM

30 ' Combinazione di _ (continuazione) e : (separatore di istruzioni)
40 PRINT "Valore="; _
    A : PRINT " (calcolato)"

50 GOTO 100

' Questo commento /* deve essere ignorato come blocco se supportato, o come commento linea */
' Test di compilazione condizionale
#IFDEF PRODUCTION
    70 PRINT "Modalità Produzione Attiva"
#ELSE
    80 PRINT "Questo non deve apparire"
#ENDIF

100 PRINT "Saltato alla linea 100."
110 GOSUB 1000 ' Chiama una subroutine numerata (Legacy GOSUB)

' ======================================================================
' Sezione 2: SUB MAIN e Sintassi legacy
' ======================================================================

SUB Main()
    ' --- Test Stringhe e Commenti ---
    ' Raw String: permette di includere caratteri speciali senza escape.
    DIM RAW_STRING$ = `
        Questa è una raw string.
        Contiene un : e un _ che non devono essere interpretati.
        Anche un commento ' come questo.
        E una direttiva: #IFDEF FAKE
    `
    PRINT RAW_STRING$

    ' --- Test Assegnazioni e Chiamate Miste ---
    
    ' Assegnazione implicita a funzione non qualificata (da namespace di default MATH)
    x = SIN(0.5)
    
    ' Chiamata a comando legacy che richiede il prefisso del namespace (es. DBF)
    DBF USE "./Fixtures/Dbf/working/dbase3/dbase_03.dbf" ALIAS TestDBF SHARED
    
    ' Chiamata a comando legacy a spazio
    DBF SELECT 1
    
    ' Chiamata a funzione qualificata che restituisce un oggetto
    DIM myJson AS JSON = JSON.Parse("{}")
    
    ' Chiamata a metodo su oggetto (LIST)
    DIM myList AS LIST<STRING>
    myList.Add("test")

    ' --- Test Chaining Complesso ---
    ' Accesso a proprietà dinamica di JSON e poi indicizzazione array
    myJson.data = [10, 20, 30]
    PRINT "Accesso a catena: "; myJson.data[0] ' Accesso con []
    
    ' Definizione UDT locale
    TYPE MyUdt
        items(5) AS STRING * 10
    END TYPE
    DIM u AS MyUdt
    REDIM u.items(1)
    u.items(0) = "valore"
    
    ' Chiamata a catena complessa: oggetto.array(indice)
    ' Il parser deve distinguere questo da una chiamata a funzione
    PRINT "Accesso UDT a catena: "; u.items(0)

    ' Chiamata a catena: funzione.metodo()
    REM DIM keys = myJson.Keys()
    
    ' --- Test Chaining Complesso su Dizionari ---
    DIM myDict AS DICT<STRING, JSON>
    ' Parsing di JSON inline e assegnazione a chiave di dizionario
    myDict["data"] = JSON.Parse("[10, 20, 30]")
    PRINT "Accesso a catena: "; myDict["data"][0] ' Accesso con []
    
    ' Accesso nidificato: Dizionario -> Oggetto JSON
    myDict["user"] = JSON.Parse(`"Mario"`)
    DIM keys = myDict.Keys()
    
    ' --- Test Sintassi Espressioni ---
    
    ' Letterale negativo e precedenza degli operatori
    DIM result = -5 * (10 + 2)
    PRINT "Risultato espressione: "; result
    
    ' Funzione senza argomenti (parentesi vuote)
    DIM r = RND()
    
    ' --- Controllo di Flusso Misto ---
    ' IF a riga singola con GOTO e GOSUB
    IF A > 0 THEN 200 ELSE GOSUB 1100
    
    ' --- VERIFICA DI COMPLETEZZA: BLOCCHI STRUTTURATI ---
    ' Aggiunti test per blocchi SELECT e TRY/CATCH per verificare il parsing
    ' dei corpi dei blocchi all'interno di una SUB.
    
    PRINT "Test Blocco SELECT CASE..."
    SELECT CASE result
        CASE IS < 0
            PRINT "Risultato negativo"
        CASE 0
            PRINT "Risultato nullo"
        CASE ELSE
            PRINT "Risultato positivo"
    END SELECT

    PRINT "Test Blocco TRY/CATCH..."
    TRY
        ' Genera un errore deliberato per testare il catch
        DIM zero = 0
        DIM errorTest = 10 / zero
    CATCH ex
        PRINT "Errore catturato correttamente: "; ex.Message
    END TRY
    
    PRINT "Fine di SUB Main."
END SUB


' ======================================================================
' Sezione 3: Altre Routine Numerate
' ======================================================================

200 PRINT "Saltato alla linea 200."
210 GOTO 9999 ' Fine programma

1000 REM Subroutine GOSUB per linea 110
1010 PRINT "  -> Dentro GOSUB 1000"
1020 RETURN

1100 REM Subroutine GOSUB per linea di IF
1110 PRINT "  -> Dentro GOSUB 1100"
1120 RETURN

9999 END