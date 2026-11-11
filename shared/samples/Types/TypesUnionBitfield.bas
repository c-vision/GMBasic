' FILE: Types/TypesUnionBitfield.bas
' =================================================================================================
' GUIDA TEORICA: UNION E BITFIELD — MEMORIA CONDIVISA E CAMPI A LARGHEZZA DI BIT
' =================================================================================================
'
' Due costruzioni del linguaggio pensate per rispecchiare le strutture del C. Servono quando i
' dati arrivano da un formato binario (pacchetto di rete, registro di dispositivo, record a
' layout fisso) e i byte contano davvero.
'
' 1. UNION — CAMPI SOVRAPPOSTI
'    `UNION Nome ... END UNION` dichiara un tipo i cui campi **condividono la stessa memoria**:
'    tutti partono dall'offset 0 e la taglia del tipo è il MASSIMO delle taglie dei campi.
'    - Scrivere un campo e leggerne un altro restituisce la **reinterpretazione dei byte**
'      (little-endian), esattamente come in C.
'    - Una scrittura PARZIALE (un BYTE dentro un LONG) cambia solo i byte del campo scritto: gli
'      altri restano quelli di prima. È così che si leggono i "campi" di una word.
'    - Tipi ammessi (v1): quelli la cui larghezza in memoria è identica sulle 4 gambe —
'      BYTE/SBYTE (1 byte), LONG/ULONG/QUAD/UQUAD (8), DOUBLE (8). Le viste INTERMEDIE
'      (INTEGER/UINTEGER/SINGLE) sono escluse perché il backend IL memorizza quei numerici a 8
'      byte: l'aliasing byte-per-byte divergerebbe dalle altre gambe.
'    - Resta un **tipo valore**: `v = u` copia i byte, non condivide la variabile.
'
' 2. BITFIELD — CAMPI A n BIT (`... WIDTH n`)
'    Un campo di un TYPE/TYPE PACKED può dichiarare la propria larghezza in bit:
'        `A AS LONG WIDTH 3`
'    - Il valore viene mascherato a n bit e, per i tipi SIGNED (BYTE/SBYTE, INTEGER, LONG,
'      QUAD), esteso al segno: `int64_t A : 3` con A = 5 vale -3, con A = 9 vale 1.
'    - I bit si impacchettano LSB-first nell'unità di storage del tipo dichiarato (8 byte per
'      LONG/ULONG/QUAD/UQUAD, 2 per INTEGER/UINTEGER, 1 per BYTE/SBYTE): quando un campo non ci
'      sta nei bit residui si apre una nuova unità, come fa il compilatore C.
'    - `WIDTH` è ammesso solo su campi INTERI (un bitfield `double` non esiste in C).
'    - Un bitfield NON è ammesso dentro una UNION (v1) e non è ammesso nel marshalling FFI.
'
' 3. LAYOUT E DIMENSIONI
'    `SIZEOF(Tipo)` segue le regole C: per una UNION è il massimo dei campi; per i bitfield
'    conta l'unità intera; un `PACKED TYPE` non inserisce padding tra i campi. `UDTPACKED(x)`
'    continua a restituire il NUMERO di campi dichiarati.
'
' 4. QUANDO USARLI
'    - Leggere/scrivere un header di protocollo o un registro hardware senza funzioni di
'      conversione: si dichiara la struttura una volta e si accede ai campi per nome.
'    - Ispezionare il bit pattern di un double/float o i singoli byte di un intero.
'    - NON usarli per dati applicativi normali: per quello ci sono i TYPE ordinari (più chiari e
'      senza sovrapposizioni).
' =================================================================================================

' ------------------------------------------------------------------------------------------------
' UNION: una word vista come intero, come double e come singolo byte
' ------------------------------------------------------------------------------------------------
UNION Word
    L AS LONG
    D AS DOUBLE
    B AS BYTE
    SB AS SBYTE
END UNION

' ------------------------------------------------------------------------------------------------
' BITFIELD: header di un protocollo immaginario — 3+5+8+16+32 = 64 bit = UNA unità da 8 byte.
' Tutti i campi sono ULONG (unsigned): i flag non devono diventare negativi.
' ------------------------------------------------------------------------------------------------
TYPE PacketHeader
    VERSION AS ULONG WIDTH 3
    KIND AS ULONG WIDTH 5
    FLAGS AS ULONG WIDTH 8
    LENGTH AS ULONG WIDTH 16
    SEQ AS ULONG WIDTH 32
END TYPE

' Bitfield SIGNED: per mostrare maschera + estensione del segno (5 su 3 bit = -3)
TYPE SignedBits
    VALUE AS LONG WIDTH 3
END TYPE

' ------------------------------------------------------------------------------------------------
' Layout: gli stessi campi con e senza padding. Un bitfield BYTE occupa 1 byte; il LONG che segue
' sta a offset 8 nel TYPE ordinario (allineamento) e a offset 1 nel PACKED TYPE.
' ------------------------------------------------------------------------------------------------
TYPE MixedNatural
    SMALL AS BYTE WIDTH 3
    WIDE AS LONG
END TYPE

PACKED TYPE MixedPacked
    SMALL AS BYTE WIDTH 3
    WIDE AS LONG
END TYPE

' ------------------------------------------------------------------------------------------------
' SUB che scrive nell'header per riferimento: i bitfield sono campi come gli altri
' ------------------------------------------------------------------------------------------------
SUB EncodeHeader(BYREF h AS PacketHeader, version AS LONG, kind AS LONG, flags AS LONG, length AS LONG, seq AS LONG)
    h.VERSION = version
    h.KIND = kind
    h.FLAGS = flags
    h.LENGTH = length
    h.SEQ = seq
END SUB

' Traduce i flag in un nome leggibile (bit 0 SYN, bit 1 ACK, bit 2 FIN, bit 3 RST)
FUNCTION FlagNames(flags AS LONG) AS STRING
    DIM s AS STRING
    s = ""
    IF (flags AND 1) <> 0 THEN s = s + "SYN "
    IF (flags AND 2) <> 0 THEN s = s + "ACK "
    IF (flags AND 4) <> 0 THEN s = s + "FIN "
    IF (flags AND 8) <> 0 THEN s = s + "RST "
    IF s = "" THEN s = "-"
    RETURN s
END FUNCTION

SUB Main()
    PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TypesUnionBitfield - UNION e BITFIELD (memoria condivisa e campi a bit) ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
    PRINT

    PRINT "--- 1. UNION: la stessa memoria letta in modi diversi ---"
    DIM u AS Word
    u.L = 258
    PRINT "u.L = "; u.L; "  (0x0102)"
    PRINT "u.B = "; u.B; "   (byte basso)"
    PRINT "u.SB = "; u.SB; "  (lo stesso byte come signed)"
    PRINT
    PRINT "Scrivo u.B = 255: cambia SOLO il byte basso (0x0102 -> 0x01FF)"
    u.B = 255
    PRINT "u.B = "; u.B
    PRINT "u.SB = "; u.SB; "  (signed: 255 -> -1)"
    PRINT "u.L = "; u.L; "  (i byte alti sono rimasti quelli di prima)"
    PRINT
    PRINT "u.D = 1.0: i bit di un double letti come intero"
    u.D = 1
    PRINT "u.D = "; u.D
    PRINT "u.L = "; u.L; "  (bit pattern del double)"
    PRINT "u.B = "; u.B; "   (byte basso di 1.0 = 0)"
    PRINT
    PRINT "SIZEOF(Word) = "; SIZEOF(Word); "   campi dichiarati = "; UDTPACKED(u)
    PRINT

    PRINT "--- 2. BITFIELD: header di protocollo ---"
    DIM h AS PacketHeader
    PRINT "SIZEOF(PacketHeader) = "; SIZEOF(PacketHeader); "  (3+5+8+16+32 = 64 bit = 8 byte)"
    EncodeHeader(h, 3, 5, 2, 1500, 42)
    PRINT "VERSION = "; h.VERSION
    PRINT "KIND = "; h.KIND
    PRINT "FLAGS = "; h.FLAGS; "  ("; FlagNames(h.FLAGS); ")"
    PRINT "LENGTH = "; h.LENGTH
    PRINT "SEQ = "; h.SEQ
    PRINT
    PRINT "Assegno VERSION = 9: la maschera a 3 bit dà 9 AND 7 = 1"
    h.VERSION = 9
    PRINT "VERSION = "; h.VERSION
    PRINT

    DIM sb AS SignedBits
    sb.VALUE = 5
    PRINT "SignedBits.VALUE (LONG WIDTH 3) con 5 = "; sb.VALUE; "  (segno esteso: -3)"
    sb.VALUE = 9
    PRINT "SignedBits.VALUE con 9 = "; sb.VALUE; "  (9 AND 7 = 1)"
    PRINT

    PRINT "--- 3. Layout: PACKED TYPE e allineamento ---"
    DIM mn AS MixedNatural
    DIM mp AS MixedPacked
    mn.SMALL = 3 : mn.WIDE = 123
    mp.SMALL = 3 : mp.WIDE = 123
    PRINT "SIZEOF(MixedNatural) = "; SIZEOF(MixedNatural); "  (il LONG è allineato a 8)"
    PRINT "SIZEOF(MixedPacked)  = "; SIZEOF(MixedPacked); "  (nessun padding: 1 + 8)"
    PRINT "Valori: natural="; mn.WIDE; " packed="; mp.WIDE
    PRINT

    PRINT "--- 4. Array di header e copia per valore ---"
    DIM recs(2) AS PacketHeader
    EncodeHeader(recs(0), 1, 2, 1, 100, 1)
    EncodeHeader(recs(1), 2, 3, 6, 250, 2)
    EncodeHeader(recs(2), 3, 4, 3, 900, 3)
    DIM total AS LONG
    DIM i AS INTEGER
    FOR i = 0 TO 2
        PRINT "rec "; i; ": v="; recs(i).VERSION; " kind="; recs(i).KIND; " len="; recs(i).LENGTH; " flags="; FlagNames(recs(i).FLAGS)
        total = total + recs(i).LENGTH
    NEXT i
    PRINT "Somma delle lunghezze = "; total
    PRINT

    PRINT "Copia per valore: v = u, poi modifico u — v resta invariato"
    DIM v AS Word
    u.L = 7 : v = u
    u.L = 9
    PRINT "u.L = "; u.L; "   v.L = "; v.L
    PRINT
    PRINT "--- Test completato. ---"
END SUB
