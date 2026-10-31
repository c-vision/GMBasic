' Regressione: SQL.EXEC(...) forma FUNZIONE deve restituire i changes (double) anche con
' `DIM x = SQL.EXEC(...)` SENZA AS e sotto ON ERROR RESUME NEXT. Prima il tipo inferito era
' ANY -> locale object -> valore corrotto (~4.6e18) dalla 2a chiamata.
' EXPECT: r1= 0|r2= 1|r3= 1
IMPORT SQL

SUB Main()
    ON ERROR RESUME NEXT
    SQL OPEN ":memory:" ALIAS C
    DIM r1 = SQL.EXEC("C", "CREATE TABLE p (id INT, v INT)")
    PRINT "r1="; r1
    DIM r2 = SQL.EXEC("C", "INSERT INTO p VALUES (1,10)")
    PRINT "r2="; r2
    DIM r3 = SQL.EXEC("C", "INSERT INTO p VALUES (2,20)")
    PRINT "r3="; r3
    SQL CLOSE C
END SUB
