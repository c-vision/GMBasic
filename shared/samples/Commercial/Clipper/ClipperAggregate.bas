// FILE: Clipper/ClipperAggregate.bas

IMPORT DBF
IMPORT CONSOLE
IMPORT IO

' =========================================================================================
' ESEMPIO DIMOSTRATIVO: DBF AGGREGATE
' =========================================================================================
' Questo programma illustra l'utilizzo del comando `DBF AGGREGATE`, una potente estensione
' del linguaggio che introduce funzionalità di aggregazione dati simili al `GROUP BY` di SQL.
'
' A differenza del classico `TOTAL ON` (che supporta solo la somma), `AGGREGATE` permette
' di calcolare molteplici statistiche (Conteggio, Media, Minimo, Massimo, Somma) in un
' unico passaggio, generando una nuova tabella DBF con i risultati raggruppati.
'
' SINTASSI LEGACY (Compatibilità xBase estesa):
'   DBF AGGREGATE ON <KeyExpr> TO <FileName> [FOR <Condition>] _
'       COMPUTE <Func>(<Expr>) AS <FieldAlias>, ...
'
'   Esempio:
'   DBF AGGREGATE ON Region TO "stats.dbf" COMPUTE SUM(Amount) AS Tot, COUNT(*) AS Cnt
'
' SINTASSI LEGACY (a spazio):
'   DBF AGGREGATE ON <Key> TO <File> [FOR <Condition>] _
'       COMPUTE <Func>(<Expr>) AS <FieldAlias>, ...
'
'   Esempio:
'   DBF AGGREGATE ON Region TO "stats.dbf" COMPUTE SUM(Amount) AS Tot, COUNT(*) AS Cnt
' =========================================================================================

' 1. Definizione della struttura dati
TYPE Sale
    Region  AS STRING * 10
    Product AS STRING * 10
    Amount  AS CURRENCY
END TYPE

' 2. Setup: Creazione e popolamento di un DBF di test
SUB SetupData()
    PRINT "Creazione dati di test..."
    
    ' Rimuove file precedenti se esistono
    IF IO.FILEEXISTS("sales.dbf") THEN KILL "sales.dbf"
    IF IO.FILEEXISTS("sales.ntx") THEN KILL "sales.ntx"
    IF IO.FILEEXISTS("stats_leg.dbf") THEN KILL "stats_leg.dbf"
    IF IO.FILEEXISTS("stats_mod.dbf") THEN KILL "stats_mod.dbf"

    ' Crea il DBF
    DBF CREATE "sales.dbf" FROM TYPE Sale
    
    DBF USE "sales.dbf" ALIAS Sales EXCLUSIVE NEW

    ' Aggiungiamo dati: Regioni (Nord/Sud), Prodotti, Importi
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Nord", Product WITH "Mouse", Amount WITH 10.00
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Nord", Product WITH "Keyboard", Amount WITH 20.00
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Nord", Product WITH "Mouse", Amount WITH 12.00
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Sud", Product WITH "Mouse", Amount WITH 11.00
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Sud", Product WITH "Screen", Amount WITH 150.00
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Sud", Product WITH "Keyboard", Amount WITH 25.00
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Nord", Product WITH "Screen", Amount WITH 200.00
    DBF APPEND BLANK
    DBF REPLACE Region WITH "Sud", Product WITH "Mouse", Amount WITH 9.00

    PRINT "Dati originali:"
    DBF LIST FIELDS Region, Product, Amount
    
    ' Per aggregare su 'Region', il file deve essere indicizzato o ordinato per quel campo
    PRINT "Indicizzazione per Regione..."
    DBF INDEX ON Region TO "sales.ntx"
    DBF SET ORDER TO 1
END SUB

' =========================================================================================
' CASO 1: SINTASSI LEGACY (DBF AGGREGATE ON ... TO ... COMPUTE ...)
' Simile a TOTAL ON, ma supporta funzioni multiple (MIN, MAX, AVG, COUNT).
' Usa '_' per la continuazione di riga.
' =========================================================================================
SUB DemoLegacySyntax()
    PRINT ""
    PRINT "--- 1. DEMO SINTASSI LEGACY ---"
    
    ' Assicuriamoci di essere sulla tabella giusta
    DBF SELECT Sales
    DBF GO TOP

    ' Sintassi:
    ' DBF AGGREGATE ON <Key> TO <File> [FOR <Scope>] COMPUTE <Func>(<Expr>) AS <Alias>, ...
    
    PRINT "Esecuzione aggregazione: Somma, Media e Conteggio per Regione..."
    
    ' Nota: Uso di underscore per spezzare l'istruzione
    DBF AGGREGATE ON Region TO "stats_leg.dbf" _
        COMPUTE SUM(Amount) AS TotVendite, _
                AVG(Amount) AS MediaVendite, _
                COUNT(*)    AS NumOrdini, _
                MAX(Amount) AS MaxVendita

    ' Mostra Risultati
    ShowResults("stats_leg.dbf")
END SUB

' =========================================================================================
' CASO 2: AGGREGATE (legacy)
' Utilizza argomenti stringa/array per le definizioni, ideale per codice dinamico.
' =========================================================================================
SUB DemoModernSyntax()
    PRINT ""
    PRINT "--- 2. DEMO AGGREGATE ---"
    
    DBF SELECT Sales
    
    ' Sintassi:
    ' DBF AGGREGATE ON key TO destFile [FOR scope] COMPUTE ...
    
    PRINT "Esecuzione aggregazione filtrata (Solo importi > 15.00)..."
    
    ' Nota: Nelle liste letterali [...] i newline sono gestiti automaticamente, 
    ' non serve l'underscore.
    VAR aggDefs = [ 
        "SUM(Amount) AS TotHighVal", 
        "MIN(Amount) AS MinHighVal", 
        "COUNT(*)    AS HighValCount" 
    ]

    DBF AGGREGATE ON Region TO "stats_mod.dbf" FOR Amount > 15.00 COMPUTE SUM(Amount) AS TotHighVal, MIN(Amount) AS MinHighVal, COUNT(*) AS HighValCount

    ' Mostra Risultati
    ShowResults("stats_mod.dbf")
END SUB

' Helper per visualizzare il contenuto di un DBF generato
SUB ShowResults(fileName AS STRING)
    PRINT "Risultati in '" + fileName + "':"
    
    ' Apre il file generato in una nuova area
    DBF USE fileName ALIAS ResultTable NEW
    
    ' Elenca il contenuto. Notare che i campi hanno i nomi definiti negli ALIAS (TotVendite, etc.)
    DBF LIST
    
    ' Chiude la tabella dei risultati
    DBF CLOSE ResultTable
END SUB

' =========================================================================================
' MAIN
' =========================================================================================
SUB Main()
    SetupData()
    
    DemoLegacySyntax()
    DemoModernSyntax()
    
    ' Pulizia finale
    DBF CLOSE DATABASES
    PRINT ""
    PRINT "Test completato."
END SUB