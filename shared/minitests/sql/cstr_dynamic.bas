' MiniTest — CONVERSIONS.CSTR di un campo dinamico SQL (rs("col") → gmb_val_t)
' EXPECT: id= 1|nome=a|tot= 1.5|str=[ 1]
IMPORT SQL
IMPORT CONVERSIONS
SUB Main()
    SQL OPEN ":memory:" ALIAS st
    SQL EXEC st, "CREATE TABLE t(id INTEGER, nome TEXT, prezzo REAL)"
    SQL EXEC st, "INSERT INTO t VALUES(1,'a',1.5)"
    SQL EXEC st, "INSERT INTO t VALUES(2,'b',1.0)"
    DIM rs AS SQLRESULTSET
    SQL QUERY st, rs, "SELECT id, nome, SUM(prezzo) AS tot FROM t WHERE id = 1"
    DO WHILE SQL.FETCH(rs)
        PRINT "id="; CONVERSIONS.CSTR(rs("id")); "|nome="; CONVERSIONS.CSTR(rs("nome")); "|tot="; CONVERSIONS.CSTR(rs("tot")); "|str=["; STR$(rs("id")); "]"
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE st
END SUB
