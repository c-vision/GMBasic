' StringsUtf8Len.bas — LEN (CARATTERI) vs LENB (BYTE UTF-8), sottostringhe per carattere.
'
' Mostra la differenza su testo ASCII, su caratteri a 2/3 byte (é, ─) e a 4 byte (😀):
'   LEN(s)  = numero di CARATTERI (code point Unicode)      → "😀" = 1
'   LENB(s) = numero di BYTE della codifica UTF-8           → "😀" = 4
' LEFT$/MID$/RIGHT$/INSTR usano anch'essi i CARATTERI: non spezzano mai un carattere.
' Utile per: conteggio visibile (LEN) vs limiti di buffer/file/Content-Length (LENB).
'
' Esecuzione: tutte e 4 le gambe (IL, PCodeVM, VM/C, C99Native) — stesso output.
IMPORT STRINGS

DIM campioni(4) AS STRING
DIM etichette(4) AS STRING
DIM i AS LONG

campioni(0) = "abc" : etichette(0) = "ascii"
campioni(1) = "é"   : etichette(1) = "accento"      ' 2 byte UTF-8
campioni(2) = "─"   : etichette(2) = "box-drawing"   ' 3 byte UTF-8
campioni(3) = "😀"  : etichette(3) = "emoji"         ' 4 byte UTF-8

PRINT "TESTO        LEN  LENB  PRIMO CARATTERE"
FOR i = 0 TO 3
  PRINT etichette(i); TAB(14); LEN(campioni(i)); TAB(19); LENB(campioni(i)); TAB(25); "["; LEFT$(campioni(i), 1); "]"
NEXT i

' Coerenza: il numero di caratteri restituiti da LEFT$ è quello che dice LEN.
PRINT
PRINT "roundtrip: "; LEFT$("😀x", LEN("😀")); "  (due caratteri: emoji + x)"
PRINT "instr emoji: "; INSTR("a😀b", "😀")
