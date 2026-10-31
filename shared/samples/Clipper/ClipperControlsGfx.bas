' ClipperControlsGfx.bas — ClipperControls renderizzato in GRAFICA (finestra grande,
' font piccolo). Gli stessi @...GET (listbox/text/spinner/checkbox/combobox).
SCREEN 12, 1200, 900, 32
SUB Main()
    DIM nome AS STRING
    nome = "Default"
    DIM eta AS INTEGER
    eta = 25
    DIM attivo AS INTEGER
    attivo = 1
    DIM colore AS STRING
    colore = "Rosso"
    DIM scelta AS STRING
    @ 3, 3 SAY "Scegli colore:"
    @ 4, 3 GET colore TYPE "listbox" ITEMS {"Rosso", "Verde", "Blu", "Giallo"} SIZE 15, 4
    @ 3, 25 SAY "Nome:"
    @ 4, 25 GET nome PICTURE "@!" SIZE 15
    @ 7, 3 SAY "Eta (18-99):"
    @ 8, 3 GET eta TYPE "spinner" MIN 18 MAX 99 STEP 1 SIZE 10
    @ 7, 25 SAY "Attivo:"
    @ 8, 25 GET attivo TYPE "checkbox" CAPTION "Abilita"
    @ 11, 3 SAY "Seleziona:"
    @ 12, 3 GET scelta TYPE "combobox" ITEMS {"Opzione A","Opzione B","Opzione C"} SIZE 15
    READ
    ALERT("Nome: " + nome)
END SUB
