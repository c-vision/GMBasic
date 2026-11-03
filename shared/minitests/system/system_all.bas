' MiniTest NAMESPACE SYSTEM — tutte le 26 funzioni della superficie.
' CHOOSE, COMMAND, COMMANDCOUNT, CTYPE, Count, EMPTY, GOMAXPROCS, IIF, LBOUND,
' TI, TYPENAME, UBOUND, UDTALIGNED, UDTPACKED, VALTYPE, VARPTR.
' Include il generatore (SYSTEM.NEXT) e la shell opt-in (SET SHELL ON + SHELLCODE/SHELLOUT).
' Scopo: verificare la STESSA versione del namespace sui 4 backend.
' EXPECT: choose=b|cmdcount>=-1|command=[]|ctype_s=-1|ctype_i= 123|count= 2|empty0=-1 |empty1= 0|iif= 10|lbound= 0 |ubound= 4|numcpu>0=-1|ngor>=-1|sizeof_int= 2|strptr!=0=-1|strti_len= 8|sys_os>0=-1|tab_len= 1|ti>=-1|typename>0=-1|udtaligned= 1 |udtpacked= 2|valtype_n=N|valtype_s=C|varptr!=0=-1|next= 6|gen_inst=10 20|7|30|stop|shellcode= 0|shellout>0=-1|gomax_q>0=-1|gomax_prev>0=-1|gomax_cur= 2|gomax_cur2= 2
IMPORT SYSTEM
IMPORT CONCURRENCY

TYPE TestUDT
  A AS INTEGER
  B AS DOUBLE
END TYPE

GENERATOR RangeGen(n AS LONG) AS LONG
  DIM i AS LONG
  FOR i = 1 TO n
    YIELD i
  NEXT i
END GENERATOR

GENERATOR Multipli(fattore AS LONG, n AS LONG) AS LONG
  DIM i AS LONG
  FOR i = 1 TO n
    YIELD fattore * i
  NEXT i
END GENERATOR

SUB TestGeneratori()
  ' Due istanze indipendenti, stato locale e argomenti multipli.
  DIM a = Multipli(10, 3)
  DIM b = Multipli(7, 1)
  DIM r AS STRING
  r = LTRIM$(STR$(NEXT(a))) + " "
  r = r + LTRIM$(STR$(NEXT(a))) + "|"
  r = r + LTRIM$(STR$(NEXT(b))) + "|"
  r = r + LTRIM$(STR$(NEXT(a))) + "|"
  DIM st AS STRING
  st = "ok"
  TRY
    st = LTRIM$(STR$(NEXT(b)))
  CATCH e AS ERR
    st = "stop"
  END TRY
  PRINT "gen_inst="; r + st
END SUB

SUB Main()
  PRINT "choose="; CHOOSE(2, "a", "b", "c")
  PRINT "cmdcount>="; (COMMANDCOUNT() >= 0)
  PRINT "command=["; COMMAND(999); "]"

  DIM cs AS STRING
  cs = SYSTEM.CTYPE(42, "STRING")
  PRINT "ctype_s="; (LEN(cs) > 0)
  PRINT "ctype_i="; SYSTEM.CTYPE("123", "INTEGER")

  PRINT "count="; SYSTEM.Count("banana", "an")
  PRINT "empty0="; EMPTY(0); "|empty1="; EMPTY(1)
  PRINT "iif="; IIF(1 < 2, 10, 20)

  DIM a(4)
  PRINT "lbound="; LBOUND(a); "|ubound="; UBOUND(a)
  PRINT "numcpu>0="; (NUMCPU() > 0)
  PRINT "ngor>="; (NUMGOROUTINE() >= 0)

  PRINT "sizeof_int="; SIZEOF(INTEGER)

  DIM sv AS STRING
  sv = "x"
  PRINT "strptr!=0="; (STRPTR(sv) <> 0)
  PRINT "strti_len="; LEN(STRTI())
  PRINT "sys_os>0="; (LEN(SYS("OS")) > 0)
  PRINT "tab_len="; LEN(TAB(4))
  PRINT "ti>="; (TI() >= 0)
  PRINT "typename>0="; (LEN(TYPENAME(1.5)) > 0)

  DIM u AS TestUDT
  PRINT "udtaligned="; UDTALIGNED(u); "|udtpacked="; UDTPACKED(u)
  PRINT "valtype_n="; VALTYPE(1); "|valtype_s="; VALTYPE("x")

  DIM v AS INTEGER
  v = 5
  PRINT "varptr!=0="; (VARPTR(v) <> 0)

  DIM it = RangeGen(3)
  DIM total AS LONG = 0
  DIM value AS LONG
  DO
    TRY
      value = NEXT(it)
      total = total + value
    CATCH ex AS ERR
      EXIT DO
    END TRY
  LOOP
  PRINT "next="; total
  TestGeneratori()

  SET SHELL ON
  PRINT "shellcode="; SHELLCODE("true")
  PRINT "shellout>0="; (LEN(SHELLOUT("echo hi")) > 0)

  ' GOMAXPROCS: lettura (0 argomenti), valore PRECEDENTE e valore impostato. Eseguito
  ' DOPO i park (shell/generatore) perché nel runner PCode in-process il
  ' ridimensionamento seguito da un park va in stallo
  ' (Bugs/(open) - GOMAXPROCS-Ridimensionamento-Park.md).
  DIM prev AS LONG
  PRINT "gomax_q>0="; (GOMAXPROCS() > 0)
  prev = GOMAXPROCS(2)
  PRINT "gomax_prev>0="; (prev > 0)
  PRINT "gomax_cur="; GOMAXPROCS()
  PRINT "gomax_cur2="; GOMAXPROCS(0)
END SUB
