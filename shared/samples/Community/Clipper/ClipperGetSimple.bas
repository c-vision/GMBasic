' =============================================================================
' ClipperGetSimple.bas — Test minimo @...GET + READ
' =============================================================================

DIM nome AS STRING
nome = ""
@ 10, 10 SAY "Nome:"
@ 10, 16 GET nome
READ
PRINT "GOT:"; nome
