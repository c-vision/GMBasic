' MiniTest SYSTEM — SIZEOF dipende dalla FORMA scritta, non dal folding.
'
' Canone (TypeLayout): identificatore (anche fra parentesi) → tipo statico; letterale numerico
' → 8; QUALUNQUE altra espressione → -1. Il linker applica folding algebrico (`i + 0` → i,
' `i * 1` → i, `1 + 2` → 3) e costante: senza la decisione "per forma" nel parser, SIZEOF
' rispondeva 2/8 su IL/PCode/VM-C e -1 su C99Native (divergenza 2026-11-01).
'
' EXPECT: plus=-1|mul=-1|div=-1|paren= 2|and=-1|cat=-1|simpl=-1|litparen= 8
DIM i AS INTEGER
i = 5
DIM b AS BIT
b = TRUE
DIM s AS STRING
s = "x"

PRINT "plus="; SIZEOF(i + 0)
PRINT "mul="; SIZEOF(i * 1)
PRINT "div="; SIZEOF(i / 1)
PRINT "paren="; SIZEOF((i))
PRINT "and="; SIZEOF(b AND TRUE)
PRINT "cat="; SIZEOF(s + "")
PRINT "simpl="; SIZEOF(1 + 2)
PRINT "litparen="; SIZEOF((42))
