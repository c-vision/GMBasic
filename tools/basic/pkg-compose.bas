' pkg-compose.bas — COMPONE l'artefatto autonomo di un pacchetto-lib.
'
' Equivalente BASIC di `gmbsuite/gmbpackages/tools/compose.sh` (che sparisce con gmbsuite).
' I sorgenti restano dove sono; l'artefatto è una COPIA autonoma (manifest + .inc + include/ +
' lib/<plat>/ + samples/), pronta per `gmb publish`.
'
' Uso: pkg-compose.bas <lib> [--out DIR] [--root DIR]
'   --out   destinazione dell'artefatto (default: <root>/dist/<lib>)
'   --root  radice in cui cercare <lib>/packaging/gmb-package.json (default: CWD)
IMPORT IO
IMPORT SYSTEM
IMPORT PKGTOOLS

FUNCTION Main() AS INTEGER
  SET SHELL ON            ' SHELLCODE/SHELLOUT sono opt-in (eseguono make)
  DIM lib AS STRING, root AS STRING, out AS STRING, a AS STRING
  DIM i AS LONG

  root = IO.CWD()
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--root" AND i < COMMANDCOUNT() THEN
      i = i + 1 : root = COMMAND$(i)
    ELSEIF a = "--out" AND i < COMMANDCOUNT() THEN
      i = i + 1 : out = COMMAND$(i)
    ELSEIF LEFT$(a, 1) <> "-" AND lib = "" THEN
      lib = a
    END IF
    i = i + 1
  LOOP

  IF lib = "" THEN
    PRINT "uso: pkg-compose.bas <lib> [--out DIR] [--root DIR]"
    RETURN 2
  END IF
  IF out = "" THEN out = root + "/dist/" + lib

  RETURN PKGTOOLS.Compose(root, lib, out)
END FUNCTION
