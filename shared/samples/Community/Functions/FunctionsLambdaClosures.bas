' FILE: Functions/FunctionsLambdaClosures.bas
' =================================================================================================
'                                 GUIDA TEORICA: FUNZIONI DI PRIMA CLASSE
' =================================================================================================
'
' GM-BASIC supporta le funzioni come "cittadini di prima classe". Questo significa che le funzioni
' non sono solo blocchi di codice statici, ma possono essere trattate come valori. Possono essere:
' 1. Assegnate a variabili (Puntatori a Funzione).
' 2. Passate come argomenti ad altre funzioni (Higher-Order Functions).
' 3. Restituite da altre funzioni.
' 4. Definite dinamicamente (Lambda) o localmente (Funzioni Locali).
'
' --- CONCETTI CHIAVE ---
'
' 1. DELEGATE (Delegati):
'    Un DELEGATE è un "tipo" che definisce la firma di una funzione (i suoi parametri e il tipo di ritorno).
'    Permette di dichiarare variabili che possono contenere riferimenti a qualsiasi funzione che rispetti quella firma.
'    Es: DELEGATE FUNCTION Matematica(a AS INT, b AS INT) AS INT
'
' 2. LAMBDA EXPRESSIONS (Funzioni Anonime):
'    Sono funzioni definite "al volo" senza un nome, spesso usate per brevi operazioni passate come argomenti.
'    Sintassi a freccia: FN(x) => x * 2
'    Sintassi a blocco:  FUNCTION(x) ... END FUNCTION
'
' 3. CLOSURES (Chiusure):
'    Una closure è una funzione che "ricorda" l'ambiente in cui è stata creata. Può accedere alle variabili
'    locali della funzione che la contiene, anche dopo che la funzione esterna ha terminato l'esecuzione.
'    Questo è reso possibile dalla "Heap Promotion", che sposta le variabili catturate dallo stack all'heap.
'
' =================================================================================================

IMPORT ARRAYS

' Definiamo un delegato per la funzione contatore.
' Questo definisce un TIPO per una funzione che non prende argomenti e restituisce un INTERO.
DELEGATE FUNCTION ContatoreFunc() AS INTEGER

SUB Main
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsLambdaClosures - Funzioni di Prima Classe                     ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' ---------------------------------------------------------------------------------------------
    ' 1. PUNTATORI A FUNZIONE
    ' ---------------------------------------------------------------------------------------------
    ' Assegniamo funzioni globali esistenti (Raddoppia, Saluta) a variabili locali.
    ' Il compilatore inferisce che 'FunzioneRaddoppia' è un puntatore a funzione compatibile.
    PRINT "--- 1. Assegnazione di Puntatori a Funzione ---"
    LET FunzioneRaddoppia = Raddoppia
    LET FunzioneSaluta = Saluta
    PRINT "Puntatori a 'Raddoppia' e 'Saluta' creati."
    PRINT
    
    ' ---------------------------------------------------------------------------------------------
    ' 2. INVOCAZIONE INDIRETTA
    ' ---------------------------------------------------------------------------------------------
    ' Chiamiamo le funzioni attraverso le variabili puntatore.
    PRINT "--- 2. Invocazione tramite Puntatori a Funzione ---"
    PRINT "Raddoppia(10) tramite puntatore -> "; FunzioneRaddoppia(10)
    FunzioneSaluta("Mondo")
    PRINT
    
    ' ---------------------------------------------------------------------------------------------
    ' 3. LAMBDA EXPRESSIONS (Funzioni Anonime)
    ' ---------------------------------------------------------------------------------------------
    ' Definiamo una funzione "in-line" usando la sintassi lambda (=>).
    ' Questa funzione non ha nome, ma viene assegnata alla variabile 'Triplica'.
    PRINT "--- 3. Funzioni Anonime (Lambda) ---"
    LET Triplica = FN(x AS INTEGER) AS INTEGER => x * 3
    PRINT "Lambda 'Triplica' creata con sintassi =>"
    PRINT "Triplica(10) -> "; Triplica(10)
    PRINT

    ' ---------------------------------------------------------------------------------------------
    ' 4. CLOSURES E STATEFULNESS (Con Delegati Nominati)
    ' ---------------------------------------------------------------------------------------------
    ' Qui dimostriamo il potere delle closure. 'CreaContatore' restituisce una funzione che
    ' mantiene il proprio stato interno (valoreCorrente).
    PRINT "--- 4. Funzioni Locali e Closure (con DELEGATE) ---"
    
    ' Creiamo un'istanza del contatore. La variabile 'valoreIniziale' (100) viene "catturata".
    LET Contatore = CreaContatore(100)
    
    PRINT "Creato un contatore con valore iniziale 100."
    ' Ogni chiamata incrementa lo stato interno, che persiste tra le chiamate.
    PRINT "Prima chiamata -> "; Contatore()  // Stampa 101
    PRINT "Seconda chiamata -> "; Contatore() // Stampa 102
    PRINT "Terza chiamata -> "; Contatore()  // Stampa 103
    PRINT
    
    ' ---------------------------------------------------------------------------------------------
    ' 4b. CLOSURES E STATEFULNESS (Con Tipi Funzione Anonimi)
    ' ---------------------------------------------------------------------------------------------
    ' Stesso concetto, ma usando la sintassi 'AS FUNCTION' per il tipo di ritorno,
    ' senza definire un DELEGATE esplicito.
    PRINT "--- 4b. Funzioni Locali e Closure (con AS FUNCTION) ---"
    LET Contatore2 = CreaContatoreAnonimo(200)
    PRINT "Creato un contatore anonimo con valore iniziale 200."
    PRINT "Prima chiamata -> "; Contatore2()  // Stampa 201
    PRINT "Seconda chiamata -> "; Contatore2() // Stampa 202
    PRINT

    ' ---------------------------------------------------------------------------------------------
    ' 5. PROGRAMMAZIONE FUNZIONALE (Lambda come Argomenti)
    ' ---------------------------------------------------------------------------------------------
    ' Le funzioni lambda sono potenti quando passate ad algoritmi generici come MAP, FILTER, REDUCE.
    PRINT "--- 5. Lambda come Parametri (Stile Funzionale) ---"
    DIM Numeri() = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
    PRINT "Array originale: {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}"
    
    ' AMAP: Applica una funzione a ogni elemento (Proiezione).
    ' Qui usiamo una lambda per elevare al quadrato.
    DIM Quadrati() = AMAP(Numeri, FN(n AS INTEGER) AS INTEGER => n * n)
    PRINT "Array al quadrato (AMAP): ";
    AEVAL Quadrati, SUB(x AS INTEGER) { PRINT x; " "; } : PRINT

    ' AFILTER: Seleziona gli elementi che soddisfano un predicato.
    ' Qui usiamo una lambda per selezionare i numeri pari.
    DIM Pari() = AFILTER(Numeri, FN(n AS INTEGER) AS INTEGER => (n MOD 2) = 0)
    PRINT "Numeri pari (AFILTER): ";
    AEVAL Pari, SUB(x AS INTEGER) { PRINT x; " "; } : PRINT
    
    ' AREDUCE: Aggrega gli elementi in un singolo valore.
    ' Qui usiamo una lambda per sommare tutti gli elementi.
    DIM Somma = AREDUCE(Numeri, FN(acc AS INTEGER, el AS INTEGER) AS INTEGER => acc + el)
    PRINT "Somma di tutti i numeri (AREDUCE): "; Somma
    
    ' Esempio di definizione di una lambda a blocco assegnata a variabile.
    LET FunzioneSommaPari = FUNCTION(acc AS DOUBLE, el AS DOUBLE) AS DOUBLE:
        IF (el MOD 2) = 0 THEN
            acc = acc + el
        END IF
        RETURN acc
    END FUNCTION

    ' Esempio di lambda a blocco definita inline nell'argomento.
    ' Nota: Questa sintassi permette logica complessa dentro l'argomento di una funzione.
    DIM SommaPariInLine = AREDUCE(Numeri, FUNCTION(acc AS DOUBLE, el AS DOUBLE) AS DOUBLE:
        IF (el MOD 2) = 0 THEN
            acc = acc + el
        END IF
        RETURN acc
    END FUNCTION)

    ' Poi, la variabile contenente la lambda viene passata come argomento.
    DIM SommaPari = AREDUCE(Numeri, FunzioneSommaPari)
    PRINT "Somma dei soli numeri pari (AREDUCE con blocco): "; SommaPari

    PRINT
    
    PRINT "Esecuzione di Main terminata."
END SUB

' =================================================================================================
' DEFINIZIONI DELLE FUNZIONI HELPER
' =================================================================================================

' Funzione globale standard
FUNCTION Raddoppia(valore AS INTEGER) AS INTEGER
    RETURN valore * 2
END FUNCTION

' Subroutine globale standard
SUB Saluta(nome AS STRING)
    PRINT "Ciao, "; nome; "!"
END SUB

' ---------------------------------------------------------------------------------------------
' PATTERN CLOSURE: Funzione che restituisce una funzione con stato.
' ---------------------------------------------------------------------------------------------
' Quando 'CreaContatore' viene eseguita, la variabile 'valoreCorrente' verrebbe normalmente
' distrutta al termine. Tuttavia, poiché 'ContatoreInterno' la usa (la "cattura"),
' il compilatore "promuove" 'valoreCorrente' nell'Heap (dentro una classe contesto nascosta).
' In questo modo, 'valoreCorrente' sopravvive finché esiste il delegato restituito.
FUNCTION CreaContatore(valoreIniziale AS INTEGER) AS ContatoreFunc
    LOCAL valoreCorrente AS INTEGER = valoreIniziale

    ' Questa è una FUNZIONE LOCALE. Può "vedere" e modificare 'valoreCorrente'.
    FUNCTION ContatoreInterno() AS INTEGER
        valoreCorrente = valoreCorrente + 1
        RETURN valoreCorrente
    END FUNCTION

    ' Restituisce un PUNTATORE alla funzione locale.
    RETURN ContatoreInterno
END FUNCTION

' Esempio equivalente con sintassi AS FUNCTION esplicita invece di DELEGATE.
FUNCTION CreaContatoreAnonimo(valoreIniziale AS INTEGER) AS FUNCTION() AS INTEGER
    LOCAL valoreCorrente AS INTEGER = valoreIniziale
    
    FUNCTION ContatoreInterno2() AS INTEGER
        valoreCorrente = valoreCorrente + 1
        RETURN valoreCorrente
    END FUNCTION
    
    RETURN ContatoreInterno2
END FUNCTION