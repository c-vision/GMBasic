' pkg-emit-version.bas — EMETTE una nuova versione di un pacchetto-lib.
'
' Equivalente BASIC di `gmbsuite/gmbpackages/tools/emit-version.sh` (che sparisce con gmbsuite).
' Bumpa `version` in `<lib>/packaging/gmb-package.json` (e sincronizza `VER :=` nel Makefile, se
' c'è), poi ricompone l'artefatto con pkg-compose.
'
' Uso: pkg-emit-version.bas <lib> <major|minor|patch|X.Y.Z> [--no-compose] [--root DIR]
IMPORT IO
IMPORT SYSTEM
IMPORT PKGTOOLS

FUNCTION Main() AS INTEGER
  SET SHELL ON            ' SHELLCODE/SHELLOUT sono opt-in (eseguono make)
  DIM lib AS STRING, bump AS STRING, root AS STRING, a AS STRING
  DIM man AS STRING, txt AS STRING, cur AS STRING, nv AS STRING
  DIM mk AS STRING, mk2 AS STRING, i AS LONG, doCompose AS LONG, rc AS LONG

  root = IO.CWD()
  doCompose = 1
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--root" AND i < COMMANDCOUNT() THEN
      i = i + 1 : root = COMMAND$(i)
    ELSEIF a = "--no-compose" THEN
      doCompose = 0
    ELSEIF LEFT$(a, 1) <> "-" AND lib = "" THEN
      lib = a
    ELSEIF LEFT$(a, 1) <> "-" AND bump = "" THEN
      bump = a
    END IF
    i = i + 1
  LOOP

  IF lib = "" OR bump = "" THEN
    PRINT "uso: pkg-emit-version.bas <lib> <major|minor|patch|X.Y.Z> [--no-compose] [--root DIR]"
    RETURN 2
  END IF

  man = PKGTOOLS.FindManifest(root, lib)
  IF man = "" THEN
    PRINT "[emit-version] '"; lib; "' non ha packaging/gmb-package.json"
    RETURN 1
  END IF

  txt = IO.READFILE(man)
  cur = PKGTOOLS.JsonStr(txt, "version")
  nv = PKGTOOLS.BumpVersion(cur, bump)
  IF nv = "" OR nv = cur THEN
    PRINT "[emit-version] versione non valida: '"; bump; "' (attuali: "; cur; ")"
    RETURN 2
  END IF

  ' manifest: nuova versione (prima occorrenza del campo "version")
  DIM h AS LONG
  h = IO.FCREATE(man)
  IO.FWRITE(h, PKGTOOLS.JsonSetStr(txt, "version", nv))
  IO.FCLOSE(h)
  PRINT "[emit-version] "; lib; ": manifest "; cur; " -> "; nv

  ' Makefile accanto alla lib: allinea `VER := <versione>` se la variabile esiste
  mk = PKGTOOLS.DirName(PKGTOOLS.DirName(man)) + "/Makefile"
  IF FILEEXISTS(mk) THEN
    mk2 = IO.READFILE(mk)
    IF INSTR(1, mk2, "VER :=") > 0 OR INSTR(1, mk2, "VER:=") > 0 THEN
      DIM p AS LONG, e AS LONG, pre AS STRING, post AS STRING
      p = INSTR(1, mk2, "VER")
      IF p > 0 THEN
        e = INSTR(p, mk2, CHR$(10))
        IF e = 0 THEN e = LEN(mk2) + 1
        pre = LEFT$(mk2, p - 1)
        post = MID$(mk2, e)
        h = IO.FCREATE(mk)
        IO.FWRITE(h, pre + "VER := " + nv + post)
        IO.FCLOSE(h)
        PRINT "[emit-version] "; lib; ": Makefile VER := "; nv
      END IF
    END IF
  END IF

  IF doCompose = 1 THEN
    rc = PKGTOOLS.Compose(root, lib, root + "/dist/" + lib)
    IF rc <> 0 THEN RETURN rc
  END IF
  RETURN 0
END FUNCTION
