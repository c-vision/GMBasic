' MiniTest NAMESPACE SYSTEM — tutte le 26 funzioni della superficie.
' CHOOSE, COMMAND, COMMANDCOUNT, CTYPE, Count, EMPTY, GOMAXPROCS, IIF, LBOUND,
' TI, TYPENAME, UBOUND, UDTALIGNED, UDTPACKED, VALTYPE, VARPTR.
' NEXT(generator) è omesso: i generatori non sono supportati da IL e VM/C (gap aperto);
' SHELLCODE/SHELLOUT sono fuori da questo file: richiedono SET SHELL ON (opt-in) e la gamba
' PCode va in stallo intermittente sul park della shell (Bugs/(open) - PCode-Shell-Park-Intermittente.md).
' su PCode funziona. Vedi Bugs/(open) - SYSTEM-Next-Generatori.md.
' Scopo: verificare la STESSA versione del namespace sui 4 backend.
' EXPECT: choose=b|cmdcount>=-1|command=[]|ctype_s=-1|ctype_i= 123|count= 2|empty0=-1 |empty1= 0|gomax>=0=-1|iif= 10|lbound= 0 |ubound= 4|numcpu>0=-1|ngor>=-1|sizeof_int= 2|strptr!=0=-1|strti_len= 8|sys_os>0=-1|tab_len= 1|ti>=-1|typename>0=-1|udtaligned= 1 |udtpacked= 2|valtype_n=N|valtype_s=C|varptr!=0=-1
IMPORT SYSTEM
IMPORT CONCURRENCY

TYPE TestUDT
  A AS INTEGER
  B AS DOUBLE
END TYPE

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
  PRINT "gomax>=0="; (GOMAXPROCS(4) >= 0)   ' solo forma con argomento: la 0-argomenti e la semantica sono divergenti (Bugs)
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
END SUB
