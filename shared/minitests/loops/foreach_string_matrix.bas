' MiniTest: FOR EACH su STRINGA (itera i CARATTERI) e su MATRICE 2D (itera le RIGHE).
' Sull'emitter C# erano `#error unsupported FOR EACH collection` (chiuso 2026-09-17);
' VM/C e C99Native avevano divergenze (chiuso 2026-09-18: UBOUND/LOAD_ELEMENT_POSITIONAL
' per stringa sul VM/C, ramo stringa + righe sul C99Native).
DIM cc AS INTEGER = 0
DIM s AS STRING = ""
FOR EACH ch IN "abc"
  cc = cc + 1
  s = s + ch
NEXT
PRINT "str="; cc; " s="; s

DIM m(1, 1) AS INTEGER
m(0,0) = 1 : m(0,1) = 2 : m(1,0) = 3 : m(1,1) = 4
DIM mrows AS INTEGER = 0
FOR EACH row IN m
  mrows = mrows + 1
NEXT
PRINT "mrows="; mrows

DIM lst = ["p", "q", "r", "s"]
DIM full AS INTEGER = 0
FOR EACH e IN lst
  full = full + 1
NEXT
PRINT "full="; full
' EXPECT: str= 3  s=abc|mrows= 2|full= 4
