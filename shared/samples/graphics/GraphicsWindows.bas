IMPORT WINDOWS
' FILE: Graphics/GraphicsWindows.bas
' ======================================================================================================================
'       GUIDA TEORICA: VIEWPORT E SISTEMI DI COORDINATE
' ======================================================================================================================
'
' Questo programma dimostra due comandi grafici avanzati che semplificano il disegno in aree specifiche
' dello schermo e la creazione di grafici matematici: VIEW e WINDOW.
'
' 1. VIEW: DEFINIRE UN "VIEWPORT" (SOTTO-FINESTRA)
'    - Comando: VIEW (x1, y1)-(x2, y2) [, [colore_sfondo] [, colore_bordo]]
'    - Problema che risolve: Senza VIEW, per disegnare in un pannello (es. da x=100 a x=300), bisogna
'      aggiungere manualmente l'offset a ogni coordinata (es. PSET 100+50, ...). Questo è complesso
'      e soggetto a errori. Inoltre, il disegno può "sforare" al di fuori del pannello.
'    - Soluzione: VIEW definisce un rettangolo sullo schermo che agisce come una "schermata virtuale".
'      Dopo aver chiamato VIEW, le coordinate (0,0) si riferiranno all'angolo in alto a sinistra del
'      viewport, non della finestra. Tutti i comandi di disegno vengono automaticamente "clippati"
'      (tagliati) ai bordi del viewport.
'    - Caratteristiche Chiave:
'      - Le coordinate sono relative al viewport.
'      - Il disegno non può uscire dal viewport.
'      - CLS pulisce solo il viewport attivo.
'      - VIEW senza argomenti ripristina la visualizzazione a schermo intero.
'
' 2. WINDOW: DEFINIRE UN SISTEMA DI COORDINATE LOGICHE
'    - Comando: WINDOW [SCREEN] (x1, y1)-(x2, y2)
'    - Problema che risolve: Le coordinate grafiche sono in pixel (es. da 0 a 639). Per disegnare un
'      grafico matematico (es. y = SIN(x) da -PI a PI), è necessario convertire manualmente ogni
'      punto dalle coordinate logiche (matematiche) a quelle fisiche (pixel).
'    - Soluzione: WINDOW mappa un sistema di coordinate logiche definito dall'utente all'area di
'      disegno corrente (che può essere l'intero schermo o un viewport definito da VIEW).
'      L'interprete si occupa automaticamente della traslazione e dello scaling.
'    - Caratteristiche Chiave:
'      - Permette di pensare in termini di unità del problema (metri, radianti, dollari) invece che in pixel.
'      - La parola chiave SCREEN inverte l'asse Y, facendo in modo che i valori Y positivi vadano
'        verso l'alto, come nei grafici cartesiani standard.
'
' ======================================================================================================================

IMPORT GRAPHICS
IMPORT CONSOLE
IMPORT STRINGS

SUB Main()
    ' Inizializza la modalità grafica 640x480
    SCREEN 12
    CLS

    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ GraphicsWindows - Test per VIEW e WINDOW                       ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT
    PRINT "Premi un tasto per continuare tra le varie dimostrazioni..."
    WAITKEY

    ' --- 1. DIMOSTRAZIONE DI VIEW ---
    CLS
    PRINT "1. Dimostrazione di VIEW: Creazione di 3 pannelli indipendenti."
    
    ' Disegna i bordi dei pannelli usando coordinate assolute
    LINE (9, 49)-(311, 451), 15, B     ' Bordo pannello sinistro
    LINE (329, 49)-(631, 251), 15, B    ' Bordo pannello in alto a destra
    LINE (329, 279)-(631, 451), 15, B   ' Bordo pannello in basso a destra
    
    ' Pannello 1: Sinistra
    VIEW (10, 50)-(310, 450)
    ' Ora (0,0) è l'angolo in alto a sinistra del pannello.
    ' Il pannello è largo 300 pixel e alto 400 pixel.
    LINE (0, 0)-(300, 400), 1, BF ' Riempie il viewport con un colore
    CIRCLE (150, 200), 100, 14    ' Disegna un cerchio al centro del viewport

    ' Pannello 2: Alto a destra
    VIEW (330, 50)-(630, 250)
    CLS 4 ' Pulisce SOLO questo viewport con il colore rosso
    LINE (0, 0)-(300, 200), 10, B ' Disegna una cornice interna relativa al viewport
    PSET (150, 100), 15           ' Disegna un punto al centro del viewport

    ' Pannello 3: Basso a destra
    VIEW (330, 280)-(630, 450)
    LINE (0, 0)-(300, 170), 12      ' Linea diagonale relativa
    LINE (300, 0)-(0, 170), 12
    
    WAITKEY

    ' --- 2. DIMOSTRAZIONE DI WINDOW ---
    VIEW ' Ripristina la visualizzazione a schermo intero
    CLS
    PRINT "2. Dimostrazione di WINDOW: Grafico di una funzione seno."
    
    ' Definisce un sistema di coordinate logiche da -4 a 4 sull'asse X
    ' e da -1.5 a 1.5 sull'asse Y. L'opzione SCREEN inverte l'asse Y.
    VIEWPORT SCREEN (-4, -1.5)-(4, 1.5)
    
    ' Disegna gli assi cartesiani usando le coordinate logiche
    LINE (-4, 0)-(4, 0), 7  ' Asse X
    LINE (0, -1.5)-(0, 1.5), 7 ' Asse Y
    
    ' Disegna la funzione y = SIN(x)
    DIM x AS DOUBLE
    FOR x = -4 TO 4 STEP 0.01
        PSET (x, SIN(x)), 14
    NEXT x

    WAITKEY

    ' --- 3. DIMOSTRAZIONE COMBINATA: VIEW + WINDOW ---
    CLS
    PRINT "3. Dimostrazione Combinata: Grafico di una funzione coseno in un viewport."
    
    ' Definisce un pannello al centro dello schermo
    VIEW (100, 100)-(540, 380), , 15 ' Viewport con bordo bianco
    
    ' Definisce un sistema di coordinate logiche DENTRO il viewport
    VIEWPORT SCREEN (-MATH.PI, -1.2)-(MATH.PI, 1.2)
    
    ' Disegna gli assi relativi al viewport E al sistema di coordinate logiche
    LINE (-MATH.PI, 0)-(MATH.PI, 0), 7
    LINE (0, -1.2)-(0, 1.2), 7
    
    ' Disegna la funzione y = COS(x)
    FOR x = -MATH.PI TO MATH.PI STEP 0.01
        PSET (x, COS(x)), 12
    NEXT x
    
    WAITKEY

    ' Ripristina lo schermo e termina
    VIEW
    VIEWPORT
    CLS
    PRINT "--- TEST COMPLETATO ---"
END SUB
