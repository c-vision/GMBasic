' FILE: Security/ValidationDemo.bas
' =================================================================================================
'       VALIDATION — STRINGS.ISREQUIRED/ISEMAIL/ISURL/ISCREDITCARD/ISLENGTH
' =================================================================================================
' Funzioni booleane pure (core condiviso C#+C, 2026-09-02) per la validazione di input/form.
' Eseguibile su IL, PCodeVM e VM/C (C99Native: gap dichiarato).

IMPORT STRINGS

PRINT "=== Validation ==="

IF STRINGS.ISREQUIRED("abc") THEN PRINT "REQ OK" ELSE PRINT "REQ FAIL"
IF NOT STRINGS.ISREQUIRED("   ") THEN PRINT "REQ EMPTY OK" ELSE PRINT "REQ EMPTY FAIL"

IF STRINGS.ISEMAIL("a@b.co") THEN PRINT "EMAIL OK" ELSE PRINT "EMAIL FAIL"
IF NOT STRINGS.ISEMAIL("not-an-email") THEN PRINT "EMAIL NEG OK" ELSE PRINT "EMAIL NEG FAIL"

IF STRINGS.ISURL("https://example.com") THEN PRINT "URL OK" ELSE PRINT "URL FAIL"

IF STRINGS.ISCREDITCARD("4111111111111111") THEN PRINT "CC OK" ELSE PRINT "CC FAIL"
IF NOT STRINGS.ISCREDITCARD("1234567890123456") THEN PRINT "CC NEG OK" ELSE PRINT "CC NEG FAIL"

IF STRINGS.ISLENGTH("abc", 2, 5) THEN PRINT "LEN OK" ELSE PRINT "LEN FAIL"
IF NOT STRINGS.ISLENGTH("abcdef", 1, 3) THEN PRINT "LEN NEG OK" ELSE PRINT "LEN NEG FAIL"

PRINT "ValidationDemo done"
