' FILE: Clipper/ClipperReport.bas

IMPORT DBF
IMPORT IO
IMPORT CONSOLE

' =========================================================================================
' ESEMPIO DIMOSTRATIVO: DBF REPORT FORM
' =========================================================================================
' Questo programma illustra come utilizzare il comando `DBF REPORT FORM` per generare
' report testuali complessi basati su un file di definizione esterno (.frx).
' =========================================================================================

' Definizione della struttura dati per i clienti
TYPE Cliente
    ID      AS LONG
    Nome    AS STRING * 30
    Citta   AS STRING * 20
    Regione AS STRING * 15
    Saldo   AS CURRENCY
    Attivo  AS BIT
END TYPE

' Setup: Crea e popola un DBF di test
SUB SetupReportData()
    PRINT "Creazione dati di test per il report..."
    
    ' Rimuove file precedenti
    IF IO.FILEEXISTS("clienti.dbf") THEN KILL "clienti.dbf"
    IF IO.FILEEXISTS("clienti.ntx") THEN KILL "clienti.ntx"

    ' Crea il DBF dalla struttura del TIPO
    DBF CREATE "clienti.dbf" FROM TYPE Cliente
    
    DBF USE "clienti.dbf" ALIAS Clienti EXCLUSIVE NEW

    ' Aggiunge dati di esempio
    DBF APPEND BLANK : DBF REPLACE ID WITH 1, Nome WITH "Mario Rossi", Citta WITH "Milano", Regione WITH "Nord", Saldo WITH 150.50, Attivo WITH TRUE
    DBF APPEND BLANK : DBF REPLACE ID WITH 2, Nome WITH "Giuseppe Verdi", Citta WITH "Roma", Regione WITH "Centro", Saldo WITH -25.00, Attivo WITH TRUE
    DBF APPEND BLANK : DBF REPLACE ID WITH 3, Nome WITH "Paolo Bianchi", Citta WITH "Torino", Regione WITH "Nord", Saldo WITH 1200.00, Attivo WITH TRUE
    DBF APPEND BLANK : DBF REPLACE ID WITH 4, Nome WITH "Luca Neri", Citta WITH "Napoli", Regione WITH "Sud", Saldo WITH 0.00, Attivo WITH FALSE
    DBF APPEND BLANK : DBF REPLACE ID WITH 5, Nome WITH "Franco Gialli", Citta WITH "Firenze", Regione WITH "Centro", Saldo WITH 75.20, Attivo WITH TRUE

    PRINT "Dati originali nel DBF:"
    DBF LIST

    ' Il report è raggruppato per Regione, quindi indicizziamo per quel campo
    DBF INDEX ON Regione TO "clienti.ntx"
    DBF SET ORDER TO 1
END SUB

' =========================================================================================
'                                           MAIN
' =========================================================================================
SUB Main()
    SetupReportData()
    
    PRINT ""
    PRINT "------------------------------------------------------------------"
    PRINT "Esempio 1: Report completo su file 'report_completo.txt' (Sintassi Legacy)"
    PRINT "------------------------------------------------------------------"
    WAITKEY
    
    ' Usa la sintassi legacy per generare il report
    DBF REPORT FORM "ElencoClienti.frx" TO FILE "report_completo.txt"
    
    PRINT "File 'report_completo.txt' generato. Controllare il contenuto."
    PRINT ""
    
    PRINT "------------------------------------------------------------------"
    PRINT "Esempio 2: Report filtrato su console/stampante"
    PRINT "         (Solo clienti attivi della regione 'Centro')"
    PRINT "------------------------------------------------------------------"
    WAITKEY

    ' Genera il report filtrato
    DBF REPORT FORM "ElencoClienti.frx" FOR Attivo AND Regione = "Centro" TO PRINT
    
    ' Pulizia finale
    DBF CLOSE DATABASES
    PRINT ""
    PRINT "Test del report completato."
END SUB
