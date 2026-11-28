' scene_census.bas — censimento della superficie `SCENE` realmente agganciata (P8).
'
' Non ha una lista scritta a mano: la superficie attesa si legge dalle DUE sedi canoniche
'   1. i verbi (comandi) da `gmb_p_scene_verb_tk` in gmb_parser_stmt_domain.c — l'unico punto
'      in cui il lessema diventa TokenKind (regola §12: a valle si dispatcha per TokenKind);
'   2. le letture da `shared/spec/registry/stdlib_tokens.csv` (righe ns = SCENE).
' Per ogni voce verifica che il TokenKind arrivi fino in fondo:
'   TK     gmb_tokenkind.h         il TokenKind esiste
'   EMIT   gmb_c99_emit_3d.c       l'emitter C99 lo tratta (gamba C99Native)
'   VMC    gmb_syscall_codes.h + gmb_vm_core.c / gmb_builtin.c  la gamba VM/C lo tratta
'   CS     Parser.Stmt.Scene.cs / StdLib.Scene.cs               il ponte C# lo conosce
'   RT     gmb_3d*.c               la facciata runtime ha la funzione gmb_scene_*
'   MINI   shared/minitests/3d     un MiniTest lo usa davvero
'   SAMP   Samples3D               un sample lo usa davvero
'   GUIDE  Guide/utente/SCENE-Guida-Utente.md   la guida lo documenta
' e riporta anche il drift inverso: TokenKind `Scene*` che nessuna superficie raggiunge,
' syscall `GMB_SC_SCENE_*` che nessun verbo produce.
'
' Exit 0 = nessuna lacuna bloccante. Exit 1 = manca almeno una gamba (EMIT/VMC/CS/RT) o un
' comando senza MiniTest. Le lacune di sample/guida sono WARN: si stampano, non bloccano
' (una lettura può essere documentata in una riga e non avere un sample dedicato).
' Exit 2 = sorgente canonica assente o illeggibile (non si censura il nulla).
'
' Uso: tools/sh/gmb_c99native_run.sh ../tools/basic/scene_census.bas census [--quiet]
IMPORT IO
IMPORT GREP

DIM g_root AS STRING

' ---- sorgenti, già in MAIUSCOLO (il confronto è case-insensitive in origine) ----
DIM s_tk AS STRING
DIM s_parser AS STRING
DIM s_emit AS STRING
DIM s_sys AS STRING
DIM s_vmcore AS STRING
DIM s_vmbf AS STRING
DIM s_cslib AS STRING
DIM s_csparse AS STRING
DIM s_rt AS STRING
DIM s_rdcsv AS STRING

' ---- superficie attesa ----
DIM g_cmdv(128) AS STRING
DIM g_cmdtk(128) AS STRING
DIM g_ncmd AS LONG
DIM g_rdv(128) AS STRING
DIM g_rdtk(128) AS STRING
DIM g_rdcs(128) AS STRING
DIM g_nrd AS LONG

' ---- corpus per MINI / SAMP / GUIDE (righe, una volta sola) ----
DIM g_mini(30000) AS STRING
DIM g_nmini AS LONG
DIM g_samp(4000) AS STRING
DIM g_nsamp AS LONG
DIM g_guide(8000) AS STRING
DIM g_nguide AS LONG

' ---- esito ----
DIM g_fail AS LONG
DIM g_warn AS LONG
DIM g_quiet AS LONG
DIM g_miss(400) AS STRING
DIM g_nmiss AS LONG
DIM g_hit AS LONG

' ============================ helper di testo ============================

FUNCTION IsWord(c AS STRING) AS INTEGER
  IF c >= "A" AND c <= "Z" THEN RETURN -1
  IF c >= "a" AND c <= "z" THEN RETURN -1
  IF c >= "0" AND c <= "9" THEN RETURN -1
  IF c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

' LOWER fatto a mano: il linguaggio non garantisce LOWER$ sulla gamba nativa.
FUNCTION Low(s AS STRING) AS STRING
  DIM i AS LONG, c AS STRING, o AS STRING
  o = ""
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c >= "A" AND c <= "Z" THEN c = CHR$(ASC(c) + 32)
    o = o + c
  NEXT i
  RETURN o
END FUNCTION

' Presenza di un token con confini di parola: `gmb_scene_zone` NON combacia con
' `gmb_scene_zone_at` (l'underscore è carattere di parola), che è esattamente il
' distinguo che serve fra la facciata di un verbo e la sua variante.
FUNCTION HasWord(txt AS STRING, tok AS STRING) AS INTEGER
  DIM p AS LONG, u AS STRING, t AS STRING
  u = UPPER(txt) : t = UPPER(tok)
  IF LEN(t) = 0 OR LEN(u) = 0 THEN RETURN 0
  p = 1
  DO WHILE p <= LEN(u)
    p = INSTR(p, u, t)
    IF p = 0 THEN EXIT DO
    IF (p = 1 OR IsWord(MID$(u, p - 1, 1)) = 0) AND (p + LEN(t) > LEN(u) OR IsWord(MID$(u, p + LEN(t), 1)) = 0) THEN RETURN -1
    p = p + LEN(t)
  LOOP
  RETURN 0
END FUNCTION

FUNCTION SkipSp(line AS STRING, p AS LONG) AS LONG
  DO WHILE p <= LEN(line)
    IF MID$(line, p, 1) <> " " AND MID$(line, p, 1) <> CHR$(9) THEN EXIT DO
    p = p + 1
  LOOP
  RETURN p
END FUNCTION

' sequenza di due parole a b (spazi arbitrari, confini di parola) sulla singola riga
FUNCTION CountSeq(line AS STRING, a AS STRING, b AS STRING) AS LONG
  DIM p AS LONG, q AS LONG, la AS LONG, lb AS LONG, n AS LONG
  la = LEN(a) : lb = LEN(b) : n = 0 : p = 1
  DO WHILE p <= LEN(line)
    p = INSTR(p, line, a)
    IF p = 0 THEN EXIT DO
    IF (p = 1 OR IsWord(MID$(line, p - 1, 1)) = 0) AND (p + la > LEN(line) OR IsWord(MID$(line, p + la, 1)) = 0) THEN
      q = SkipSp(line, p + la)
      IF q <= LEN(line) AND q + lb - 1 <= LEN(line) THEN
        IF MID$(line, q, lb) = b THEN
          IF q + lb > LEN(line) OR IsWord(MID$(line, q + lb, 1)) = 0 THEN n = n + 1
        END IF
      END IF
    END IF
    p = p + la
  LOOP
  RETURN n
END FUNCTION

FUNCTION Pad(s AS STRING, w AS LONG) AS STRING
  IF LEN(s) >= w THEN RETURN LEFT$(s, w)
  RETURN s + SPACE$(w - LEN(s))
END FUNCTION

' prima parola (lettere/cifre/underscore) della stringa: serve per togliere il `\r` di un
' CSV e il codice numerico che segue un nome in un #define.
FUNCTION FirstWord(s AS STRING) AS STRING
  DIM i AS LONG, c AS STRING, o AS STRING
  o = ""
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF IsWord(c) = 0 THEN EXIT FOR
    o = o + c
  NEXT i
  RETURN o
END FUNCTION

FUNCTION F(b AS INTEGER) AS STRING
  IF b <> 0 THEN RETURN "ok"
  RETURN "--"
END FUNCTION

' ============================ caricamento ============================

SUB LoadLines(txt AS STRING, which AS LONG)
  DIM l$() AS STRING, i AS LONG
  l$ = SPLIT(txt, CHR$(10))
  FOR i = 0 TO UBOUND(l$)
    IF which = 0 AND g_nmini < 30000 THEN
      g_nmini = g_nmini + 1 : g_mini(g_nmini) = l$(i)
    ELSEIF which = 1 AND g_nsamp < 4000 THEN
      g_nsamp = g_nsamp + 1 : g_samp(g_nsamp) = l$(i)
    ELSEIF which = 2 AND g_nguide < 8000 THEN
      g_nguide = g_nguide + 1 : g_guide(g_nguide) = l$(i)
    END IF
  NEXT i
END SUB

FUNCTION LoadFile(path AS STRING) AS STRING
  DIM t AS STRING
  IF NOT FILEEXISTS(path) THEN RETURN ""
  t = IO.READFILE(path)
  RETURN UPPER(t)
END FUNCTION

' concatena i sorgenti di una cartella che CITANO un marcatore: il censimento non
' elenca a mano i file, così un nuovo gmb_3d_*.c entra da solo.
FUNCTION LoadDir(rel AS STRING, marker AS STRING) AS STRING
  DIM dir AS STRING, files$() AS STRING, i AS LONG, t AS STRING, one AS STRING
  dir = g_root + "/" + rel
  t = ""
  IF NOT DIREXISTS(dir) THEN RETURN ""
  files$ = GREP.FILES(marker, dir)
  FOR i = 0 TO UBOUND(files$)
    one = files$(i)
    IF UPPER(RIGHT$(one, 2)) = ".H" OR UPPER(RIGHT$(one, 2)) = ".C" THEN
      t = t + CHR$(10) + UPPER(IO.READFILE(one))
    END IF
  NEXT i
  RETURN t
END FUNCTION

FUNCTION Corpus()
  DIM dir AS STRING, files$() AS STRING, i AS LONG
  DIM t AS STRING, all AS STRING
  dir = g_root + "/shared/minitests/3d"
  all = ""
  IF DIREXISTS(dir) THEN
    files$ = GREP.FILES("SCENE", dir)
    FOR i = 0 TO UBOUND(files$)
      IF RIGHT$(UPPER(files$(i)), 4) = ".BAS" THEN all = all + UPPER(IO.READFILE(files$(i))) + CHR$(10)
    NEXT i
  END IF
  LoadLines(all, 0)
  dir = g_root + "/Samples3D"
  all = ""
  IF DIREXISTS(dir) THEN
    files$ = GREP.FILES("SCENE", dir)
    FOR i = 0 TO UBOUND(files$)
      IF RIGHT$(UPPER(files$(i)), 4) = ".BAS" THEN all = all + UPPER(IO.READFILE(files$(i))) + CHR$(10)
    NEXT i
  END IF
  LoadLines(all, 1)
  RETURN 0
END FUNCTION

' ============================ superficie attesa ============================

' `{ "RESET",   GMB_TOK_SceneReset  },`  →  verbo + TokenKind
SUB ParseMapLine(ln AS STRING)
  DIM p AS LONG, q AS LONG, k AS LONG, e AS LONG, c AS STRING, qq AS STRING, pos AS LONG
  qq = CHR$(34)
  pos = 1
  DO WHILE pos <= LEN(ln)
    p = INSTR(pos, ln, "{ " + qq)
    IF p = 0 THEN EXIT DO
    q = INSTR(p + 3, ln, qq)
    IF q = 0 THEN EXIT DO
    k = INSTR(q, ln, "GMB_TOK_Scene")
    IF k = 0 THEN EXIT DO
    e = k
    DO WHILE e <= LEN(ln)
      c = MID$(ln, e, 1)
      IF IsWord(c) = 0 THEN EXIT DO
      e = e + 1
    LOOP
    IF e - k >= 15 THEN
      IF g_ncmd < 128 THEN
        g_ncmd = g_ncmd + 1
        g_cmdv(g_ncmd) = MID$(ln, p + 3, q - (p + 3))
        g_cmdtk(g_ncmd) = MID$(ln, k, e - k)
      END IF
    END IF
    pos = e
    IF pos <= k THEN pos = k + 1
  LOOP
  g_hit = g_ncmd
END SUB

' riga CSV: 893,SCENE,CEILAT,SceneCeilAt,TokenKind.SceneCeilAt
SUB ParseReadLine(ln AS STRING)
  DIM f$() AS STRING, nm AS STRING, tk AS STRING
  IF INSTR(ln, ",SCENE,") = 0 THEN EXIT SUB
  f$ = SPLIT(ln, ",")
  IF UBOUND(f$) < 4 THEN EXIT SUB
  nm = FirstWord(f$(2))
  tk = f$(4)
  IF INSTR(tk, "TokenKind.") = 0 THEN EXIT SUB
  IF g_nrd >= 128 THEN EXIT SUB
  g_nrd = g_nrd + 1
  g_rdv(g_nrd) = nm
  ' TokenKind.SceneXxx serve al ponte C# così come è scritto; GMB_TOK_SceneXxx alle gambe C.
  g_rdcs(g_nrd) = FirstWord(tk)
  g_rdtk(g_nrd) = "GMB_TOK_" + FirstWord(MID$(tk, INSTR(tk, "TokenKind.") + 10))
END SUB

SUB LoadSurface()
  DIM l$() AS STRING, i AS LONG
  l$ = SPLIT(s_parser, CHR$(10))
  FOR i = 0 TO UBOUND(l$)
    ParseMapLine(l$(i))
  NEXT i
  l$ = SPLIT(s_rdcsv, CHR$(10))
  FOR i = 0 TO UBOUND(l$)
    ParseReadLine(l$(i))
  NEXT i
END SUB

' ============================ nomi nelle singole gambe ============================

' facciata runtime di un COMANDO (una sola eccezione: TEXTURE non ha gmb_scene_texture)
FUNCTION RtCmd(v AS STRING) AS STRING
  IF v = "TEXTURE" THEN RETURN "gmb_scene_texture_file"
  RETURN "gmb_scene_" + Low(v)
END FUNCTION

' facciata runtime di una LETTURA (le P5 sono spezzate: floor_at, zone_at, …)
FUNCTION RtRead(nm AS STRING) AS STRING
  IF nm = "FLOORAT" THEN RETURN "gmb_scene_floor_at"
  IF nm = "CEILAT" THEN RETURN "gmb_scene_ceil_at"
  IF nm = "ZONE" THEN RETURN "gmb_scene_zone_at"
  IF nm = "ZONETAG" THEN RETURN "gmb_scene_zonetag_text"
  IF nm = "SOUNDGAIN" THEN RETURN "gmb_scene_sound_gain"
  IF nm = "SOUNDPAN" THEN RETURN "gmb_scene_sound_pan"
  RETURN "gmb_scene_" + Low(nm)
END FUNCTION

' builtin VM/C di una LETTURA (CEILAT si chiama `ceiling` da prima di SCENE)
FUNCTION VmcRead(nm AS STRING) AS STRING
  IF nm = "CEILAT" THEN RETURN "gmb_bf_scene_ceiling"
  RETURN "gmb_bf_scene_" + Low(nm)
END FUNCTION

' ============================ esito ============================

SUB Miss(what AS STRING)
  IF g_nmiss < 400 THEN
    g_nmiss = g_nmiss + 1
    g_miss(g_nmiss) = what
  END IF
END SUB

' cercare nei corpus: righe di commento (`'`) escluse — un comando citato in un commento
' non è un uso, e il censimento deve dire "nessun test lo usa", non "il test lo nomina".
FUNCTION InCorpus(which AS LONG, a AS STRING, b AS STRING, patt AS STRING) AS INTEGER
  DIM i AS LONG, n AS LONG, s AS STRING
  IF which = 0 THEN n = g_nmini
  IF which = 1 THEN n = g_nsamp
  IF which = 2 THEN n = g_nguide
  FOR i = 1 TO n
    IF which = 0 THEN s = g_mini(i)
    IF which = 1 THEN s = g_samp(i)
    IF which = 2 THEN s = g_guide(i)
    IF LEFT$(LTRIM$(s), 1) <> "'" THEN
      IF LEN(patt) > 0 THEN
        IF INSTR(s, patt) > 0 THEN RETURN -1
      ELSE
        IF CountSeq(s, a, b) > 0 THEN RETURN -1
      END IF
    END IF
  NEXT i
  RETURN 0
END FUNCTION

SUB CmdRow(i AS LONG)
  DIM v AS STRING, tk AS STRING
  DIM b_tk AS INTEGER, b_emit AS INTEGER, b_sys AS INTEGER, b_vm AS INTEGER
  DIM b_cs AS INTEGER, b_rt AS INTEGER, b_mini AS INTEGER, b_samp AS INTEGER, b_gu AS INTEGER
  DIM qq AS STRING
  qq = CHR$(34)
  v = g_cmdv(i) : tk = g_cmdtk(i)
  b_tk = HasWord(s_tk, tk)
  b_emit = HasWord(s_emit, tk)
  b_sys = HasWord(s_sys, "GMB_SC_SCENE_" + v)
  b_vm = HasWord(s_vmcore, "GMB_SC_SCENE_" + v)
  b_cs = HasWord(s_csparse, qq + v + qq)
  b_rt = HasWord(s_rt, RtCmd(v))
  b_mini = InCorpus(0, "SCENE", v, "")
  b_samp = InCorpus(1, "SCENE", v, "")
  b_gu = InCorpus(2, "SCENE", v, "")

  IF g_quiet = 0 THEN
    PRINT "  "; Pad(v, 11); " TK="; F(b_tk); " EMIT="; F(b_emit); " VMC="; F(b_sys AND b_vm); " CS="; F(b_cs); " RT="; F(b_rt); " MINI="; F(b_mini); " SAMP="; F(b_samp); " GUIDE="; F(b_gu)
  END IF

  IF b_emit = 0 THEN CALL Miss("comando " + v + ": emitter C99 non lo tratta (" + tk + ")")
  IF b_sys = 0 OR b_vm = 0 THEN CALL Miss("comando " + v + ": VM/C non lo tratta (GMB_SC_SCENE_" + v + ")")
  IF b_cs = 0 THEN CALL Miss("comando " + v + ": il ponte C# non lo conosce")
  IF b_rt = 0 THEN CALL Miss("comando " + v + ": facciata runtime assente (" + RtCmd(v) + ")")
  IF b_mini = 0 THEN CALL Miss("comando " + v + ": nessun MiniTest lo usa")
  IF b_gu = 0 THEN CALL Miss("comando " + v + ": guida utente non lo documenta")
  IF b_emit = 0 OR b_sys = 0 OR b_vm = 0 OR b_cs = 0 OR b_rt = 0 OR b_mini = 0 THEN g_fail = g_fail + 1
  IF b_gu = 0 OR b_samp = 0 THEN g_warn = g_warn + 1
END SUB

SUB RdRow(i AS LONG)
  DIM nm AS STRING, tk AS STRING
  DIM b_tk AS INTEGER, b_emit AS INTEGER, b_vm AS INTEGER
  DIM b_cs AS INTEGER, b_rt AS INTEGER, b_mini AS INTEGER, b_samp AS INTEGER, b_gu AS INTEGER
  nm = g_rdv(i) : tk = g_rdtk(i)
  b_tk = HasWord(s_tk, tk)
  b_emit = HasWord(s_emit, tk)
  b_vm = HasWord(s_vmbf, VmcRead(nm))
  b_cs = HasWord(s_cslib, g_rdcs(i))
  b_rt = HasWord(s_rt, RtRead(nm))
  b_mini = InCorpus(0, "", "", "SCENE." + nm + "(")
  b_samp = InCorpus(1, "", "", "SCENE." + nm + "(")
  b_gu = InCorpus(2, "", "", "SCENE." + nm)
  ' la guida raggruppa le letture affini: `SCENE.SHOTX()` / `SHOTY()` / `SHOTZ()`.
  ' Un nome in forma di chiamata fra backtick è documentato anche senza il prefisso SCENE.
  IF b_gu = 0 THEN b_gu = InCorpus(2, "", "", "`" + nm + "(")

  IF g_quiet = 0 THEN
    PRINT "  "; Pad(nm, 11); " TK="; F(b_tk); " EMIT="; F(b_emit); " VMC="; F(b_vm); " CS="; F(b_cs); " RT="; F(b_rt); " MINI="; F(b_mini); " SAMP="; F(b_samp); " GUIDE="; F(b_gu)
  END IF

  IF b_emit = 0 THEN CALL Miss("lettura " + nm + ": emitter C99 non la tratta (" + tk + ")")
  IF b_vm = 0 THEN CALL Miss("lettura " + nm + ": VM/C non la tratta (" + VmcRead(nm) + ")")
  IF b_cs = 0 THEN CALL Miss("lettura " + nm + ": il ponte C# non la registra")
  IF b_rt = 0 THEN CALL Miss("lettura " + nm + ": facciata runtime assente (" + RtRead(nm) + ")")
  IF b_gu = 0 THEN CALL Miss("lettura " + nm + ": guida utente non la documenta")
  IF b_emit = 0 OR b_vm = 0 OR b_cs = 0 OR b_rt = 0 THEN g_fail = g_fail + 1
  IF b_mini = 0 OR b_samp = 0 OR b_gu = 0 THEN g_warn = g_warn + 1
END SUB

' TokenKind `Scene*` che nessuna delle due superfici raggiunge: o è un residuo, o è
' una superficie nuova dimenticata nel tavolo dei verbi (che è il caso che P8 deve vedere).
SUB DriftTokens()
  DIM l$() AS STRING, i AS LONG, j AS LONG, ln AS STRING, nm AS STRING, s AS STRING, found AS LONG
  l$ = SPLIT(s_tk, CHR$(10))
  FOR i = 0 TO UBOUND(l$)
    ln = LTRIM$(l$(i))
    IF LEFT$(ln, 12) = "GMB_TOK_Scen" THEN
      nm = ""
      DO WHILE LEN(nm) < LEN(ln)
        s = MID$(ln, LEN(nm) + 1, 1)
        IF IsWord(s) = 0 THEN EXIT DO
        nm = nm + s
      LOOP
      IF nm <> "GMB_TOK_Scene" THEN
        found = 0
        FOR j = 1 TO g_ncmd
          IF g_cmdtk(j) = nm THEN found = 1
        NEXT j
        FOR j = 1 TO g_nrd
          IF g_rdtk(j) = nm THEN found = 1
        NEXT j
        IF found = 0 THEN
          CALL Miss("TokenKind " + nm + " dichiarato ma nessun verbo/lettura lo raggiunge")
          g_fail = g_fail + 1
        END IF
      END IF
    END IF
  NEXT i
END SUB

' syscall SCENE_* che nessun verbo produce: le varianti di forma sono attese, le altre no.
SUB DriftSyscalls()
  DIM l$() AS STRING, i AS LONG, ln AS STRING, nm AS STRING, s AS STRING, found AS LONG, j AS LONG
  l$ = SPLIT(s_sys, CHR$(10))
  FOR i = 0 TO UBOUND(l$)
    ln = LTRIM$(l$(i))
    IF LEFT$(ln, 11) = "#DEFINE GMB" THEN
      IF INSTR(ln, "GMB_SC_SCENE_") > 0 THEN
        nm = FirstWord(MID$(ln, INSTR(ln, "GMB_SC_SCENE_") + 13))
        DO WHILE LEN(nm) > 0
          s = RIGHT$(nm, 1)
          IF s = " " OR s = CHR$(13) OR s = CHR$(9) THEN
            nm = LEFT$(nm, LEN(nm) - 1)
          ELSE
            EXIT DO
          END IF
        LOOP
        found = 0
        FOR j = 1 TO g_ncmd
          IF g_cmdv(j) = nm THEN found = 1
        NEXT j
        ' varianti di forma: stesso verbo, argomenti diversi (trecce esplicite)
        IF nm = "LAMP_SPOT" OR nm = "OBJECT_FLAGS" OR nm = "VERTEX_N" OR nm = "POSEBONE_TRS" OR nm = "ANIM_TRS" THEN found = 1
        IF nm = "ZONE" THEN found = 1
        IF nm = "PLAYER" THEN found = 1
        IF nm = "MOVE" THEN found = 1
        IF nm = "TURN" THEN found = 1
        IF nm = "LIFT" THEN found = 1
        IF found = 0 THEN
          CALL Miss("syscall GMB_SC_SCENE_" + nm + " senza verbo nel tavolo del parser")
          g_fail = g_fail + 1
        END IF
      END IF
    END IF
  NEXT i
END SUB

' ============================ main ============================

FUNCTION Main() AS INTEGER
  DIM i AS LONG, arg AS STRING, up AS LONG
  DIM repo AS STRING, p AS STRING

  repo = IO.CWD()
  FOR up = 1 TO 8
    IF DIREXISTS(repo + "/GMBasicSharp/common") AND DIREXISTS(repo + "/GMBasic/native") THEN EXIT FOR
    p = IO.PARENTDIR(repo)
    IF p = repo OR LEN(p) = 0 THEN EXIT FOR
    repo = p
  NEXT up
  IF NOT DIREXISTS(repo + "/GMBasicSharp/common") THEN
    PRINT "ERRORE: root del monorepo non trovata (cercata risalendo da "; IO.CWD(); ")"
    RETURN 2
  END IF
  g_root = repo

  i = 0
  DO WHILE i < COMMANDCOUNT()
    i = i + 1
    arg = COMMAND$(i)
    IF arg = "--quiet" THEN g_quiet = -1
  LOOP

  s_tk = LoadFile(g_root + "/GMBasic/native/gmb_compiler_c/frontend/include/gmb_tokenkind.h")
  s_parser = LoadFile(g_root + "/GMBasic/native/gmb_compiler_c/frontend/src/parser/gmb_parser_stmt_domain.c")
  s_emit = LoadFile(g_root + "/GMBasic/native/gmb_c99native/src/gmb_c99_emit_3d.c")
  s_sys = LoadFile(g_root + "/GMBasic/native/gmb_vm_c/gmb_syscall_codes.h")
  s_vmcore = LoadFile(g_root + "/GMBasic/native/gmb_vm_c/gmb_vm_core.c")
  s_vmbf = LoadFile(g_root + "/GMBasic/native/gmb_vm_c/gmb_builtin.c")
  s_cslib = LoadFile(g_root + "/GMBasicSharp/common/StdLib/StdLib.Scene.cs")
  s_csparse = LoadFile(g_root + "/GMBasicSharp/common/Parser/Parser.Stmt.Scene.cs")
  s_rdcsv = LoadFile(g_root + "/shared/spec/registry/stdlib_tokens.csv")
  s_rt = LoadDir("GMBasic/native/gmb_runtime_c/src", "gmb_scene_")

  IF LEN(s_parser) = 0 THEN
    PRINT "ERRORE: tavolo dei verbi SCENE non trovato (gmb_parser_stmt_domain.c)"
    RETURN 2
  END IF
  IF LEN(s_rdcsv) = 0 THEN
    PRINT "ERRORE: registro delle letture non trovato (stdlib_tokens.csv)"
    RETURN 2
  END IF
  IF LEN(s_tk) = 0 OR LEN(s_emit) = 0 OR LEN(s_sys) = 0 OR LEN(s_vmcore) = 0 THEN
    PRINT "ERRORE: manca una sorgente canonica (tokenkind/emitter/syscall/VM-C)"
    RETURN 2
  END IF

  LoadSurface()
  Corpus()
  s_guide = LoadFile(g_root + "/Guide/utente/SCENE-Guida-Utente.md")
  LoadLines(s_guide, 2)

  IF g_quiet = 0 THEN
    PRINT "# Censimento superficie SCENE  (root="; g_root; ")"
    PRINT "Comandi attesi (tavolo verbi): "; g_ncmd; " | letture attese (registro): "; g_nrd
    PRINT ""
    PRINT "## Comandi"
  END IF
  FOR i = 1 TO g_ncmd
    CmdRow(i)
  NEXT i
  IF g_quiet = 0 THEN
    PRINT ""
    PRINT "## Letture"
  END IF
  FOR i = 1 TO g_nrd
    RdRow(i)
  NEXT i

  DriftTokens()
  DriftSyscalls()

  IF g_quiet = 0 OR g_nmiss > 0 THEN
    PRINT ""
    PRINT "## Lacune ("; g_nmiss; ")"
    IF g_nmiss = 0 THEN PRINT "  nessuna"
    FOR i = 1 TO g_nmiss
      PRINT "  - "; g_miss(i)
    NEXT i
  END IF

  PRINT ""
  PRINT "SCENE-CENSUS comandi="; g_ncmd; " letture="; g_nrd; " lacune="; g_nmiss; " warn="; g_warn; " exit="; IIF(g_fail > 0, 1, 0)
  IF g_fail > 0 THEN RETURN 1
  RETURN 0
END FUNCTION
