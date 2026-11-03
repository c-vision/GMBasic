' FILE: Bits/ProtocolloPacchetti.bas
' SCENARIO DIDATTICO — leggere l'header di un protocollo binario e verificarne l'integrità.
'
' Un'apparecchiatura invia pacchetti con un header compresso in un solo intero a 16 bit:
'   [15..8] versione | [7..4] tipo di messaggio | [3..0] flag
' Il programma mostra il lavoro tipico di chi scrive un driver o un parser di protocollo:
'   1) impacchettare e spacchettare i campi (BITFIELD con estremi inclusi);
'   2) verificare l'integrità con la parità e la distanza di Hamming fra pacchetti;
'   3) contatore di Gray (due valori consecutivi differiscono di un solo bit);
'   4) indirizzo spaziale Morton di una griglia (x,y) → indice lineare;
'   5) cifrario elementare a rotazione e serializzazione big/little endian;
'   6) capacità del buffer arrotondata alla potenza di 2 e ricerca del bit più alto.
'
' Domini: BITS + MATRIX-free base. Output deterministico, nessun I/O.
IMPORT BITS

' Header: versione (8 bit), tipo (4 bit), flag (4 bit).
FUNCTION Impacchetta(versione AS LONG, tipo AS LONG, flag AS LONG) AS LONG
    RETURN (versione * 256) + (tipo * 16) + flag
END FUNCTION

FUNCTION Campo(header AS LONG, da AS LONG, a AS LONG) AS LONG
    RETURN BITS.BITFIELD(header, da, a)
END FUNCTION

SUB Main()
    ' ── 1. Impacchettare e rileggere i campi ─────────────────────────────────
    PRINT "== 1. Header del pacchetto =="
    DIM h AS LONG
    h = Impacchetta(3, 7, 5)
    PRINT "header: "; h
    PRINT "versione: "; Campo(h, 8, 15)
    PRINT "tipo:     "; Campo(h, 4, 7)
    PRINT "flag:     "; Campo(h, 0, 3)
    ' BITFIELD scambia gli estremi se arrivano invertiti: utile per non sbagliare.
    PRINT "campo con estremi invertiti: "; Campo(h, 7, 4)

    ' ── 2. Integrità dei pacchetti ───────────────────────────────────────────
    ' Quattro header consecutivi: la parità dice se il numero di bit accesi è pari,
    ' la distanza di Hamming dice quanto due pacchetti sono diversi.
    PRINT "== 2. Integrità =="
    DIM pacchetti(3) AS LONG
    pacchetti(0) = Impacchetta(1, 2, 3)
    pacchetti(1) = Impacchetta(3, 0, 0)
    pacchetti(2) = Impacchetta(2, 7, 5)
    pacchetti(3) = Impacchetta(0, 1, 15)
    DIM i AS LONG
    DIM parita AS LONG
    DIM distanza AS LONG
    parita = 0
    distanza = 0
    FOR i = 0 TO 3
        parita = parita + BITS.PARITY(pacchetti(i))
        IF i < 3 THEN distanza = distanza + BITS.HAMMINGDIST(pacchetti(i), pacchetti(i + 1))
    NEXT i
    PRINT "somma delle parità: "; parita
    PRINT "distanza totale fra pacchetti consecutivi: "; distanza
    PRINT "un pacchetto è identico a se stesso: "; BITS.HAMMINGDIST(h, h)

    ' ── 3. Contatore di Gray ─────────────────────────────────────────────────
    ' Nella rotazione di un encoder due posizioni adiacenti devono differire di UN
    ' bit: è la proprietà che rende il codice di Gray immune agli errori di lettura.
    PRINT "== 3. Codice di Gray =="
    DIM violazioni AS LONG
    violazioni = 0
    FOR i = 0 TO 14
        IF BITS.FROMGRAY(BITS.GRAYCODE(i)) <> i THEN violazioni = violazioni + 1
        IF BITS.HAMMINGDIST(BITS.GRAYCODE(i), BITS.GRAYCODE(i + 1)) <> 1 THEN violazioni = violazioni + 1
    NEXT i
    PRINT "violazioni su 0..15: "; violazioni
    PRINT "Gray(5) = "; BITS.GRAYCODE(5); ", da Gray(7) si torna a "; BITS.FROMGRAY(7)

    ' ── 4. Indirizzo spaziale Morton ─────────────────────────────────────────
    ' Una griglia si può numerare per righe oppure "a Z" (Morton): celle vicine in
    ' mappa restano vicine anche in memoria, e l'indice si ricava dai bit di x e y.
    PRINT "== 4. Indirizzamento Morton =="
    DIM errori AS LONG
    DIM x AS LONG
    DIM y AS LONG
    errori = 0
    FOR y = 0 TO 3
        FOR x = 0 TO 3
            DIM indice AS LONG
            indice = BITS.BITINTERLEAVE(x, y)
            IF BITS.BITDEINTERLEAVE(indice, 0) <> x THEN errori = errori + 1
            IF BITS.BITDEINTERLEAVE(indice, 1) <> y THEN errori = errori + 1
        NEXT x
    NEXT y
    PRINT "errori di andata e ritorno su 16 celle: "; errori
    PRINT "cella (3,5) -> indice "; BITS.BITINTERLEAVE(3, 5); _
          " -> x="; BITS.BITDEINTERLEAVE(39, 0); " y="; BITS.BITDEINTERLEAVE(39, 1)

    ' ── 5. Rotazione e ordine dei byte ───────────────────────────────────────
    ' Un cifrario da campo: si invertono i bit, si ruota a sinistra, e per decifrare
    ' si ruota a destra e si inverte di nuovo. BYTESWAP converte big/little endian.
    PRINT "== 5. Rotazione e byte order =="
    DIM chiaro AS LONG
    chiaro = 202
    DIM cifrato AS LONG
    cifrato = BITS.BITROTATEL(BITS.BITREVERSE(chiaro, 8), 3, 8)
    DIM decifrato AS LONG
    decifrato = BITS.BITREVERSE(BITS.BITROTATER(cifrato, 3, 8), 8)
    PRINT "chiaro: "; chiaro; ", cifrato: "; cifrato; ", decifrato: "; decifrato
    PRINT "round-trip corretto: "; (decifrato = chiaro)
    PRINT "byteswap due volte è l'identità: "; (BITS.BYTESWAP(BITS.BYTESWAP(12345)) = 12345)
    PRINT "byteswap(256) = "; BITS.BYTESWAP(256)

    ' ── 6. Capacità del buffer e bit di riferimento ──────────────────────────
    ' Il buffer deve contenere 1000 byte: si arrotonda alla potenza di 2 superiore e
    ' si usa il bit più alto (BITSCANREV) per dimensionare gli indici.
    PRINT "== 6. Capacità =="
    DIM bisogno AS LONG
    bisogno = 1000
    DIM capacita AS LONG
    capacita = BITS.NEXTPOW2(bisogno)
    PRINT "buffer: "; capacita; " byte (potenza di 2: "; BITS.ISPOW2(capacita); ")"
    PRINT "1000 è una potenza di 2? "; BITS.ISPOW2(bisogno)
    PRINT "bit più basso di 40: "; BITS.BITSCANFWD(40); _
          ", più alto: "; BITS.BITSCANREV(40)
    PRINT "su zero non esiste: "; BITS.BITSCANFWD(0); " "; BITS.BITSCANREV(0)
END SUB
