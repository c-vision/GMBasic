' MiniTest NAMESPACE SQL — tutte le 8 funzioni della superficie.
' CHANGES, COL, DFSCHEMA, EXEC, FETCH, FROMCSV, LASTINSERTROWID, TOCSV (DB :memory:).
' L'alias è passato come STRINGA ("m") nelle funzioni: forma verificata su tutte le gambe.
' I BIT sono normalizzati con <> 0 (la rappresentazione 1/-1 varia fra i backend).
' EXPECT: exec_create>=0=-1|exec_ins1>=0=-1|exec_ins2>=0=-1|changes= 1|lastid= 2|schema_has_name=-1|col=a,b|fetch1=-1|id1= 1|fetch2=-1|id2= 2|fetch3= 0|tocsv>=0=-1|csv_lines= 3|fromcsv>=0=-1|imported= 2
IMPORT SQL
IMPORT IO

SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  SQL OPEN ":memory:" ALIAS m

  PRINT "exec_create>=0="; (SQL.EXEC("m", "CREATE TABLE t(id INTEGER, name TEXT)") >= 0)
  PRINT "exec_ins1>=0="; (SQL.EXEC("m", "INSERT INTO t VALUES(1,'a')") >= 0)
  PRINT "exec_ins2>=0="; (SQL.EXEC("m", "INSERT INTO t VALUES(2,'b')") >= 0)
  PRINT "changes="; SQL.CHANGES("m")
  PRINT "lastid="; SQL.LASTINSERTROWID("m")

  DIM rs AS SQLRESULTSET
  SQL QUERY m, rs, "SELECT id, name FROM t ORDER BY id"
  PRINT "schema_has_name="; (INSTR(SQL.DFSCHEMA(rs), "name") > 0)
  DIM names() = SQL.COL(rs, "name")
  PRINT "col="; names(0); ","; names(1)
  PRINT "fetch1="; (SQL.FETCH(rs) <> 0)
  PRINT "id1="; rs("id")
  PRINT "fetch2="; (SQL.FETCH(rs) <> 0)
  PRINT "id2="; rs("id")
  PRINT "fetch3="; (SQL.FETCH(rs) <> 0)
  PRINT "tocsv>=0="; (SQL.TOCSV(rs, "work/sql_all.csv") >= 0)
  SQL CLOSE RESULTSET rs
  DIM lines = IO.READLINES("work/sql_all.csv")
  PRINT "csv_lines="; UBOUND(lines) + 1
  KILL "work/sql_all.csv"

  DIM h AS LONG
  h = IO.FCREATE("work/sql_all_in.csv")
  IO.FWRITE(h, "id,name" + CHR(10) + "1,x" + CHR(10) + "2,y" + CHR(10))
  IO.FCLOSE(h)
  PRINT "fromcsv>=0="; (SQL.FROMCSV("m", "imp", "work/sql_all_in.csv") >= 0)
  DIM rs2 AS SQLRESULTSET
  SQL QUERY m, rs2, "SELECT COUNT(*) AS n FROM imp"
  IF SQL.FETCH(rs2) <> 0 THEN PRINT "imported="; rs2("n")
  SQL CLOSE RESULTSET rs2
  KILL "work/sql_all_in.csv"

  SQL CLOSE m
END SUB
