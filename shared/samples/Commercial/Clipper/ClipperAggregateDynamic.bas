' FILE: Clipper/ClipperAggregateDynamic.bas

IMPORT DBF
IMPORT CONSOLE
IMPORT IO

' =========================================================================================
' ESEMPIO DIMOSTRATIVO: DBF AGGREGATE con SCOPE DINAMICO
' =========================================================================================
' Questo programma è una variante di ClipperAggregate.bas.
' La differenza chiave è che le operazioni di popolamento del database (DBF REPLACE)
' sono state spostate in una procedura separata, `AddSaleRecord`.
'
' OBIETTIVO DEL TEST:
' Verificare che la work area "Sales", aperta in `SetupData` con `DBF USE`, sia
' implicitamente disponibile all'interno di `AddSaleRecord` senza essere passata
' come parametro. Questo dimostra il corretto funzionamento dello scope dinamico
' per la gestione delle work area, un comportamento fondamentale di Clipper/xBase.
' Se `AddSaleRecord` riesce a eseguire `DBF REPLACE` sulla work area "Sales",
' il test ha successo.
' =========================================================================================

' Definizione della struttura dati
TYPE Sale
    Region  AS STRING * 10
    Product AS STRING * 10
    Amount  AS CURRENCY
END TYPE

' NUOVO HELPER: Aggiunge un record alla work area ATTIVA
' Nota: questa SUB non sa quale sia la work area attiva.
' Si affida allo scope dinamico per risolvere i campi.
SUB AddSaleRecord(r AS STRING, p AS STRING, a AS CURRENCY)
    DBF APPEND BLANK
    DBF REPLACE Region WITH r, Product WITH p, Amount WITH a
END SUB

' Setup: Creazione e popolamento di un DBF di test
SUB SetupData()
    PRINT "Creazione dati di test (versione con scope dinamico)..."
    
    ' Rimuove file precedenti se esistono
    IF IO.FILEEXISTS("sales_dyn.dbf") THEN KILL "sales_dyn.dbf"
    IF IO.FILEEXISTS("sales_dyn.ntx") THEN KILL "sales_dyn.ntx"
    IF IO.FILEEXISTS("stats_leg_dyn.dbf") THEN KILL "stats_leg_dyn.dbf"
    IF IO.FILEEXISTS("stats_mod_dyn.dbf") THEN KILL "stats_mod_dyn.dbf"

    ' Crea il DBF
    DBF CREATE "sales_dyn.dbf" FROM TYPE Sale
    
    ' APRE LA WORK AREA 'SALES'. Da questo momento, "Sales" diventa
    ' la work area attiva nello scope dinamico.
    DBF USE "sales_dyn.dbf" ALIAS Sales EXCLUSIVE NEW

    ' Popola i dati chiamando la SUB helper.
    ' La SUB AddSaleRecord opererà implicitamente sulla work area "Sales".
    AddSaleRecord("Nord", "Mouse", 10.00)
    AddSaleRecord("Nord", "Keyboard", 20.00)
    AddSaleRecord("Nord", "Mouse", 12.00)
    AddSaleRecord("Sud", "Mouse", 11.00)
    AddSaleRecord("Sud", "Screen", 150.00)
    AddSaleRecord("Sud", "Keyboard", 25.00)
    AddSaleRecord("Nord", "Screen", 200.00)
    AddSaleRecord("Sud", "Mouse", 9.00)

    PRINT "Dati originali:"
    DBF LIST FIELDS Region, Product, Amount
    
    PRINT "Indicizzazione per Regione..."
    DBF INDEX ON Region TO "sales_dyn.ntx"
    DBF SET ORDER TO 1
END SUB

' =========================================================================================
' CASO 1: SINTASSI LEGACY (DBF AGGREGATE ON ... TO ... COMPUTE ...)
' =========================================================================================
SUB DemoLegacySyntax()
    PRINT ""
    PRINT "--- 1. DEMO SINTASSI LEGACY ---"
    
    DBF SELECT Sales
    DBF GO TOP

    PRINT "Esecuzione aggregazione: Somma, Media e Conteggio per Regione..."
    
    DBF AGGREGATE ON Region TO "stats_leg_dyn.dbf" _
        COMPUTE SUM(Amount) AS TotVendite, _
                AVG(Amount) AS MediaVendite, _
                COUNT(*)    AS NumOrdini, _
                MAX(Amount) AS MaxVendita

    ShowResults("stats_leg_dyn.dbf")
END SUB

' =========================================================================================
' CASO 2: SINTASSI LEGACY (DBF AGGREGATE)
' =========================================================================================
SUB DemoModernSyntax()
    PRINT ""
    PRINT "--- 2. DEMO LEGACY ---"
    
    DBF SELECT Sales
    
    PRINT "Esecuzione aggregazione filtrata (Solo importi > 15.00)..."
    
    VAR aggDefs = [ 
        "SUM(Amount) AS TotHighVal", 
        "MIN(Amount) AS MinHighVal", 
        "COUNT(*)    AS HighValCount" 
    ]

    DBF AGGREGATE ON Region TO "stats_mod_dyn.dbf" FOR Amount > 15.00 COMPUTE SUM(Amount) AS TotHighVal, MIN(Amount) AS MinHighVal, COUNT(*) AS HighValCount

    ShowResults("stats_mod_dyn.dbf")
END SUB

' Helper per visualizzare il contenuto di un DBF generato
SUB ShowResults(fileName AS STRING)
    PRINT "Risultati in '" + fileName + "':"
    DBF USE fileName ALIAS ResultTable NEW
    DBF LIST
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
    PRINT "Test con scope dinamico completato."
END SUB
