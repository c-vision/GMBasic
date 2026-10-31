' TreeFileDemo.bas — modalita' grafica: TREEVIEW (gerarchico) + FILEDIALOG (elenco file).
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM t = 0
DIM f AS STRING
SUB Main()
    WINDOW THEME "Turbo"
    TREEVIEW 40, 30, 300, 200, "Progetto/src|Progetto/src/main.c|Progetto/include|Progetto/include/a.h|Altro/x" INTO t
    FILEDIALOG 40, 250, 320, 180, "." INTO f
    GO SUB EventLoop()
END SUB
SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
