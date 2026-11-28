' ui_golden.bas — GOLDEN IMAGE della UI (percorso PIXEL/GRAPHICS).
'
' Uno schermo fisso che usa le funzionalità UI introdotte insieme: WrapBox/Grid
' (STYLE flow=), ruoli del tema, grafica. Lo script tools/sh/ui_golden_check.sh
' produce quattro schermate e le confronta con i golden per RID in
' shared/minitests/controls/golden/:
'
'   ui_golden.<rid>.png        tema normale, controlli disposti dalla GRIGLIA
'   ui_golden_deut.<rid>.png   la stessa UI con ui.color_filter = deuteranopia
'   ui_palette.<rid>.png       dipinto a colori noti (palette VGA16)
'   ui_palette_deut.<rid>.png  lo stesso dipinto con il filtro
'
' Gate (tools/sh/ui_golden_check.sh): A) ogni coppia filtrata differisce da quella
' normale (il filtro è applicato DAVVERO); B) tutte corrispondono al golden del RID
' (pct <= tol). `--bless` riscrive i golden.
'
' I controlli si creano UNA volta sola: cambiare il filtro non ricrea la UI, la
' ridipinge soltanto (stesso albero, colori diversi). Lo shot NON contiene
' l'inspector (riporta un tempo: vedi ui.inspector in gmb_form.c).
IMPORT GRAPHICS
IMPORT CONTROLS
IMPORT WINDOWS
IMPORT AI WITH ALIAS

DIM i AS INTEGER
DIM sel AS INTEGER
DIM shot_completo AS INTEGER

SUB Frame(n AS INTEGER)
    DIM k AS INTEGER
    FOR k = 1 TO n : DOEVENTS : NEXT k
END SUB

SUB Build_screen()
    PANEL 16, 16, 608, 388, "UI GOLDEN" NAME "g_p"
    LABEL 34, 52, 260, "campi del modulo" NAME "g_l1"
    LABEL 34, 74, 300, "layout: griglia 3 colonne" NAME "g_l2"
    LABEL 34, 96, 300, "ruoli del tema e focus" NAME "g_l3"

    ' GRIGLIA (WrapBox): tre pulsanti disposti dal contenitore, non a mano.
    ' Il contenitore non ha titolo: i figli partono dal pad, senza sovrapposizioni.
    PANEL 34, 130, 420, 120, "" NAME "g_grid" STYLE "flow=grid cols=3 gap=4 pad=12"
    BUTTON 0, 0, 120, 28, "uno"     NAME "g_b1" STYLE "group=g_grid role=primary"
    BUTTON 0, 0, 120, 28, "due"     NAME "g_b2" STYLE "group=g_grid role=success"
    BUTTON 0, 0, 120, 28, "tre"     NAME "g_b3" STYLE "group=g_grid role=warning"
    BUTTON 0, 0, 120, 28, "quattro" NAME "g_b4" STYLE "group=g_grid role=danger"
    BUTTON 0, 0, 120, 28, "cinque"  NAME "g_b5" STYLE "group=g_grid"
    BUTTON 0, 0, 120, 28, "sei"     NAME "g_b6" STYLE "group=g_grid"

    LISTBOX 34, 268, 300, 96, "alfa|beta|gamma|delta" INTO sel NAME "g_lb"
    CHECKBOX 350, 268, "opzione" INTO sel NAME "g_cb"

    ' PANNELLO MARKDOWN (vista testuale virtuale): il testo si impagina da solo,
    ' il markup non compare. Lo STYLE "mode=" è la sola chiave che serve.
    LISTBOX 352, 130, 260, 120, "" NAME "g_md" STYLE "mode=markdown"
    CALL Set_text()
    LISTBOX 352, 300, 260, 64, "" NAME "g_df" STYLE "mode=diff"
    CALL Set_diff()
END SUB

SUB Set_text()
    CONTROL SET "g_md", "text", "# Titolo" + CHR$(10) + CHR$(10) _
        + "> citazione" + CHR$(10) + "- primo" + CHR$(10) + "- secondo"
END SUB

SUB Set_diff()
    CONTROL SET "g_df", "text", "--- a/x" + CHR$(10) + "+++ b/x" + CHR$(10) _
        + "@@ -1 +1 @@" + CHR$(10) + "-vecchio" + CHR$(10) + "+nuovo"
END SUB

' Dipinto a colori NOTI (ROSSO/VERDE/BLU/GIALLO della palette VGA16): la prova che
' il filtro arriva ai pixel anche quando il colore non viene dal tema.
SUB Paint_palette()
    CLS 0
    LINE (40, 40)-(240, 140), 12, "BF"      ' 12 = rosso vivo
    LINE (260, 40)-(460, 140), 10, "BF"     ' 10 = verde acceso
    LINE (40, 170)-(240, 270), 9, "BF"      ' 9  = blu
    LINE (260, 170)-(460, 270), 14, "BF"    ' 14 = giallo
    LOCATE 8, 40 : COLOR 15 : PRINT "testo"
END SUB

SUB Main()
    shot_completo = 1
    SCREEN 12, 640, 420, 32
    WINDOW THEME "glass"

    ' PRIMA i dipinti puri: senza controlli sulla pagina nessuno li ridisegna sopra.
    CALL Paint_palette()
    CALL Frame(6)
    SAVE SCREEN "work/ui_palette.png"

    AI SET OPTION "ui.color_filter", "deuteranopia"
    CALL Paint_palette()
    CALL Frame(6)
    SAVE SCREEN "work/ui_palette_deut.png"
    AI SET OPTION "ui.color_filter", ""

    ' Poi la UI vera (create UNA volta): il form possiede la pagina da qui in poi.
    CALL Build_screen()
    CALL Frame(12)
    SAVE SCREEN "work/ui_golden.png"

    ' Stessa UI, filtro daltonia (deuteranopia): nessun controllo ricreato, si
    ' ridipinge soltanto. Il cambio di filtro invalida il frame da solo.
    AI SET OPTION "ui.color_filter", "deuteranopia"
    CALL Frame(12)
    SAVE SCREEN "work/ui_golden_deut.png"

    AI SET OPTION "ui.color_filter", ""
    SCREEN 0
END SUB