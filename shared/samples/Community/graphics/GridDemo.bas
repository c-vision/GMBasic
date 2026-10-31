' GridDemo.bas — GRID pixel: header cliccabili (SORT), selezione riga.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM sel = -1
SUB Main()
    WINDOW THEME "Turbo"
    GRID 40, 30, 500, 200, "Nome|Eta|Citta", "Anna|30|Roma;Bruno|25|Milano;Carla|41|Napoli;Dino|19|Torino;Elsa|37|Genova" INTO sel
    GO SUB EventLoop()
END SUB
SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
