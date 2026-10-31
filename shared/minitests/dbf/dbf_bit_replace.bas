' MiniTest DBF — REPLACE su campo BIT (TRUE → 'T', FALSE → 'F') e filtro Attivo = TRUE.
' Path ASSOLUTI (/tmp): la CWD differisce fra i backend. Il MiniTest è il proprio oracolo.
' EXPECT: t=-1 f= 0 n= 1
IMPORT DBF
IMPORT IO

TYPE TBit
    ID AS INTEGER
    Attivo AS BIT
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_bit_replace.dbf"
    KILL p
    DBF CREATE p FROM TYPE TBit
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND BLANK
    DBF REPLACE ID WITH 1, Attivo WITH TRUE
    DBF APPEND BLANK
    DBF REPLACE ID WITH 2, Attivo WITH FALSE
    DBF GO TOP
    DIM tVal AS INTEGER
    DIM fVal AS INTEGER
    tVal = Attivo
    DBF SKIP
    fVal = Attivo
    DBF SET FILTER TO Attivo = TRUE
    DBF GO TOP
    DIM n AS INTEGER
    n = 0
    DO WHILE NOT DBF.EOF()
        n = n + 1
        DBF SKIP
    LOOP
    PRINT "t="; STR(tVal); " f="; STR(fVal); " n="; STR(n)
    DBF CLOSE DATABASES
END SUB
