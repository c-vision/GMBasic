' FILE: Loops/LoopsForEach.bas
' =================================================================================================
'       GUIDA TEORICA: IL CICLO FOR EACH E L'INFERENZA DEI TIPI (TODO #25)
' =================================================================================================
'
' Questo file testa l'implementazione dell'inferenza di tipo per il ciclo FOR EACH,
' una funzionalità che riduce la verbosità e modernizza la sintassi del linguaggio.
'
' 1. IL COMANDO `FOR EACH`
'    - Scopo: Iterare sugli elementi di una collezione (array, lista, dizionario, etc.)
'      senza la necessità di gestire manualmente un indice numerico come nel ciclo FOR...NEXT.
'
' 2. SINTASSI TRADIZIONALE (CON TIPO ESPLICITO)
'    - Prima di questa modifica, era necessario dichiarare esplicitamente il tipo della
'      variabile di iterazione usando la clausola `AS`.
'    - Esempio:
'      DIM Nomi() AS STRING
'      ...
'      FOR EACH nome AS STRING IN Nomi()
'        PRINT nome
'      NEXT
'
' 3. NUOVA SINTASSI (CON INFERENZA DI TIPO)
'    - Proposta: Il compilatore (specificamente il Rewriter nella FASE 3) è ora in grado
'      di dedurre automaticamente il tipo della variabile di iterazione dal tipo della collezione.
'    - La clausola `AS` diventa opzionale.
'    - Esempio (equivalente):
'      DIM Nomi() AS STRING
'      ...
'      FOR EACH nome IN Nomi()  ' 'nome' viene inferito come STRING
'        PRINT UCASE(nome)      ' Le funzioni per stringhe funzionano senza errori
'      NEXT
'
' 4. TIPI DI COLLEZIONE SUPPORTATI E INFERENZA
'    - Array (es. `DIM Arr() AS DOUBLE`): la variabile di iterazione viene inferita come `DOUBLE`.
'    - Array di UDT (es. `DIM Arr() AS Persona`): la variabile viene inferita come `Persona`,
'      permettendo l'accesso diretto ai campi (es. `variabile.Nome`).
'    - Liste Native (es. `LIST OF STRING`): la variabile viene inferita come `STRING`.
'    - Dizionari Nativi (`DICT OF K, V`):
'      a) `FOR EACH k, v IN MioDizionario`: `k` viene inferito come tipo `K`, `v` come tipo `V`.
'      b) `FOR EACH entry IN MioDizionario`: `entry` viene inferito come tipo speciale `KEYVALUEPAIR`,
'         permettendo l'accesso a `entry.Key` e `entry.Value`.
'    - Stringhe: Iterare su una stringa inferisce il tipo della variabile come `STRING` (un singolo carattere).
'
' 5. BENEFICI
'    - Codice più conciso e leggibile.
'    - Meno ripetizioni, specialmente con nomi di tipo lunghi (UDT, Classi).
'    - Allineamento con le pratiche dei linguaggi moderni.
'
' =================================================================================================

' Dichiarazione di un tipo utente per testare l'inferenza su array di UDT
TYPE Persona
    Nome AS STRING * 20
    Eta AS INTEGER
END TYPE

SUB Main()
    ' --- SETUP DELLE COLLEZIONI DI TEST ---
    DIM Numeri(4) AS INTEGER = {10, 20, 30, 40, 50}
    DIM Nomi$() = {"Alice", "Bob", "Charlie"}
    
    DIM Persone(1) AS Persona
    Persone(0).Nome = "Mario"
    Persone(0).Eta = 42
    Persone(1).Nome = "Luisa"
    Persone(1).Eta = 35
    
    DIM ListaNomi AS LIST OF STRING = {"David", "Eve", "Frank"}
    
    DIM Prezzi AS DICT OF STRING, DOUBLE = { "Mela": 0.50, "Banana": 0.30, "Arancia": 0.70 }
    
    DIM Testo AS STRING = "GM-BASIC"
    
    ' ==================================================================
    PRINT "--- 1. Test su Array di tipi primitivi (INTEGER) ---"
    FOR EACH n IN Numeri()
        ' Se 'n' è inferito correttamente come INTEGER/DOUBLE, questa operazione è valida.
        PRINT "Numero:", n, "Doppio:", n * 2
    NEXT
    PRINT

    ' ==================================================================
    PRINT "--- 2. Test su Array di User-Defined Types (Persona) ---"
    FOR EACH p IN Persone()
        ' Se 'p' è inferito correttamente come Persona, l'accesso a .Nome è valido.
        PRINT "Persona:", p.Nome, "Età:", p.Eta
    NEXT
    PRINT

    ' ==================================================================
    PRINT "--- 3. Test su LIST OF STRING ---"
    FOR EACH nome IN ListaNomi
        ' Se 'nome' è inferito come STRING, UCASE() è una funzione valida.
        PRINT "Nome in maiuscolo:", UPPER(nome)
    NEXT
    PRINT
    
    ' ==================================================================
    PRINT "--- 4. Test su DICT OF STRING, DOUBLE (con chiave, valore) ---"
    FOR EACH prodotto, prezzo IN Prezzi
        ' 'prodotto' deve essere inferito come STRING, 'prezzo' come DOUBLE.
        PRINT UPPER(prodotto), "costa", prezzo, "(IVA inclusa:", prezzo * 1.22, ")"
    NEXT
    PRINT
    
    ' ==================================================================
    PRINT "--- 5. Test su DICT OF STRING, DOUBLE (con KeyValuePair) ---"
    FOR EACH entry IN Prezzi
        ' 'entry' deve essere inferito come KEYVALUEPAIR.
        PRINT "Entry -> Key:", entry.Key, "| Value:", entry.Value
    NEXT
    PRINT
    
    ' ==================================================================
    PRINT "--- 6. Test su iterazione di una STRING ---"
    FOR EACH carattere IN Testo
        ' 'carattere' deve essere inferito come STRING (di un carattere).
        PRINT "Carattere:", carattere, "Codice ASCII:", ASC(carattere)
    NEXT
    PRINT
    
    PRINT "Tutti i test FOR EACH con inferenza sono stati eseguiti."
    
END SUB