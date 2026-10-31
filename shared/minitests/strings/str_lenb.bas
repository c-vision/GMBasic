' EXPECT: 3|2|3|4|4|0
' LENB(s) = BYTE della codifica UTF-8 (LEN conta i CARATTERI: code point).
' Copre ASCII (1 byte), accento (2), box-drawing (3), emoji (4) e stringa vuota.
' Disponibile su tutte e 4 le gambe; forma qualificata e nuda sono equivalenti.
PRINT LENB("abc")
PRINT LENB("é")
PRINT LENB("─")
PRINT LENB("😀")
PRINT STRINGS.LENB("😀")
PRINT LENB("")
