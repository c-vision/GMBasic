' MiniTest — conversioni/aritmetica su campo dinamico SQL (rs("col") → gmb_val_t)
' EXPECT: sum=[151]up=[ABC]len=[ 3 ]abs=[ 3 ]int=[-3 ]mid=[b]
IMPORT SQL
SUB Main()
    SQL OPEN ":memory:" ALIAS st
    SQL EXEC st, "CREATE TABLE t(id INTEGER, nome TEXT, prezzo INTEGER)"
    SQL EXEC st, "INSERT INTO t VALUES(-3,'abc',150)"
    DIM rs AS SQLRESULTSET
    SQL QUERY st, rs, "SELECT id, nome, prezzo FROM t"
    DO WHILE SQL.FETCH(rs)
        PRINT "sum=["; FORMAT(rs("prezzo") + 1, "0"); "]up=["; UPPER(rs("nome")); "]len=["; LEN(rs("nome")); "]abs=["; ABS(rs("id")); "]int=["; INT(rs("id")); "]mid=["; MID$(rs("nome"), 2, 1); "]"
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE st
END SUB
