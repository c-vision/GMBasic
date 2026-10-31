' Minitest STRINGS — LEN = CARATTERI (code point), LENB = BYTE UTF-8, sottostringhe per carattere.
' Le gambe C contavano i BYTE (LEN("┌─┐")=9) e le managed le unità UTF-16 (LEN("😀")=2):
' ora tutte contano i CARATTERI, e per i byte c'è LENB. Vedi
' Bugs/(solved) - LEN-UTF8-Byte-VS-Char-Gambe-C.
' EXPECT: len= 3|lenb= 9|left=┌─|mid=─|right=─┐|instr= 3|instr1= 1|accent= 1|accentb= 2|emoji= 1|emojib= 4|eleft=😀|einstr= 1|ascii= 3
DIM s AS STRING
DIM t AS STRING
s = "┌─┐"
PRINT "len="; LEN(s)
PRINT "lenb="; LENB(s)
PRINT "left="; LEFT$(s, 2)
PRINT "mid="; MID$(s, 2, 1)
PRINT "right="; RIGHT$(s, 2)
PRINT "instr="; INSTR(s, "┐")
PRINT "instr1="; INSTR(s, "┌")
t = "é"
PRINT "accent="; LEN(t)
PRINT "accentb="; LENB(t)
t = "😀"
PRINT "emoji="; LEN(t)
PRINT "emojib="; LENB(t)
PRINT "eleft="; LEFT$(t, 1)
PRINT "einstr="; INSTR(t, "😀")
PRINT "ascii="; LEN("abc")
