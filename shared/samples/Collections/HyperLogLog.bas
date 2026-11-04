' FILE: Collections/HyperLogLog.bas
' =================================================================================================
'       HYPERLOGLOG: CONTARE GLI ELEMENTI DISTINTI CON MEMORIA FISSA
' =================================================================================================
'
' Un HASHSET conta esattamente gli elementi distinti ma consuma memoria proporzionale al numero
' di elementi (O(n)). Un HLL stima la cardinalità con memoria FISSA (p=14 → 16 KiB, errore
' atteso ≈ 0.81%) e due sketch si possono UNIRE senza perdere informazione (merge registro per
' registro): è il tipo giusto per metriche su flussi enormi o distribuite.
'
' Metodi: Add(v) · Count() · Merge(altro) · Error() · Precision() · Serialize() · Load(s) · Clear()
' TYPENAME(h) = "HLL" ; disponibile su IL, PCode, VM/C e C99Native (stessa semantica).

IMPORT COLLECTIONS

SUB Main()
  CLS
  PRINT "=== HLL: cardinalità stimata con memoria fissa ==="
  PRINT

  ' ---------------------------------------------------------------------------------------------
  ' 1. Visitatori distinti di tre giorni (ogni giorno un HLL; il totale è l'UNIONE dei tre)
  ' ---------------------------------------------------------------------------------------------
  PRINT "--- 1. Tre giorni, poi merge ---"
  DIM giorno1 AS HLL
  DIM giorno2 AS HLL
  DIM giorno3 AS HLL
  DIM i AS INTEGER

  ' 1.200 visite il giorno 1, ma solo 400 utenti distinti (id 1..400, ognuno 3 volte)
  FOR i = 1 TO 1200
    giorno1.Add("utente-" + STR((i - 1) MOD 400 + 1))
  NEXT
  ' 300 utenti distinti nuovi (401..700)
  FOR i = 401 TO 700
    giorno2.Add("utente-" + STR(i))
  NEXT
  ' 300 utenti: 401..700 (già visti) + 701..800 (nuovi)
  FOR i = 401 TO 800
    giorno3.Add("utente-" + STR(i))
  NEXT

  PRINT "Giorno 1 (400 distinti su 1200 visite): "; giorno1.Count()
  PRINT "Giorno 2 (300 nuovi):                  "; giorno2.Count()
  PRINT "Giorno 3 (400 visti, 300 già noti):    "; giorno3.Count()

  ' Unione (merge in-place): il totale deve essere 800 utenti distinti
  giorno1.Merge(giorno2)
  giorno1.Merge(giorno3)
  PRINT "Totale unione (atteso 800):           "; giorno1.Count()
  PRINT "Errore atteso:                        "; giorno1.Error()
  PRINT "Precisione (registri = 2^p):          "; giorno1.Precision()
  PRINT

  ' ---------------------------------------------------------------------------------------------
  ' 2. Confronto con il conteggio ESATTO (HASHSET) su un insieme piccolo
  ' ---------------------------------------------------------------------------------------------
  PRINT "--- 2. Confronto con HASHSET (esatto) su 500 elementi ---"
  DIM esatto AS HASHSET OF STRING
  DIM stima AS HLL
  FOR i = 1 TO 500
    esatto.Add("k" + STR(i))
    stima.Add("k" + STR(i))
  NEXT
  PRINT "HASHSET (esatto): "; esatto.Count()
  PRINT "HLL   (stimato): "; stima.Count()
  PRINT

  ' ---------------------------------------------------------------------------------------------
  ' 3. Persistenza: serializzazione e ripristino (per salvare lo sketch su file/DB)
  ' ---------------------------------------------------------------------------------------------
  PRINT "--- 3. Serialize / Load ---"
  DIM blob AS STRING
  blob = stima.Serialize()
  DIM ripristinato AS HLL
  ripristinato.Load(blob)
  PRINT "Dopo il round-trip: "; ripristinato.Count()
  ' NB: lo sketch serializzato è BINARIO (contiene NUL): non è testo, quindi non si usa LEN()
  ' (LEN conta i caratteri e si fermerebbe al primo NUL). La dimensione è nota dalla precisione:
  ' 1 registro per 2^p valori → 2^14 = 16384 byte + 9 byte di header.
  PRINT "Dimensione sketch:  ~"; 1 + 1 + (2 ^ ripristinato.Precision()); " byte circa"
  PRINT

  ' ---------------------------------------------------------------------------------------------
  ' 4. Duplicati e valori equivalenti (1 e 1.0 sono lo STESSO valore; "1" è un altro valore)
  ' ---------------------------------------------------------------------------------------------
  PRINT "--- 4. Payload canonico ---"
  DIM dup AS HLL
  dup.Add(1)
  dup.Add(1.0)
  dup.Add(1)
  PRINT "1, 1.0, 1 (stesso valore): "; dup.Count()
  dup.Add("1")
  PRINT "aggiungendo la STRINGA ""1"": "; dup.Count()
  PRINT

  ' ---------------------------------------------------------------------------------------------
  ' 5. Reset
  ' ---------------------------------------------------------------------------------------------
  PRINT "--- 5. Clear ---"
  dup.Clear()
  PRINT "Dopo Clear: "; dup.Count()
  PRINT
  PRINT "TYPENAME: "; TYPENAME(giorno1)
  PRINT
  PRINT "Completato."
END SUB
