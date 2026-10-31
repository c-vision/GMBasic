' SQL DataFrame (G10) — CSV → tabella → SELECT → DFSCHEMA/COL/TOCSV/FROMCSV.
' Esercita in un unico programma tutta l'ergonomia DataFrame del namespace SQL.
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    SQL OPEN ":memory:" ALIAS D

    ' 1) CSV -> tabella (header = nomi colonne)
    PRINT "fromcsv="; SQL.FROMCSV("D", "prezzi", "../../Fixtures/csv_scan.csv")

    DIM rs AS SQLRESULTSET
    SQL QUERY D, rs, "SELECT id, name, price FROM prezzi ORDER BY id"

    ' 2) schema come JSON
    PRINT "schema="; SQL.DFSCHEMA(rs)

    ' 3) intere colonne come ARRAY
    DIM ids() = SQL.COL(rs, "id")
    DIM nomi() = SQL.COL(rs, "name")
    PRINT "righe="; UBOUND(ids()) + 1
    FOR i = 0 TO UBOUND(ids())
        PRINT ids(i); " -> "; nomi(i)
    NEXT i

    ' 4) esporta il result set e ricaricalo in un'altra tabella
    PRINT "tocsv="; SQL.TOCSV(rs, "work/dataframe.csv")
    PRINT "reload="; SQL.FROMCSV("D", "prezzi2", "work/dataframe.csv")

    SQL CLOSE RESULTSET rs
    SQL CLOSE D
END SUB
