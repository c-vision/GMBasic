' fence.bas — fenca i .md giganti (molte direttive \begin / tag HTML) con un blocco ```md.
' Equivalente del vecchio tools/fence.py, senza Python.
' Uso: gmb fence.bas [root]     (default: CWD)
IMPORT IO
IMPORT GREP

FUNCTION CountOf(s AS STRING, sub AS STRING) AS LONG
  DIM p AS LONG, n AS LONG
  p = 1 : n = 0
  DO WHILE p <= LEN(s)
    p = INSTR(p, s, sub)
    IF p = 0 THEN EXIT DO
    n = n + 1
    p = p + LEN(sub)
  LOOP
  RETURN n
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM root AS STRING, f AS STRING, d AS STRING, h AS LONG
  DIM files$() AS STRING
  DIM i AS LONG, n AS LONG
  IF COMMANDCOUNT() >= 1 THEN root = COMMAND$(1) ELSE root = IO.CWD()
  IF NOT DIREXISTS(root) THEN
    PRINT "ERRORE: root non trovato ("; root; ")"
    RETURN 2
  END IF
  files$ = GREP.FILES(".", root)
  n = 0
  IF UBOUND(files$) >= 0 THEN
    FOR i = 0 TO UBOUND(files$)
      f = files$(i)
      IF RIGHT$(UPPER(f), 3) = ".MD" THEN
        IF INSTR(f, "/.obsidian") = 0 AND FILEEXISTS(f) THEN
          d = IO.READFILE(f)
          IF CountOf(d, "\begin") > 200 OR CountOf(d, "<div") > 200 OR CountOf(d, "<img") > 200 THEN
            h = IO.FCREATE(f)
            IO.FWRITE(h, "```md" + CHR$(10) + d + CHR$(10) + "```")
            IO.FCLOSE(h)
            n = n + 1
          END IF
        END IF
      END IF
    NEXT i
  END IF
  PRINT "fenced: "; n
  RETURN 0
END FUNCTION
