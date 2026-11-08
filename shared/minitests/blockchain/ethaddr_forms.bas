' EXPECT: full=0x7e5f4552091a69125d5dfcb7b8c2659029395bdf|comp=0x7034adcca06964c0dffdb3eae70e8666faa5a63d|cks=0x7E5F4552091A69125d5DfCb7b8C2659029395Bdf|diversi=SI|stessaX=SI|pad33=SI|pad65=SI
' MiniTest BLOCKCHAIN (pacchetto FFI): GUARDIA sul comportamento di ETHADDRFROMPUB sulle DUE
' forme di chiave pubblica. Non e' una verifica "in astratto": fissa cio' che la lib fa OGGI,
' cosi' un cambiamento silenzioso diventa rosso invece di passare inosservato.
' Con priv = 1:
'   NON compressa (65 byte, 0x04 || X || Y) -> 0x7e5f4552...  = indirizzo corretto
'   COMPRESSA     (33 byte, 0x02/0x03 || X) -> 0x7034adcc...  = indirizzo DIVERSO
' Se in futuro ETHADDRFROMPUB cominciasse a decomprimere il punto, il valore della chiave
' completa resterebbe lo stesso ma quello della compressa cambierebbe e questo test andrebbe
' rosso: la modifica va allora accompagnata da una decisione esplicita (aggiornare la guardia
' e la sezione "Indirizzi" del README del pacchetto).
' La FORMA si deduce dal primo byte (0x04 = non compressa, 0x02/0x03 = compressa), non da
' LEN(pub): i byte in coda oltre la chiave NON entrano nell'hash. `pad33`/`pad65` lo fissano —
' la stessa chiave in un array sovradimensionato deve dare lo stesso indirizzo. Prima del fix
' un buffer piu' grande produceva in silenzio un ALTRO indirizzo.
' `stessaX=SI` documenta il perche': le due forme descrivono lo STESSO punto (X identica),
' cambia solo la codifica — quindi un indirizzo diverso non e' un errore di calcolo, e' la
' lib che non converte fra le due forme.
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

' L'array di parametro attraversa il confine FFI anche come argomento di FUNCTION.
FUNCTION EthDa(p() AS BYTE) AS STRING
  RETURN ETHADDRFROMPUB(p)
END FUNCTION

SUB DammiFull(priv() AS BYTE, o() AS BYTE)
  DIM n AS LONG
  n = GETFULLPUBLICKEY(priv, o)
END SUB

SUB DammiComp(priv() AS BYTE, o() AS BYTE)
  DIM n AS LONG
  n = GETPUBLICKEY(priv, o)
END SUB

SUB Main()
  DIM priv(31) AS BYTE
  priv(31) = 1
  DIM k65(64) AS BYTE
  DIM k33(32) AS BYTE
  DammiFull(priv, k65)
  DammiComp(priv, k33)
  DIM eth65$ AS STRING
  DIM eth33$ AS STRING
  eth65$ = EthDa(k65)
  eth33$ = EthDa(k33)
  PRINT "full="; eth65$
  PRINT "comp="; eth33$
  PRINT "cks="; ETHADDRCHECKSUM(eth65$)
  IF eth65$ <> eth33$ THEN
    PRINT "diversi=SI"
  ELSE
    PRINT "diversi=NO"
  END IF
  IF MID(TOHEX(k65), 3, 64) = MID(TOHEX(k33), 3, 64) THEN
    PRINT "stessaX=SI"
  ELSE
    PRINT "stessaX=NO"
  END IF
  ' Buffer SOVRADIMENSIONATI: la stessa chiave dentro un array piu' grande (zeri in coda)
  ' deve dare lo STESSO indirizzo. E' la regressione del bug "buffer piu' grande del
  ' necessario -> indirizzo diverso, senza nessun errore".
  DIM pad33(63) AS BYTE
  DIM pad65(69) AS BYTE
  DIM i AS LONG
  FOR i = 0 TO 32
    pad33(i) = k33(i)
  NEXT i
  FOR i = 0 TO 64
    pad65(i) = k65(i)
  NEXT i
  IF EthDa(pad33) = eth33$ THEN
    PRINT "pad33=SI"
  ELSE
    PRINT "pad33=NO"
  END IF
  IF EthDa(pad65) = eth65$ THEN
    PRINT "pad65=SI"
  ELSE
    PRINT "pad65=NO"
  END IF
END SUB
