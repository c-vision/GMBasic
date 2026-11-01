' Suffisso tipo su una SCRITTURA che contraddice il tipo dichiarato della variabile.
' `S` e `S$` sono la STESSA variabile (il suffisso non fa parte del nome, è un'annotazione
' di tipo): scrivere con `S$` su una variabile SINGLE sarebbe ambiguo. Decisione 2026-11-01:
' è un ERRORE esplicito, con lo stesso testo su tutte e 4 le gambe (IL, PCode, VM-C, C99Native).
' EXPECT: ERROR: La variabile 'S' e' di tipo numerico, ma il nome 'S$' ha il suffisso di tipo STRING: sono la STESSA variabile (il suffisso non fa parte del nome), quindi la scrittura e' ambigua. Usare un nome distinto o togliere il suffisso.
DIM S AS SINGLE
S$ = "abc"
PRINT S
