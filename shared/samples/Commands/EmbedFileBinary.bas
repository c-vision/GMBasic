' ============================================================================
' EmbedFileBinary.bas — EMBEDFILE ... AS BASE64: portarsi dietro un payload BINARIO.
'
' Caso reale: un'applicazione che spedisce un'immagine/firma/archivio dentro il proprio
' eseguibile. La stringa del linguaggio è TESTO, quindi il payload si incorpora nella sua
' codifica Base64 (ASCII puro) e si decodifica all'avvio in un array di byte: il file
' originale NON serve più.
'
' Sintassi:  EMBEDFILE "<percorso>" AS BASE64 nome$
'   - qualunque file (byte 0x00/0xFF compresi): il compilatore lo codifica in Base64;
'   - il testo è ASCII puro, identico sulle 4 gambe (RFC 4648 con padding);
'   - decodifica: CRYPTOGRAPHY.FROMBASE64(testo, out() AS BYTE) -> numero di byte scritti.
'
' Il percorso è relativo alla cartella del programma (qui ../Fixtures).
' ============================================================================
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

' Il payload: 4 byte binari 41 00 FF 42 (fixture condivisa con il MiniTest).
EMBEDFILE "../Fixtures/embed_binary.bin" AS BASE64 payload$

SUB Main()
    PRINT "--- payload incorporato (Base64) ---"
    PRINT payload$
    PRINT "caratteri: "; LEN(payload$)

    ' Decodifica in un array di byte: la dimensione la dà il testo stesso (3 byte ogni 4
    ' caratteri, meno il padding).
    DIM n AS LONG, pad AS LONG, cap AS LONG
    pad = 0
    IF RIGHT$(payload$, 2) = "==" THEN
        pad = 2
    ELSEIF RIGHT$(payload$, 1) = "=" THEN
        pad = 1
    END IF
    cap = (LEN(payload$) / 4) * 3 - pad
    DIM data(cap - 1) AS BYTE
    n = CRYPTOGRAPHY.FROMBASE64(payload$, data)

    PRINT "--- decodificato ---"
    PRINT "byte: "; n; " (attesi "; cap; ")"
    DIM i AS LONG
    FOR i = 0 TO n - 1
        PRINT "b("; i; ")="; data(i); " ";
    NEXT i
    PRINT

    ' Prova di andata e ritorno sui BYTE: l'esadecimale dell'array decodificato deve essere
    ' quello del file originale (41 00 FF 42). Il confronto si fa sui byte, non sulle
    ' stringhe: CHR$(255) in una stringa è un carattere Unicode, non il byte 0xFF.
    PRINT "hex: "; CRYPTOGRAPHY.BYTESHEX(data)
    PRINT "roundtrip: "; (UCASE$(CRYPTOGRAPHY.BYTESHEX(data)) = "4100FF42")
    PRINT "primo byte: "; data(0); " | terzo: "; data(2)
END SUB
