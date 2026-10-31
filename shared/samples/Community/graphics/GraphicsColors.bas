' FILE: GraphicsColors.bas
' =================================================================================================
'       GUIDA TEORICA: IL COMANDO COLOR CON TIPI RGBA
' =================================================================================================
'
' Questo file dimostra l'utilizzo avanzato del comando COLOR con il nuovo tipo di dato
' RGBA, che rappresenta un colore True Color.
'
' 1. IL TIPO `RGBA`
'    - È un nuovo tipo di dato built-in, definito nel namespace GRAPHICS.
'    - Viene utilizzato per memorizzare un colore definito dai suoi componenti Rosso, Verde e Blu.
'    - Le variabili di questo tipo possono essere dichiarate con: DIM mioColore AS RGBA
'
' 2. LA FUNZIONE `RGB()`
'    - Scopo: Creare un'istanza del tipo RGBA.
'    - Sintassi: RGB(rosso AS INTEGER, verde AS INTEGER, blu AS INTEGER)
'    - Esempio: mioColore = RGB(255, 100, 20)
'
' 3. UTILIZZO CON IL COMANDO `COLOR`
'    - Il comando `COLOR` ora accetta sia i classici indici numerici (0-15) sia le variabili
'      o le chiamate a funzione che restituiscono un `RGBA`.
'    - Esempio:
'      DIM c AS RGBA = RGB(0, 255, 0)
'      COLOR c  ' Imposta il primo piano a verde
'
' 4. PASSAGGIO A SUBROUTINE
'    - Le variabili `RGBA` possono essere passate come parametri a SUB e FUNCTION,
'      permettendo la creazione di codice modulare e riutilizzabile.
'    - Esempio:
'      SUB PrintHighlight(messaggio AS STRING, colore AS RGBA)
'          COLOR colore
'          PRINT messaggio
'      END SUB
'
' 5. ACCESSO AI COMPONENTI
'    - È possibile leggere i singoli componenti di una variabile `RGBA`
'      utilizzando la notazione a punto.
'    - Membri disponibili: .Red, .Green, .Blue, .Alpha (Alpha è riservato per usi futuri).
'    - Esempio:
'      PRINT "Componente Rosso:", mioColore.Red
'
' =================================================================================================

' =================================================================================================
'       GUIDA TEORICA: IL COMANDO COLOR CON TIPI RGBA E CANALE ALPHA
' =================================================================================================
'
' Questo file dimostra l'utilizzo avanzato del comando COLOR con il tipo di dato RGBA.
'
' 1. LA FUNZIONE `RGB()` AGGIORNATA
'    - Scopo: Creare un'istanza del tipo RGBA.
'    - Sintassi: RGB(rosso AS INTEGER, verde AS INTEGER, blu AS INTEGER [, alpha AS INTEGER])
'    - Il quarto parametro, `alpha`, è opzionale e controlla la trasparenza (0=trasparente, 255=opaco).
'      Se omesso, il valore di default è 255 (opaco).
'    - Esempio: mioColore = RGB(255, 100, 20, 128) ' Arancione semi-trasparente
'
' 2. ACCESSO AI COMPONENTI
'    - È possibile leggere i singoli componenti di una variabile `RGBA`.
'    - Membri disponibili: .Red, .Green, .Blue, .Alpha.
'    - Esempio:
'      PRINT "Componente Alpha:", mioColore.Alpha
'
' =================================================================================================

SUB Main()
    SCREEN 12 ' 640x480, 16 colori
    FONT "font.ttf", 12 ' Il testo è visibile solo con un font caricato

    ' ==================================================================
    PRINT "--- 1. Test Colori Standard (Indici 0-15) ---"
    FOR i = 0 TO 15
        COLOR i, 0 ' Imposta il colore di primo piano, sfondo nero
        PRINT "Questo e' il colore con indice "; i
    NEXT
    COLOR 7, 0
    PRINT

    ' ==================================================================
    PRINT "--- 2. Creazione e Assegnazione di variabili RGBA ---"
    
    DIM arancione AS RGBA = RGB(255, 100, 20)
    DIM bluScuro AS RGBA = RGB(0, 0, 128)
    DIM giallo AS RGBA = RGB(255, 255, 0)

    COLOR arancione
    PRINT "Testo in ARANCIONE (da variabile)"
    
    COLOR giallo, bluScuro
    PRINT "Testo GIALLO su sfondo BLU SCURO (da variabili)"
    COLOR 7, 0
    PRINT

    ' ==================================================================
    PRINT "--- 3. Passaggio di variabili RGBA a una Subroutine ---"
    
    DIM verdeGiallo AS RGBA = RGB(173, 255, 47)
    DIM grigioScuro AS RGBA = RGB(50, 50, 50)
    
    PrintInColor("Testo stampato tramite una SUB dedicata.", verdeGiallo, grigioScuro)
    PrintInColor("Un altro test con colori invertiti.", bluScuro, giallo)
    COLOR 7, 0
    PRINT
    
    ' ==================================================================
    PRINT "--- 4. Accesso ai componenti di una variabile RGBA ---"
    
    DIM magenta AS RGBA = RGB(255, 0, 255)
    
    PRINT "Colore: Magenta"
    PRINT "Componenti -> R:", magenta.Red, "G:", magenta.Green, "B:", magenta.Blue, "A:", magenta.Alpha
    PRINT

    ' ==================================================================
    PRINT "--- 5. Test con Canale Alpha (Trasparenza) ---"

    ' Crea un colore rosso semi-trasparente. Il rendering effettivo della trasparenza
    ' dipende dall'implementazione dell'executor e del backend grafico.
    DIM rossoTrasparente AS RGBA = RGB(255, 0, 0, 128)
    
    COLOR rossoTrasparente
    PRINT "Questo testo dovrebbe essere rosso semi-trasparente."
    PRINT "Componente Alpha:", rossoTrasparente.Alpha
    PRINT

    ' Ripristina i colori di default alla fine del programma
    COLOR 7, 0
    PRINT "Test dei colori completato. Premi un tasto per uscire."
    
    ' Attende la pressione di un tasto prima di terminare
    WAITKEY
END SUB

'
' Subroutine helper che accetta due parametri di tipo RGBA
' per impostare i colori e stampare un messaggio.
'
SUB PrintInColor(messaggio AS STRING, fg AS RGBA, bg AS RGBA)
    COLOR fg, bg
    PRINT messaggio
END SUB