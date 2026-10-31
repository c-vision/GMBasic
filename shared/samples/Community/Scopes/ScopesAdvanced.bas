' FILE: Scopes/ScopesAdvanced.bas
' ======================================================================================================================
' GUIDA TEORICA: GERARCHIA DEGLI SCOPE E CICLO DI VITA DELLE VARIABILI
' ======================================================================================================================
' GM-BASIC implementa un sistema di scoping lessicale rigoroso (block-scoped), dove la visibilità di una variabile
' è determinata dal blocco di codice in cui viene dichiarata (o assegnata per la prima volta).
'
' 1. GERARCHIA DEGLI SCOPE
'    - Global Scope: Variabili dichiarate con `GLOBAL` o a livello di modulo (fuori da SUB/FUNCTION).
'      Visibili ovunque, a meno che non vengano oscurate (shadowed).
'    - Procedure Scope: Variabili dichiarate dentro una SUB/FUNCTION (es. parametri).
'    - Block Scope: Ogni struttura di controllo (`IF`, `FOR`, `WHILE`, `TRY`, `SELECT`) crea un nuovo scope figlio.
'
' 2. DICHIARAZIONE E VISIBILITÀ
'    - `LOCAL`: Dichiara esplicitamente una variabile nel blocco corrente. Nasconde eventuali variabili con lo
'      stesso nome negli scope genitori (Shadowing).
'    - `GLOBAL`: Dichiara una variabile nello scope radice del modulo/programma.
'    - Assegnazione Implicita (`Var = Val`):
'      - Se la variabile esiste già nella catena degli scope (corrente o genitori), viene modificata quella esistente.
'      - Se non esiste, ne viene creata una NUOVA nello scope *corrente* (block-scoped).
'
' 3. CICLO DI VITA (LIFETIME)
'    - Una variabile locale a un blocco (es. dentro un `FOR`) cessa di esistere ("muore") non appena l'esecuzione
'      esce da quel blocco. Tentare di accedervi dall'esterno genera un errore a runtime o di compilazione.
' ======================================================================================================================

SUB TestScopeAnnidato()
  PRINT
  PRINT "--- 2. Inizio Test Scope Annidato ---"
  
  ' Inizia un blocco FOR. Viene creato un nuovo scope figlio della Procedura.
  FOR I = 1 TO 1 
    ' VarCiclo è dichiarata esplicitamente LOCALE al blocco FOR.
    LOCAL VarCiclo = 200     
    ' G_Ciclo non esiste in nessun scope genitore. Viene creata implicitamente nello scope CORRENTE (FOR).
    G_Ciclo = 250          
    
    ' Inizia un blocco IF annidato. Viene creato un nuovo scope figlio del FOR.
    IF I = 1 THEN 
      ' VarIf è locale solo a questo blocco IF.
      LOCAL VarIf = 300      
      
      ' VarCiclo esiste nello scope genitore (FOR). Viene modificata quella istanza.
      VarCiclo = 201       
      
      ' G_Globale esiste nello scope globale (definito in Main o modulo). Viene modificata l'istanza globale.
      G_Globale = 101      
      
      ' G_If non esiste. Viene creata implicitamente nello scope CORRENTE (IF).
      G_If = 350           
    END IF ' Fine scope IF. VarIf e G_If cessano di esistere.
    
    PRINT "  Dopo IF: VarCiclo ="; VarCiclo; "(Atteso: 201)"
    ' PRINT "  Dopo IF: G_If ="; G_If; "(ERRORE: G_If non esiste più)"
  NEXT I ' Fine scope FOR. VarCiclo e G_Ciclo cessano di esistere.
  
  ' PRINT "  Dopo FOR: G_Ciclo ="; G_Ciclo; "(ERRORE: G_Ciclo non esiste più)"
  
  PRINT "--- Fine Test Scope Annidato ---"
END SUB

' Aggiunta per verificare lo Shadowing (oscuramento) di variabili globali
SUB TestShadowing()
    PRINT
    PRINT "--- 4. Test Shadowing (Oscuramento) ---"
    PRINT "  [Start] G_Globale (Global) ="; G_Globale; " (Atteso: 101)"
    
    ' Dichiara una variabile LOCALE con lo stesso nome della globale.
    ' Da qui in poi, in questo blocco, G_Globale si riferisce alla locale.
    LOCAL G_Globale = 999
    PRINT "  [Local] Dichiarato LOCAL G_Globale = 999"
    PRINT "  [Local] G_Globale (Local) ="; G_Globale; " (Atteso: 999)"
    
    ' Modifica la variabile locale, non quella globale
    G_Globale = 888
    PRINT "  [Local] Modificato LOCAL G_Globale = 888"
END SUB

SUB Main()
  CLS
  PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ ScopesAdvanced - Scope Avanzato (LOCAL, GLOBAL, Implicito)              ║"
  PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
  PRINT

  PRINT "--- 1. Setup Scope Iniziale in Main ---"
  ' Dichiara una variabile globale visibile ovunque
  GLOBAL G_Globale = 100
  ' Dichiara una variabile locale al Main
  LOCAL L_Main = 10
  ' Assegnazione implicita: crea VarMain nello scope del Main (equivalente a LOCAL in questo caso)
  VarMain = 15                 
  
  PRINT "  Stato iniziale:"
  PRINT "    G_Globale ="; G_Globale; "(Atteso: 100)"
  PRINT "    L_Main ="; L_Main; "(Atteso: 10)"
  PRINT "    VarMain ="; VarMain; "(Atteso: 15)"

  ' Chiama la sub che modifica la globale e testa gli scope annidati
  TestScopeAnnidato() 

  PRINT
  PRINT "--- 3. Verifica Finale in Main (dopo chiamata a SUB) ---"
  PRINT "  Stato finale:"
  PRINT "    G_Globale ="; G_Globale; "(Atteso: 101, modificato dalla SUB)"
  
  ' Chiama il test di shadowing
  TestShadowing()
  PRINT "  Dopo TestShadowing: G_Globale ="; G_Globale; " (Atteso: 101 - Invariato)"
  
  ' Le seguenti variabili sono state create come locali dentro TestScopeAnnidato
  ' e non devono esistere qui. I commenti REM documentano il comportamento atteso (errore).
  REM PRINT "    G_Ciclo ="; G_Ciclo; "(ERRORE: Non esiste qui)"
  REM PRINT "    G_If ="; G_If; "(ERRORE: Non esiste qui)"
  REM PRINT VarCiclo 

END SUB
