' FILE: Strings/StringsPetscii.bas
' ====================================================================================
' PETSCII CONTROL CHARACTERS
' ====================================================================================
' PETSCII$(str$) decodifica i codici di controllo {name} in caratteri PETSCII.
'
' Codici supportati:
'   Screen:  {clr} {home} {up} {down} {left} {right} {del} {inst}
'            {rvson} {rvsoff} {stop} {return}
'   Colors:  {blk} {wht} {red} {cyn} {pur} {grn} {blu} {yel}
'            {orng} {brn} {lblu} {lred} {gry1} {gry2} {lgrn} {lblu2}
'   Fkeys:   {f1}..{f8}
'   Numeric: {N up} {N down} {N left} {N right} {N del} {N inst}
'            es. {5 down} {3 right}
' ====================================================================================

IMPORT STRINGS

PRINT "--- TEST PETSCII$ ---"
PRINT

' 1. Codici di controllo singoli
S1$ = PETSCII$("{clr}Hello{home}")
PRINT "1. CLR+HOME: LEN ="; LEN(S1$); "(Atteso: 7)"
PRINT

' 2. Codici colore
S2$ = PETSCII$("{wht}White{red}Red{grn}Green")
PRINT "2. COLORS: LEN ="; LEN(S2$); "(Atteso: 17)"
PRINT

' 3. Numeric direction
S3$ = PETSCII$("A{3 down}B{2 right}C")
PRINT "3. NUMERIC: LEN ="; LEN(S3$); "(Atteso: 9)"
PRINT

' 4. Codice sconosciuto lasciato invariato
S4$ = PETSCII$("Ciao {unknown} mondo")
PRINT "4. UNKNOWN: '"; S4$; "'"; " (Atteso: '{unknown}')"
PRINT

' 5. Nessun codice
S5$ = PETSCII$("Hello World")
PRINT "5. PLAIN: '"; S5$; "'"; " (Atteso: 'Hello World')"
PRINT

' 6. RVSON/RVSOFF
S6$ = PETSCII$("{rvson}reverse{rvsoff}")
PRINT "6. REVERSE: LEN ="; LEN(S6$); "(Atteso: 9)"
PRINT

PRINT "--- FINE TEST PETSCII$ ---"
END
