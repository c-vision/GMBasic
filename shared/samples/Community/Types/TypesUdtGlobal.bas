' FILE: Types/TypesUdtGlobal.bas
' =================================================================================================
'                                GUIDA TEORICA: TIPI DEFINITI DALL'UTENTE (UDT)
' =================================================================================================
'
' Un UDT (User-Defined Type), dichiarato con `TYPE...END TYPE`, permette di creare strutture dati
' personalizzate raggruppando più variabili (campi) in una singola entità.
'
' 1. NATURA (TIPO VALORE)
'    Gli UDT in GM-BASIC sono "tipi valore" (Value Types). Questo ha un'implicazione fondamentale:
'    Quando un UDT viene assegnato a un'altra variabile (`P2 = P1`), viene creata una **copia completa**
'    dei dati. Modificare la copia (`P2`) non influisce sull'originale (`P1`).
'
' 2. PASSAGGIO AI SOTTOPROGRAMMI
'    - BYVAL (Predefinito): Passa una copia dell'UDT. La procedura riceve una sua istanza privata
'      e non può modificare la variabile originale del chiamante.
'    - BYREF: Passa un riferimento all'UDT originale. La procedura può modificare direttamente
'      i campi della variabile del chiamante.
'
' 3. CAMPI E ACCESSO
'    I membri di un UDT sono chiamati "campi" (fields) e vi si accede tramite l'operatore punto (`.`).
'    Es: `MiaPersona.Nome = "Mario"`
'
' 4. LIMITAZIONI (RISPETTO ALLE CLASSI)
'    A differenza delle Classi, gli UDT tradizionali non supportano l'ereditarietà. Sono pensati
'    per aggregare dati in modo semplice e performante.
'
' =================================================================================================

' Definizione di un UDT globale, visibile in tutto il modulo.
TYPE Persona
  Nome AS STRING * 15
  Cognome AS STRING * 20
  Eta AS INTEGER
  Attivo AS BIT
END TYPE

' Funzione che restituisce un'istanza di un UDT.
FUNCTION GetPersonaDefault() AS Persona
  DIM P AS Persona
  P.Nome = "Valore"
  P.Cognome = "Default"
  P.Eta = 99
  P.Attivo = 0 // False
  GetPersonaDefault = P // Ritorno per assegnazione
END FUNCTION

' Subroutine che modifica un UDT passato per riferimento (BYREF).
' Qualsiasi modifica a 'P' qui dentro si rifletterà sulla variabile originale passata.
SUB ModificaPersona(BYREF P AS Persona, NuovoNome AS STRING)
  PRINT
  PRINT "--- Dentro SUB ModificaPersona (BYREF) ---"
  PRINT "  Stato iniziale: "; P.Nome; P.Cognome; P.Eta
  P.Nome = NuovoNome
  P.Cognome = "Modificato"
  P.Eta = P.Eta + 1
  PRINT "  Stato finale: "; P.Nome; P.Cognome; P.Eta
  PRINT "--- Uscita da SUB ---"
END SUB

' Subroutine che riceve un UDT per valore (BYVAL implicito).
' Riceve una copia e non può modificare l'originale.
SUB StampaPersona(BYVAL P AS Persona)
   PRINT "  Nome: '"; P.Nome; "'"
   PRINT "  Cognome: '"; P.Cognome; "'"
   PRINT "  Eta:"; P.Eta
   PRINT "  Attivo:"; P.Attivo
END SUB

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ TypesUdtGlobal - Tipi Definiti Utente (UDT)                             ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT

20 PRINT "--- 1. Test Variabile UDT Semplice ---"
' Dichiarazione di una variabile di tipo Persona
30 DIM P1 AS Persona
' Accesso e assegnazione ai campi
40 P1.Nome = "Mario"
50 P1.Cognome = "Rossi"
60 P1.Eta = 42
70 P1.Attivo = -1 //True
80 StampaPersona(P1)
90 PRINT

100 PRINT "--- 2. Test Assegnazione e Comparazione UDT ---"
110 DIM P2 AS Persona
' Assegnazione: P2 diventa una COPIA completa di P1.
120 P2 = P1
' L'operatore '=' su UDT confronta il valore di ogni campo.
130 PRINT "P1 e P2 sono uguali? (Atteso: True) -->"; (P1 = P2)
140 P2.Nome = "Luigi" // Modifico solo la copia (P2)
150 PRINT "Modifico P2. P1 e P2 sono uguali? (Atteso: False) -->"; (P1 = P2)
160 PRINT "Stampa di P1 (non deve essere cambiato):"
170 StampaPersona(P1)
180 PRINT "Stampa di P2 (modificato):"
190 StampaPersona(P2)
200 PRINT

210 PRINT "--- 3. Test UDT come Ritorno da Funzione ---"
220 DIM P3 AS Persona
' Anche qui, l'assegnazione crea una copia del valore restituito dalla funzione.
230 P3 = GetPersonaDefault()
240 PRINT "Stampa P3 (da funzione):"
250 StampaPersona(P3)
260 PRINT

270 PRINT "--- 4. Test Passaggio BYREF a SUB ---"
280 PRINT "Stato di P1 prima della chiamata:"
290 StampaPersona(P1)
' Passiamo P1 per riferimento. La SUB 'ModificaPersona' opererà sull'originale.
300 ModificaPersona(P1, "Mario Nuovo")
310 PRINT "Stato di P1 dopo la chiamata (dovrebbe essere cambiato):"
320 StampaPersona(P1)
330 PRINT

340 PRINT "--- 5. Test Array di UDT ---"
' Dichiarazione di un array i cui elementi sono di tipo Persona.
350 DIM Elenco(2) AS Persona
360 PRINT "Assegno valori a Elenco(1)..."
370 Elenco(1).Nome = "Array"
380 Elenco(1).Cognome = "Elemento Uno"
390 Elenco(1).Eta = 1
400 PRINT "Stampa di Elenco(0) (default):"
410 StampaPersona(Elenco(0))
420 PRINT "Stampa di Elenco(1) (assegnato):"
430 StampaPersona(Elenco(1))
440
' Anche un elemento di un array può essere passato BYREF.
450 PRINT "Modifico Elenco(1) con la SUB BYREF..."
460 ModificaPersona(Elenco(1), "Array Mod.")
470 PRINT "Stampa di Elenco(1) dopo la modifica:"
480 StampaPersona(Elenco(1))
490 PRINT

500 PRINT "--- Test UDT completato ---"
510 END