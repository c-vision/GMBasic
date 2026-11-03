' MiniTest STRUCTURE — struttura base del linguaggio (un solo file, nessuna applicazione).
'
' Scopo: verificare che i 4 backend (IL, PCodeVM, VM/C, C99Native) supportino la STESSA
' versione della struttura base: tipi scalari, operatori, controllo di flusso, cicli,
' array, UDT, CLASS, procedure, GOSUB/GOTO, gestione errori e semantica BIT.
' L'oracolo è deterministico e non dipende da ambiente, file o modelli.
'
' Riferimento: Guide/utente (linguaggio base) e i MiniTest per namespace in
' shared/minitests/<ns>/<ns>_all.bas. Qui NON si testano i namespace applicativi.
' EXPECT: types= 7 | 70000 | 2.5 |abc|-1|bit=-1 | 0 | 0|arith= 15 | 1 | 6.25 | 3 |-7|cmp=-1 |-1 |-1 | 0|logic=-1 |-1 | 5|str=abcde| 3 |ab|bc|b|case=ABC|abc| 2|if=B|sel=seven|for= 10|forstep= 22|while= 6|do= 4|exitdo= 3|each= 60 |ub= 2 |lb= 0|mat= 2 | 3|eachstr= 3 |abc|eachlist= 12|udt= 3 | 4 |P1|cls= 41 | 41|proc= 15 | 42 |-1|gosub= 2|try= 42|onerror=ok|bitnum=-1 |-1|True|BIT|bitdouble=-1|bitarg=-1|shadow= 7
SUB Aggiungi(BYREF acc AS INTEGER, v AS INTEGER)
  acc = acc + v
END SUB

FUNCTION Doppio(v AS INTEGER) AS INTEGER
  RETURN v * 2
END FUNCTION

FUNCTION Maggiore(a AS INTEGER, b AS INTEGER) AS BIT
  RETURN a > b
END FUNCTION

TYPE Punto
  X AS INTEGER
  Y AS INTEGER
  Etichetta AS STRING * 8
END TYPE

CLASS Contatore
  PUBLIC DIM N AS INTEGER
  PUBLIC SUB NEW(v AS INTEGER)
    SELF.N = v
  END SUB
  PUBLIC FUNCTION Inc() AS INTEGER
    SELF.N = SELF.N + 1
    RETURN SELF.N
  END FUNCTION
END CLASS

' TRY/CATCH in una procedura separata: l'IL non supporta GOSUB e ON ERROR nella
' stessa procedura (limite noto del backend, non della semantica del linguaggio).
SUB Errori()
  TRY
    ERROR 42, "base"
  CATCH e AS ERR
    PRINT "try="; e.Number
  END TRY
END SUB

SUB StampaDoppio(x AS DOUBLE)
  PRINT "bitarg="; x
END SUB

SUB Recupero()
  ' RESUME NEXT su un errore di ESECUZIONE in una LET (divisione per zero):
  ' il percorso coperto dai guard per-istruzione di C99Native e dal resume-ip
  ' delle gambe gestite. L'errore esplicito ERROR + RESUME NEXT ha un gap noto
  ' su C99Native (vedi Bugs) e vive nei MiniTest del dominio exceptions.
  DIM z AS DOUBLE
  DIM q AS DOUBLE
  ON ERROR RESUME NEXT
  z = 0
  q = 1 / z
  ON ERROR GOTO 0
  PRINT "onerror=ok"
END SUB

SUB Main()
  ' ── tipi scalari e stampa ────────────────────────────────────────────────
  DIM i AS INTEGER
  DIM l AS LONG
  DIM d AS DOUBLE
  DIM s AS STRING
  DIM b AS BIT
  i = 7 : l = 70000 : d = 2.5 : s = "abc" : b = TRUE
  PRINT "types="; i; "|"; l; "|"; d; "|"; s; "|"; b
  PRINT "bit="; (b = TRUE); "|"; (b = FALSE); "|"; NOT b

  ' ── operatori ────────────────────────────────────────────────────────────
  PRINT "arith="; (i * 2 + 1); "|"; (i MOD 3); "|"; (d ^ 2); "|"; (7 \ 2); "|"; (-i)
  PRINT "cmp="; (i > 3); "|"; (i = 7); "|"; (s = "abc"); "|"; (i <> 7)
  PRINT "logic="; (i > 3 AND i < 10); "|"; (i < 3 OR i = 7); "|"; (i XOR 2)
  PRINT "str="; (s + "de"); "|"; LEN(s); "|"; LEFT$(s, 2); "|"; RIGHT$(s, 2); "|"; MID$(s, 2, 1)
  PRINT "case="; UCASE(s); "|"; LCASE("ABC"); "|"; INSTR(s, "b")

  ' ── IF / SELECT CASE ─────────────────────────────────────────────────────
  IF i < 5 THEN
    PRINT "if=A"
  ELSEIF i = 7 THEN
    PRINT "if=B"
  ELSE
    PRINT "if=C"
  END IF

  SELECT CASE i
    CASE 1 TO 5
      PRINT "sel=low"
    CASE 7
      PRINT "sel=seven"
    CASE ELSE
      PRINT "sel=other"
  END SELECT

  ' ── cicli ────────────────────────────────────────────────────────────────
  DIM k AS INTEGER
  DIM sum AS INTEGER
  sum = 0
  FOR k = 1 TO 4
    sum = sum + k
  NEXT k
  PRINT "for="; sum

  sum = 0
  FOR k = 10 TO 1 STEP -3
    sum = sum + k
  NEXT
  PRINT "forstep="; sum

  k = 0 : sum = 0
  WHILE k < 3
    k = k + 1
    sum = sum + k
  WEND
  PRINT "while="; sum

  k = 0
  DO
    k = k + 1
  LOOP UNTIL k = 4
  PRINT "do="; k

  k = 0
  DO WHILE k < 5
    k = k + 1
    IF k = 3 THEN EXIT DO
  LOOP
  PRINT "exitdo="; k

  ' ── array ────────────────────────────────────────────────────────────────
  DIM a(2) AS INTEGER
  a(0) = 10 : a(1) = 20 : a(2) = 30
  sum = 0
  FOR EACH x IN a
    sum = sum + x
  NEXT x
  PRINT "each="; sum; "|ub="; UBOUND(a); "|lb="; LBOUND(a)

  DIM m(1, 1) AS INTEGER
  m(0, 0) = 1 : m(0, 1) = 2 : m(1, 0) = 3 : m(1, 1) = 4
  PRINT "mat="; m(0, 1); "|"; m(1, 0)

  ' FOR EACH su stringa (caratteri) e su lista
  DIM chars AS INTEGER
  DIM txt$ AS STRING
  chars = 0 : txt$ = ""
  FOR EACH ch IN "abc"
    chars = chars + 1
    txt$ = txt$ + ch
  NEXT
  PRINT "eachstr="; chars; "|"; txt$

  DIM lst = [2, 4, 6]
  sum = 0
  FOR EACH n IN lst
    sum = sum + n
  NEXT n
  PRINT "eachlist="; sum

  ' ── UDT ──────────────────────────────────────────────────────────────────
  DIM p AS Punto
  p.X = 3 : p.Y = 4 : p.Etichetta = "P1"
  PRINT "udt="; p.X; "|"; p.Y; "|"; p.Etichetta

  ' ── CLASS ────────────────────────────────────────────────────────────────
  DIM c AS Contatore
  c = NEW Contatore(40)
  PRINT "cls="; c.Inc(); "|"; c.N

  ' ── procedure, BYREF, funzione BIT ───────────────────────────────────────
  DIM tot AS INTEGER
  tot = 10
  Aggiungi(tot, 5)
  PRINT "proc="; tot; "|"; Doppio(21); "|"; Maggiore(5, 3)

  ' ── GOSUB / GOTO ─────────────────────────────────────────────────────────
  k = 0
  GOSUB incrementa
  GOSUB incrementa
  PRINT "gosub="; k
  GOTO fine
incrementa:
  k = k + 1
  RETURN
fine:

  ' ── errori: TRY/CATCH e ON ERROR RESUME NEXT (in procedure separate) ─────
  Errori()
  Recupero()

  ' ── BIT: conversione e testo (parità 4 backend) ──────────────────────────
  b = TRUE
  PRINT "bitnum="; b; "|"; STR$(b); "|"; CSTR(b); "|"; TYPENAME(b)
  d = b
  PRINT "bitdouble="; d
  StampaDoppio(b)

  ' ── ombreggiatura: un array locale vince sul builtin non qualificato ─────
  DIM fill(3)
  fill(0) = 7
  PRINT "shadow="; fill(0)
END SUB
