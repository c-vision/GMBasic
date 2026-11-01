' MiniTest TYPES — il suffisso tipo ($ % ! #) è un'annotazione, NON fa parte del nome.
'
' `S` e `S$` sono la STESSA variabile: una scrittura con un suffisso della STESSA famiglia
' del tipo dichiarato è legittima (`DIM s AS STRING` + `s$ = ...`, `DIM n AS INTEGER` +
' `n% = ...`, anche `DIM x AS LONG` + `x%` perché entrambi numerici). Una scrittura con un
' suffisso di famiglia DIVERSA è un errore (vedi parser/suffix_type_conflict.bas).
'
' EXPECT: s=a s$=a|n= 42  n%= 42|x= 7  x%= 7|t=ok t$=ok
DIM s AS STRING
s$ = "a"
PRINT "s="; s; " s$="; s$

DIM n AS INTEGER
n% = 42
PRINT "n="; n; " n%="; n%

' suffisso INTEGER su una variabile LONG: stessa famiglia (numerico) → permesso
DIM x AS LONG
x% = 7
PRINT "x="; x; " x%="; x%

DIM t AS STRING
t = "ok"
PRINT "t="; t; " t$="; t
