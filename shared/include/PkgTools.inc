' PkgTools.inc — helper per i PACCHETTI (composizione + versioning), in puro BASIC.
' Usato da `tools/basic/pkg-compose.bas` e `tools/basic/pkg-emit-version.bas`.
'
' PERCHÉ ESISTE. I due tool erano script bash in `gmbsuite/gmbpackages/tools/`: componevano
' l'artefatto autonomo di una lib (make + copia di `packaging/`) e ne emettevano la versione.
' gmbsuite sparisce, la capacità resta: qui, in GMBASIC.
'
' LAYOUT ATTESO (fonte di verità del pacchetto):
'   <root>/**/<lib>/packaging/gmb-package.json     manifest (version, build.target, build.cwd)
'   <root>/**/<lib>/packaging/                     artefatto autonomo (manifest + .inc + include/…)
NAMESPACE PKGTOOLS

' Valore stringa di un campo JSON `"chiave": "valore"` (prima occorrenza). "" se assente.
PUBLIC FUNCTION JsonStr(text AS STRING, key AS STRING) AS STRING
  DIM p AS LONG, q AS LONG, e AS LONG
  p = INSTR(1, text, CHR$(34) + key + CHR$(34))
  IF p = 0 THEN RETURN ""
  q = INSTR(p, text, ":")
  IF q = 0 THEN RETURN ""
  q = INSTR(q, text, CHR$(34))
  IF q = 0 THEN RETURN ""
  e = INSTR(q + 1, text, CHR$(34))
  IF e = 0 THEN RETURN ""
  RETURN MID$(text, q + 1, e - q - 1)
END FUNCTION

' Sostituisce il valore stringa del campo `chiave` (prima occorrenza). Ritorna il testo nuovo.
PUBLIC FUNCTION JsonSetStr(text AS STRING, key AS STRING, val AS STRING) AS STRING
  DIM p AS LONG, q AS LONG, e AS LONG
  p = INSTR(1, text, CHR$(34) + key + CHR$(34))
  IF p = 0 THEN RETURN text
  q = INSTR(p, text, ":")
  IF q = 0 THEN RETURN text
  q = INSTR(q, text, CHR$(34))
  IF q = 0 THEN RETURN text
  e = INSTR(q + 1, text, CHR$(34))
  IF e = 0 THEN RETURN text
  RETURN LEFT$(text, q) + val + MID$(text, e)
END FUNCTION

' Nome base di un percorso (`a/b/c` → `c`).
PUBLIC FUNCTION BaseName(p AS STRING) AS STRING
  DIM i AS LONG, k AS LONG
  k = 0
  FOR i = 1 TO LEN(p)
    IF MID$(p, i, 1) = "/" THEN k = i
  NEXT i
  IF k = 0 THEN RETURN p
  RETURN MID$(p, k + 1)
END FUNCTION

' Cartella di un percorso (`a/b/c` → `a/b`).
PUBLIC FUNCTION DirName(p AS STRING) AS STRING
  DIM i AS LONG, k AS LONG
  k = 0
  FOR i = 1 TO LEN(p)
    IF MID$(p, i, 1) = "/" THEN k = i
  NEXT i
  IF k <= 1 THEN RETURN ""
  RETURN LEFT$(p, k - 1)
END FUNCTION

' Quoting minimo per la shell (i percorsi possono contenere spazi).
PUBLIC FUNCTION Q(p AS STRING) AS STRING
  RETURN CHR$(34) + p + CHR$(34)
END FUNCTION

' Versione bumpata: kind = major|minor|patch, oppure una versione esplicita "X.Y.Z".
PUBLIC FUNCTION BumpVersion(cur AS STRING, kind AS STRING) AS STRING
  DIM a AS LONG, b AS LONG, c AS LONG, p1 AS LONG, p2 AS LONG
  p1 = INSTR(1, cur, ".")
  p2 = INSTR(p1 + 1, cur, ".")
  IF p1 > 0 AND p2 > 0 THEN
    a = VAL(LEFT$(cur, p1 - 1))
    b = VAL(MID$(cur, p1 + 1, p2 - p1 - 1))
    c = VAL(MID$(cur, p2 + 1))
  ELSE
    a = VAL(cur) : b = 0 : c = 0
  END IF
  DIM k AS STRING
  k = LCASE$(kind)
  IF k = "major" THEN
    a = a + 1
    b = 0
    c = 0
  ELSEIF k = "minor" THEN
    b = b + 1
    c = 0
  ELSEIF k = "patch" THEN
    c = c + 1
  ELSE
    RETURN kind                          ' versione esplicita
  END IF
  ' `STR$` aggiunge uno spazio iniziale (formato BASIC): qui serve "1.2.3" pulito.
  RETURN LTRIM$(STR$(a)) + "." + LTRIM$(STR$(b)) + "." + LTRIM$(STR$(c))
END FUNCTION

' Trova `<root>/**/<lib>/packaging/gmb-package.json` (BFS iterativa; salta build/out/.git).
PUBLIC FUNCTION FindManifest(root AS STRING, lib AS STRING) AS STRING
  DIM stack$(8192) AS STRING, kids$() AS STRING
  DIM sp AS LONG, i AS LONG
  DIM d AS STRING, k AS STRING, cand AS STRING
  sp = 1 : stack$(1) = root
  DO WHILE sp > 0
    d = stack$(sp) : sp = sp - 1
    IF BaseName(d) = lib THEN
      cand = d + "/packaging/gmb-package.json"
      IF FILEEXISTS(cand) THEN RETURN cand
    END IF
    kids$ = IO.DIR(d + "/*")
    IF UBOUND(kids$) >= 0 THEN
      FOR i = 0 TO UBOUND(kids$)
        ' IO.DIR restituisce il NOME, non il percorso: si ricompone qui.
        k = kids$(i)
        IF LEFT$(k, 1) <> "/" THEN k = d + "/" + k
        IF IO.DIREXISTS(k) THEN
          IF INSTR(k, "/.git") = 0 AND INSTR(k, "/build") = 0 AND INSTR(k, "/.build") = 0 AND INSTR(k, "/out") = 0 THEN
            IF sp < 8190 THEN sp = sp + 1 : stack$(sp) = k
          END IF
        END IF
      NEXT i
    END IF
  LOOP
  RETURN ""
END FUNCTION

' Compone l'artefatto autonomo di `<lib>`: `make -C <dir> <target>` + copia di `packaging/` in `out`.
' Ritorna 0 = ok, 1 = errore. Stampa l'avanzamento (come faceva compose.sh).
PUBLIC FUNCTION Compose(root AS STRING, lib AS STRING, out AS STRING) AS LONG
  DIM man AS STRING, libdir AS STRING, target AS STRING, cwd AS STRING
  DIM pkg AS STRING, js AS STRING, mkdirp AS STRING, cmd AS STRING, ver AS STRING

  man = FindManifest(root, lib)
  IF man = "" THEN
    PRINT "[compose] '"; lib; "' non ha packaging/gmb-package.json (non è distribuita come pacchetto)"
    RETURN 1
  END IF
  libdir = DirName(DirName(man))
  js = IO.READFILE(man)
  target = JsonStr(js, "target")
  IF target = "" THEN target = "packlib"
  cwd = JsonStr(js, "cwd")

  mkdirp = libdir
  IF cwd <> "" AND cwd <> "." THEN mkdirp = mkdirp + "/" + cwd

  PRINT "[compose] "; lib; ": make -C "; mkdirp; " "; target
  cmd = "make -C " + Q(mkdirp) + " " + target
  IF SYSTEM.SHELLCODE(cmd) <> 0 THEN
    PRINT "[compose] build fallito (make "; target; ")"
    RETURN 1
  END IF

  pkg = libdir + "/packaging"
  IF NOT IO.DIREXISTS(pkg) THEN
    PRINT "[compose] packaging/ assente dopo il build"
    RETURN 1
  END IF
  cmd = "rm -rf " + Q(out) + " && mkdir -p " + Q(out) + " && cp -R " + Q(pkg) + "/. " + Q(out)
  IF SYSTEM.SHELLCODE(cmd) <> 0 THEN
    PRINT "[compose] copia fallita: "; pkg; " -> "; out
    RETURN 1
  END IF

  ver = JsonStr(IO.READFILE(out + "/gmb-package.json"), "version")
  PRINT "[compose] OK -> "; out; " (v"; ver; ")"
  RETURN 0
END FUNCTION
