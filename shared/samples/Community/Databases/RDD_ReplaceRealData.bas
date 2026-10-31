' FILE: Databases/RDD_ReplaceRealData.bas
' RDD REPLACE field WITH value [scope] su dati REALI (non un'area ARRAY vuota
' come RDD_ScopeCommands.bas/RDD_Omonime.bas, che non hanno mai un record da
' verificare). Genera un CSV, lo apre via RDD USE (driver CSV, campi/record
' veri), applica REPLACE sul record corrente e su TUTTI i record (scope ALL),
' poi rilegge i valori per confermare che la scrittura sia stata persistita.

IMPORT RDD WITH ALIAS
IMPORT DBF WITH ALIAS
IMPORT IO WITH ALIAS

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    PRINT "=== RDD REPLACE su dati reali (CSV) ==="
    PRINT

    ON ERROR RESUME NEXT
    IF IO.DIREXISTS(g_work) THEN RMDIR g_work, TRUE
    ON ERROR GOTO 0
    MKDIR g_work

    OPEN g_work + "/clienti.csv" FOR OUTPUT AS #1
    PRINT #1, "Nome,Saldo"
    PRINT #1, "Alice,100"
    PRINT #1, "Bob,200"
    PRINT #1, "Carla,300"
    CLOSE #1

    RDD USE g_work + "/clienti.csv" VIA "CSV" ALIAS Clienti

    PRINT "1. REPLACE sul record corrente (Alice -> Alicia)"
    DBF GO TOP
    PRINT "   prima: Nome=["; Nome; "] Saldo=["; Saldo; "]"
    RDD REPLACE Nome WITH "Alicia"
    DBF GO TOP
    PRINT "   dopo:  Nome=["; Nome; "] Saldo=["; Saldo; "]"
    PRINT

    PRINT "2. REPLACE ALL (azzera Saldo per tutti i record)"
    RDD REPLACE Saldo WITH "0" ALL
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        PRINT "   Nome=["; Nome; "] Saldo=["; Saldo; "]"
        DBF SKIP 1
    LOOP
    PRINT

    PRINT "3. REPLACE FOR (solo Bob)"
    RDD REPLACE Saldo WITH "999" FOR Nome = "Bob"
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        PRINT "   Nome=["; Nome; "] Saldo=["; Saldo; "]"
        DBF SKIP 1
    LOOP
    DBF CLOSE

    PRINT
    PRINT "--- RDD REPLACE REAL DATA OK ---"
END SUB
