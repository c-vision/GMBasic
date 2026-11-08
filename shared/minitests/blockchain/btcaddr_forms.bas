' EXPECT: b33main=1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH|b33test=mrCDrCybB6J1vRfbwM5hemdJz73FwDBC8r|b65main=1EHNa6Q4Jz2uvNExL497mE43ikXhwF6kZm|b65test=mtoKs9V381UAhUia3d7Vb9GNak8Qvmcsme|diversi=SI|verMain=SI|verTest=SI|pad33=SI|pad65=SI
' MiniTest BLOCKCHAIN (pacchetto FFI): GUARDIA su BTCADDRFROMPUB sulle DUE forme di chiave.
' Come la guardia ETH: fissa cio' che la lib fa OGGI, cosi' un cambio silenzioso diventa rosso.
' Con priv = 1 (generatore di secp256k1), buffer dimensionati ESATTAMENTE (33 e 65 byte):
'   NON compressa (65 byte, 0x04 || X || Y) -> 1EHNa6Q4...  (mainnet) = indirizzo DIVERSO
'   COMPRESSA     (33 byte, 0x02/0x03 || X) -> 1BgGZ9tc...  (mainnet) = corretto, e' la forma
'                                              che DERIVEKEY/GETPUBLICKEY restituiscono
' BTCADDRFROMPUB non converte fra le due forme e non indovina quella giusta: la FORMA la deduce
' dal primo byte (0x04 / 0x02-0x03) e il byte di testa resta nei dati hashati, mentre i byte in
' coda oltre la chiave NON entrano nell'hash (`pad33`/`pad65` lo fissano). Se un domani la lib
' cominciasse a normalizzare la chiave, il valore b65 cambierebbe e questo test andrebbe rosso:
' la modifica va accompagnata da una decisione esplicita (guardia + README del pacchetto).
' verMain/verTest fissano anche il byte di versione dell'indirizzo (0x00 mainnet, 0x6F testnet).
REQUIRE "blockchain"
IMPORT BLOCKCHAIN
IMPORT CRYPTOGRAPHY
IMPORT STRINGS

' L'array di parametro attraversa l'FFI anche come argomento di FUNCTION.
FUNCTION BtcDa(p() AS BYTE, testnet AS LONG) AS STRING
  RETURN BTCADDRFROMPUB(p, testnet)
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
  DIM b33main$ AS STRING
  DIM b33test$ AS STRING
  DIM b65main$ AS STRING
  DIM b65test$ AS STRING
  b33main$ = BtcDa(k33, FALSE)
  b33test$ = BtcDa(k33, TRUE)
  b65main$ = BtcDa(k65, FALSE)
  b65test$ = BtcDa(k65, TRUE)
  PRINT "b33main="; b33main$
  PRINT "b33test="; b33test$
  PRINT "b65main="; b65main$
  PRINT "b65test="; b65test$
  IF b33main$ <> b65main$ THEN
    PRINT "diversi=SI"
  ELSE
    PRINT "diversi=NO"
  END IF
  IF LEFT(b33main$, 1) = "1" THEN
    PRINT "verMain=SI"
  ELSE
    PRINT "verMain=NO"
  END IF
  IF LEFT(b33test$, 1) = "m" OR LEFT(b33test$, 1) = "n" THEN
    PRINT "verTest=SI"
  ELSE
    PRINT "verTest=NO"
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
  IF BtcDa(pad33, FALSE) = b33main$ THEN
    PRINT "pad33=SI"
  ELSE
    PRINT "pad33=NO"
  END IF
  IF BtcDa(pad65, FALSE) = b65main$ THEN
    PRINT "pad65=SI"
  ELSE
    PRINT "pad65=NO"
  END IF
END SUB
