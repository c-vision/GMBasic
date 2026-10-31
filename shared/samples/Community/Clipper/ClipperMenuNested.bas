IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperMenuNested.bas
' Test MENU con submenus, separatori e azioni

SUB ShowMessage(msg AS STRING)
    DIM alertResult AS INTEGER
    alertResult = ALERT(msg)
END SUB

SUB Main()
    DIM choice AS STRING
    choice = ""

    ' Menu con submenus e separatori
    ' Sintassi inline: [["File", "Open", "Save", "---", "Exit"], ["Edit", "Cut", "Copy", "Paste"], "Help"]
    MENU  3, 5, [["File", "Open", "Save", "---", "Exit"], ["Edit", "Cut", "Copy", "Paste"], "Help"] INTO choice
    
    IF choice = "Open" THEN
        ShowMessage("Hai scelto: Open")
    ELSEIF choice = "Save" THEN
        ShowMessage("Hai scelto: Save")
    ELSEIF choice = "Exit" THEN
        ShowMessage("Hai scelto: Exit")
    ELSEIF choice = "Cut" THEN
        ShowMessage("Hai scelto: Cut")
    ELSEIF choice = "Copy" THEN
        ShowMessage("Hai scelto: Copy")
    ELSEIF choice = "Paste" THEN
        ShowMessage("Hai scelto: Paste")
    ELSEIF choice = "Help" THEN
        ShowMessage("Hai scelto: Help")
    ELSE
        PRINT "Nessuna scelta"
    END IF
END SUB
