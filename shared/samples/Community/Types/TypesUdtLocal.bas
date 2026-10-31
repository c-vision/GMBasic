' FILE: Types/TypesUdtLocal.bas
' =================================================================================================
'                                GUIDA TEORICA: UDT LOCALI (LOCAL USER-DEFINED TYPES)
' =================================================================================================
'
' GM-BASIC estende la funzionalità dei Tipi Definiti dall'Utente (UDT) permettendo la loro
' dichiarazione all'interno di sottoprogrammi (SUB o FUNCTION).
'
' 1. SCOPE E VISIBILITÀ
'    - Un `TYPE` definito all'interno di una procedura è **locale** a quella procedura.
'    - È visibile solo all'interno del corpo della SUB/FUNCTION in cui è definito e in
'      eventuali blocchi annidati (IF, FOR, etc.).
'    - Non è accessibile da altre procedure o dallo scope del modulo.
'
' 2. VANTAGGI
'    - Incapsulamento: Permette di creare strutture dati di supporto che sono pertinenti
'      solo a una specifica procedura, senza "inquinare" lo scope globale del modulo
'      o del namespace.
'    - Chiarezza: Mantiene la definizione del tipo vicina al suo punto di utilizzo,
'      migliorando la leggibilità del codice.
'
' 3. SINTASSI
'    La sintassi è identica a quella di un UDT globale, ma la dichiarazione `TYPE...END TYPE`
'    è posizionata all'interno del corpo di una SUB o FUNCTION.
'
' =================================================================================================

SUB Main

    PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TypesUdtLocal - Tipi Definiti Utente (UDT) Locali                       ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' ---------------------------------------------------------------------------------------------
    ' DEFINIZIONE DI UN TIPO LOCALE
    ' ---------------------------------------------------------------------------------------------
    ' Questo TYPE 'Cliente' esiste solo all'interno della SUB Main.
    ' Non è visibile da altre procedure in questo o altri file.
    TYPE Cliente
        ID AS LONG
        Nome AS STRING * 20
        Citta AS STRING * 15
        Saldo AS CURRENCY
        Attivo AS BIT
        UltimoOrdine AS DATE
    END TYPE
    
    PRINT "TYPE 'Cliente' definito localmente all'interno di SUB Main."
    
    ' ---------------------------------------------------------------------------------------------
    ' UTILIZZO DEL TIPO LOCALE
    ' ---------------------------------------------------------------------------------------------
    ' Possiamo dichiarare variabili di questo tipo locale.
    DIM clienteRec AS Cliente
    
    ' Possiamo assegnare valori ai suoi campi.
    clienteRec.ID = 1
    clienteRec.Nome = "Cliente Locale"
    
    PRINT "Variabile 'clienteRec' di tipo locale 'Cliente' creata e popolata."
    PRINT "  ID: "; clienteRec.ID
    PRINT "  Nome: "; clienteRec.Nome
    PRINT
    PRINT "Test completato con successo."

END SUB
