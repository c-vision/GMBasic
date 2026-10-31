' MessageBoxDemo.bas — ALERT/CONFIRM in grafica sono VERE FINESTRE (barra titolo,
' corpo, X) con messaggio e bottoni stilizzati (stesso stile dei BUTTON + tema).
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM esito = 0

SUB Main()
    WINDOW THEME "Nord"
    BUTTON 40, 30, 260, 44, "Chiedi conferma" ON CLICK OnAsk STYLE "rounded glow=#22d3ee bg=#0b1220 fg=#67e8f9"
    BUTTON 40, 90, 260, 44, "Mostra avviso"  ON CLICK OnAlert STYLE "flat gradient=#3b82f6,#1e3a8a fg=white"
    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB

SUB OnAsk()
    esito = CONFIRM("Salvare le modifiche?")
    IF esito = 1 THEN
        ALERT("Modifiche salvate.")
    ELSE
        ALERT("Operazione annullata.")
    END IF
END SUB

SUB OnAlert()
    ALERT("Questo e' un avviso.")
END SUB
