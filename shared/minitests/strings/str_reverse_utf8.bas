' Minitest STRINGS — REVERSE inverte CARATTERI (code point), non byte.
' "città" (c i t t à) invertito è "àttic": prima si invertivano i BYTE e la 'à'
' (2 byte UTF-8) veniva spezzata, producendo una stringa non valida.
' Cfr. Stringhe-Guida-Utente: LEN/LEFT$/MID$/RIGHT$/INSTR contano CARATTERI.
' EXPECT: acuta=àttic|lacuta= 5|box=┐─┌|lbox= 3|emoji=🙂b😀|lemoji= 3|ascii=cba|empty=|uno=z
DIM s AS STRING

s = "città"
PRINT "acuta="; STRINGS.REVERSE(s)
PRINT "lacuta="; LEN(STRINGS.REVERSE(s))

s = "┌─┐"
PRINT "box="; STRINGS.REVERSE(s)
PRINT "lbox="; LEN(STRINGS.REVERSE(s))

s = "😀b🙂"
PRINT "emoji="; STRINGS.REVERSE(s)
PRINT "lemoji="; LEN(STRINGS.REVERSE(s))

PRINT "ascii="; STRINGS.REVERSE("abc")
PRINT "empty="; STRINGS.REVERSE("")
PRINT "uno="; STRINGS.REVERSE("z")