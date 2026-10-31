' FILE: Scopes/ScopesLocal.bas
' ======================================================================================================================
' GUIDA TEORICA: TEST DI REGRESSIONE PER SCOPE E ASSEGNAZIONI
' ======================================================================================================================
' Questo file serve come test di regressione completo per assicurare che le complesse regole di scope di
' GM-BASIC funzionino correttamente in vari scenari, specialmente dopo modifiche al compilatore.
'
' CONCETTI CHIAVE TESTATI:
'
' 1. SCOPE DI BLOCCO (`FOR`, `IF`):
'    - Una variabile dichiarata con `LOCAL` all'interno di un blocco `FOR` o `IF` (`LoopVar`, `IfVar`)
'      deve esistere solo all'interno di quel blocco. Tentare di accedervi dopo `NEXT` o `END IF`
'      deve causare un errore.
'
' 2. CREAZIONE IMPLICITA vs. SHADOWING:
'    - `C.Nome = "Nuovo"`: Dopo `END IF`, la variabile `C` non esiste più. Questa assegnazione
'      ne crea una NUOVA, ma nello scope della procedura `TestIfScope`, non in quello globale.
'    - `LET GlobalVar = 20` in `TestLetVsImplicit`: La keyword `LET` (o un'assegnazione implicita)
'      su una variabile che ha lo stesso nome di una globale crea una nuova variabile *locale* che
'      "nasconde" (shadows) quella globale solo per la durata della procedura. La variabile globale
'      originale rimane inalterata.
'
' 3. VARIABILI STATICHE (`STATIC`):
'    - Una variabile dichiarata con `STATIC` all'interno di una procedura (`Counter`) viene inizializzata
'      solo la prima volta che la procedura viene chiamata.
'    - Il suo valore *persiste* tra le chiamate successive, a differenza di una variabile `LOCAL`
'      (`LocalVar`) che viene ricreata e reinizializzata ad ogni chiamata.
'
' 4. ACCESSO A GLOBALI E UDT:
'    - Il test verifica che le procedure possano leggere e modificare simboli globali (come `GlobalVar`
'      e l'array `ElencoContatti`) definiti a livello di modulo.
'    - Viene testato l'accesso ai campi di un User-Defined Type (`Contatto`) sia per variabili locali
'      che globali.
' ======================================================================================================================

' Definizione di un tipo di dato strutturato (User-Defined Type) a livello di modulo.
' Sarà visibile a tutte le procedure in questo file.
TYPE Contatto
  ID AS LONG
  Nome AS STRING * 20
END TYPE

' Dichiarazione di un array globale di UDT.
' GLOBAL garantisce che sia accessibile da qualsiasi punto del programma (dopo il linking).
GLOBAL ElencoContatti(2) AS Contatto

' Punto di ingresso principale del programma.
SUB Main()
  CLS
  PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ ScopesLocal - Regressione Scope, UDT, e Array                           ║"
  PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
  PRINT

  ' Dichiara una variabile globale dall'interno di una SUB.
  ' Grazie all'hoisting, sarà visibile a tutte le altre SUB, anche se definite prima.
  GLOBAL GlobalVar = 1000
  PRINT "Valore iniziale di GlobalVar ="; GlobalVar
  PRINT

  ' Esegue le varie subroutine di test.
  TestForScope()
  TestIfScope()
  TestStatic()
  TestLetVsImplicit()
  TestArrayScopes()

  PRINT "--- Verifica Finale nello Scope Main ---"
  ' GlobalVar non dovrebbe essere stata modificata da TestLetVsImplicit, che ha creato una sua versione locale.
  PRINT "Valore di GlobalVar dopo tutte le chiamate: "; GlobalVar; " (Atteso: 1000)"
  PRINT "ImplicitGlobal è locale alla SUB e non accessibile qui (comportamento corretto)."
  ' ElencoContatti è stato passato implicitamente per riferimento (essendo globale) e modificato da TestArrayScopes.
  PRINT "ElencoContatti(0).Nome (modificato da SUB): '"; ElencoContatti(0).Nome; "' (Atteso: 'GLOBAL MODIFICATO')"
  PRINT
  PRINT "--- TEST DI REGRESSIONE COMPLETATO ---"
END SUB

' Testa lo scope di un blocco FOR.
SUB TestForScope()
  PRINT "--- 1. Test Scope Blocco FOR...NEXT ---"
  FOR I = 1 TO 1
    ' LoopVar esiste solo tra FOR e NEXT.
    LOCAL LoopVar = 10
    PRINT "  Dentro il loop: LoopVar ="; LoopVar; "(Atteso: 10)"
  NEXT I
  PRINT "  Dopo il loop, LoopVar (dichiarata con LOCAL) non esiste più."
  REM PRINT LoopVar 
  PRINT
END SUB

' Testa lo scope di un blocco IF e la creazione implicita post-blocco.
SUB TestIfScope()
  PRINT "--- 2. Test Scope Blocco IF...THEN ---"
  IF TRUE THEN
    ' IfVar e la prima 'C' esistono solo qui dentro.
    LOCAL IfVar = 20
    DIM C AS Contatto
    C.Nome = "Test"
    PRINT "  Dentro IF: IfVar ="; IfVar; "(Atteso: 20)"
    PRINT "  Dentro IF: C.Nome = '"; C.Nome; "' (Atteso: 'Test')"
  END IF
  PRINT "  Dopo IF, C non esiste più. Assegnazione implicita:"
  ' Questa assegnazione crea una NUOVA variabile 'C' nello scope della procedura TestIfScope.
  C.Nome = "Nuovo" 
  PRINT "  Assegnato C.Nome = 'Nuovo'. Ora esiste a livello di procedura."
  PRINT
END SUB

' Subroutine helper per il test delle variabili STATIC.
SUB StaticCounterSub()
  ' 'Counter' viene inizializzata a 100 solo la prima volta.
  ' Il suo valore viene conservato tra le chiamate.
  STATIC Counter = 100
  ' 'LocalVar' viene creata e distrutta ad ogni chiamata.
  LOCAL LocalVar = 10
  Counter = Counter + 1
  LocalVar = LocalVar + 1
  PRINT "    Chiamata: Counter (STATIC) ="; Counter; "| LocalVar (LOCAL) ="; LocalVar
END SUB

' Testa il comportamento delle variabili STATIC.
SUB TestStatic()
  PRINT "--- 3. Test Variabili STATIC ---"
  PRINT "  Chiamo StaticCounterSub 3 volte:"
  StaticCounterSub()
  StaticCounterSub()
  StaticCounterSub()
  PRINT "  (Counter statico deve incrementare, LocalVar deve resettarsi)"
  PRINT
END SUB

' Testa lo "shadowing" (oscuramento) di variabili globali.
SUB TestLetVsImplicit()
  PRINT "--- 4. Test LET vs. Assegnazione Implicita ---"
  PRINT "  Dentro la SUB, GlobalVar iniziale ="; GlobalVar; "(Atteso: 1000)"
  
  PRINT "  Uso 'LET GlobalVar = 20' per creare uno shadow locale..."
  ' Questa non è la variabile globale, ma una nuova variabile locale a questa SUB.
  LET GlobalVar = 20 
  PRINT "  Valore di GlobalVar locale ="; GlobalVar; "(Atteso: 20)"

  PRINT "  Uso 'ImplicitGlobal = 50' (non esiste, creata in -> Scope LOCALE)..."
  ' Crea una nuova variabile locale.
  ImplicitGlobal = 50
  PRINT "  (Uscendo dalla SUB, la GlobalVar globale tornerà a 1000)"
  PRINT
END SUB

' Testa l'accesso e la modifica di array globali e locali.
SUB TestArrayScopes()
  PRINT "--- 5. Test Scope Array Globali e Locali ---"
  ' Dichiarazione di un array locale di scalari
  LOCAL NumeriLocali(2) AS INTEGER
  NumeriLocali(0) = 11
  
  ' Dichiarazione di un array locale di UDT
  LOCAL ContattiLocali(1) AS Contatto
  ContattiLocali(0).Nome = "LOCALE"
  
  PRINT "  Array locale NumeriLocali(0) ="; NumeriLocali(0); "(Atteso: 11)"
  PRINT "  Array locale ContattiLocali(0).Nome = '"; ContattiLocali(0).Nome; "'"
  
  PRINT "  Modifico l'array GLOBALE ElencoContatti da dentro la SUB..."
  ' Questa modifica ha effetto sull'array globale.
  ElencoContatti(0).Nome = "GLOBAL MODIFICATO" 
  PRINT "  ElencoContatti(0).Nome ora è: '"; ElencoContatti(0).Nome; "'"
  PRINT
END SUB