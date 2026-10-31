' FILE: Types/TypesAny.bas
' =================================================================================================
'       GUIDA TEORICA: IL TIPO DINAMICO 'ANY'
' =================================================================================================
'
' Il tipo `ANY` (spesso chiamato Variant o Object in altri linguaggi) introduce il 
' dinamismo nel sistema di tipi statico di GM-BASIC.
'
' 1. NATURA DI ANY
'    Una variabile dichiarata `AS ANY` può contenere valori di qualsiasi tipo (numeri, 
'    stringhe, booleani, istanze di UDT, Classi o Collezioni). Il tipo reale del 
'    dato viene determinato e gestito a runtime dall'Executor.
'
' 2. REGOLE DI COERCIZIONE (Upcasting e Downcasting)
'    - Upcasting (Sempre Sicuro): Qualsiasi tipo specifico può essere assegnato a `ANY`.
'    - Downcasting (Runtime Check): Un valore `ANY` può essere assegnato a un tipo specifico.
'      L'Executor verificherà la compatibilità al momento dell'assegnazione.
'
' 3. CASI D'USO IDEALI
'    - Collezioni eterogenee (es. una lista che contiene sia nomi che età).
'    - Interfacce con dati esterni (JSON, database non strutturati).
'    - Procedure generiche che operano su dati di natura diversa.
'
' 4. PERFORMANCE
'    L'uso di `ANY` comporta un leggero overhead rispetto ai tipi statici (INTEGER, DOUBLE)
'    perché l'interprete deve controllare il tipo reale prima di ogni operazione.
'
' =================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ TypesAny.bas - Documentazione e Test del tipo dinamico ANY                  ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' --- 1. Variabili Semplici e Riassegnazione Dinamica ---
100 PRINT "--- 1. Variabili ANY Semplici ---"
110 DIM VariabileFlessibile AS ANY
120 VariabileFlessibile = "Sono una stringa"
130 PRINT "Valore (String): "; VariabileFlessibile
140 VariabileFlessibile = 42.5
150 PRINT "Valore (Double): "; VariabileFlessibile
160 VariabileFlessibile = TRUE
170 PRINT "Valore (Bit):    "; VariabileFlessibile
180 PRINT

' --- 2. Collezioni Eterogenee ---
200 PRINT "--- 2. Collezioni Eterogenee ---"
    ' Una lista che mescola tipi diversi
210 DIM ListaMista AS LIST OF ANY = ["Testo", 100, 3.14, FALSE]
220 ListaMista.Add({ id: 1, tag: "Oggetto interno" }) ' Aggiunge un dizionario
230 FOR EACH elemento IN ListaMista
240     PRINT "  Elemento: "; elemento; " (Tipo: "; ProcessType(elemento); ")"
250 NEXT
260 PRINT

' --- 4. Passaggio a Procedure ---
500 PRINT "--- 4. Procedure e ANY ---"
510 DIM Risultato AS ANY
520 Risultato = RaddoppiaDinamico(15)
530 PRINT "Raddoppia(15)     = "; Risultato
540 Risultato = RaddoppiaDinamico("Ciao")
550 PRINT "Raddoppia('Ciao') = "; Risultato
560 PRINT

' --- 5. Downcasting (Assegnazione da ANY a tipo specifico) ---
600 PRINT "--- 5. Downcasting ---"
610 DIM Contenitore AS ANY = 1000
620 DIM ValoreIntero AS INTEGER
630 ValoreIntero = Contenitore ' Downcast implicito gestito dall'Executor
640 PRINT "Valore INTEGER estratto da ANY: "; ValoreIntero
650 PRINT

PRINT "--- TEST COMPLETATO ---"
END

' =================================================================================================
' PROCEDURE DI TEST
' =================================================================================================

''' <summary>
''' Una funzione che accetta un parametro ANY e restituisce un valore ANY.
''' Implementa una logica polimorfica basata sul contenuto.
''' </summary>
FUNCTION RaddoppiaDinamico(Input AS ANY) AS ANY
    ' L'Executor gestirà le operazioni matematiche o di stringa 
    ' basandosi sul tipo reale a runtime.
    IF MATH.ISNUMERIC(Input) THEN
        RETURN Input * 2
    ELSE
        RETURN Input + Input
    END IF
END FUNCTION

''' <summary>
''' Helper per visualizzare il nome del tipo reale contenuto in un ANY.
''' </summary>
FUNCTION ProcessType(v AS ANY) AS STRING
    ' Usiamo la funzione SYS di sistema per ottenere informazioni sul tipo
    RETURN TYPENAME(v)
END FUNCTION
