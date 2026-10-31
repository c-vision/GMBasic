' EXPECT: 4|3|1|1|1|3
' LEN(s) = CARATTERI (code point Unicode), non byte e non unità UTF-16.
' Ascii, accento (2 byte), box-drawing (3 byte), emoji (4 byte), stringa vuota, forma qualificata.
PRINT LEN("abcd")
PRINT LEN("abc")
PRINT LEN("é")
PRINT LEN("─")
PRINT LEN("😀")
PRINT STRINGS.LEN("abc")
