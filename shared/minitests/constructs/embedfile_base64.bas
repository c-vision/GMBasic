' MiniTest CONSTRUCTS — EMBEDFILE ... AS BASE64: incorporare un payload BINARIO.
'
' La stringa del linguaggio è TESTO (LEN conta i caratteri, MID$/ASC decodificano UTF-8),
' quindi un file binario non si incorpora "così com'è": si incorpora la sua codifica
' Base64 (testo ASCII puro, identico sulle 4 gambe) e il programma la decodifica — con la
' funzione del pacchetto CRYPTOGRAPHY (vedi samples/Commands/EmbedFileBinary.bas) o con
' qualunque decoder dell'applicazione.
'
' Risorsa: shared/samples/Fixtures/embed_binary.bin = 41 00 FF 42  ->  "QQD/Qg=="
' La codifica è quella standard RFC 4648 con padding, la stessa di Convert.ToBase64String:
' un testo diverso fra le gambe sarebbe una divergenza.
' EXPECT: b64=QQD/Qg==|len= 8 |head=QQD/|end===|ascii=-1 |pad= 2 |alphabet=-1
EMBEDFILE "../../samples/Fixtures/embed_binary.bin" AS BASE64 b$

SUB Main()
    DIM q AS STRING
    DIM i AS LONG, asciiOnly AS LONG, inAlphabet AS LONG, pad AS LONG
    q = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

    PRINT "b64="; b$;
    PRINT "|len="; LEN(b$);
    PRINT "|head="; LEFT$(b$, 4);
    PRINT "|end="; RIGHT$(b$, 2);

    ' testo ASCII puro e dentro l'alfabeto Base64 (con padding finale)
    asciiOnly = -1 : inAlphabet = -1 : pad = 0
    FOR i = 1 TO LEN(b$)
        DIM c AS STRING, a AS LONG
        c = MID$(b$, i, 1)
        a = ASC(c)
        IF a >= 128 THEN asciiOnly = 0
        IF c = "=" THEN
            pad = pad + 1
        ELSE
            IF INSTR(q, c) = 0 THEN inAlphabet = 0
        END IF
    NEXT i
    PRINT "|ascii="; asciiOnly;
    PRINT "|pad="; pad;
    PRINT "|alphabet="; inAlphabet
END SUB
