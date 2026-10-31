' FILE: GameDev/PacMan.bas — mini-game completo in BASIC puro
' Labirinto a celle 16px, pallini, giocatore (frecce) e un fantasma che insegue.
' Usa: SCREEN 12, TICK/TICKS, INKEY, LINE .. BF, PSET, PRINT + FONT.
IMPORT GRAPHICS
IMPORT CONSOLE

DIM WCell AS INTEGER
DIM MCols AS INTEGER
DIM MRows AS INTEGER
DIM Wall(314) AS INTEGER
DIM Dot(314) AS INTEGER
DIM K$ AS STRING
DIM mdir AS INTEGER, want AS INTEGER
DIM plx AS INTEGER, ply AS INTEGER
DIM ghx AS INTEGER, ghy AS INTEGER
DIM mvtick AS INTEGER, gvtick AS INTEGER
DIM score AS LONG, dots AS INTEGER, eaten AS INTEGER
DIM over AS BOOL, win AS BOOL
DIM frames AS INTEGER
DIM I AS INTEGER, J AS INTEGER, mx AS INTEGER, my AS INTEGER
DIM dx AS INTEGER, dy AS INTEGER, tx AS INTEGER, ty AS INTEGER
DIM moved AS BOOL

WCell = 16
MCols = 21
MRows = 15

InitMaze()
CLS 0
frames = 0

TICK 60
    frames = frames + 1
    INKEY Kn    IF K$ = CHR$(27) THEN over = TRUE
    IF K$ = CHR$(28) THEN want = 0
    IF K$ = CHR$(29) THEN want = 2
    IF K$ = CHR$(30) THEN want = 3
    IF K$ = CHR$(31) THEN want = 1
    IF NOT over THEN
        mvtick = mvtick + 1
        IF mvtick >= 6 THEN
            mvtick = 0
            mdir = want
            dx = 0 : dy = 0
            IF mdir = 0 THEN dx = -1
            IF mdir = 1 THEN dy = 1
            IF mdir = 2 THEN dx = 1
            IF mdir = 3 THEN dy = -1
            tx = plx + dx : ty = ply + dy
            IF tx >= 0 AND tx < MCols AND ty >= 0 AND ty < MRows THEN
                IF Wall(tx + ty * MCols) = 0 THEN
                    plx = tx : ply = ty
                END IF
            END IF
            IF Dot(plx + ply * MCols) = 1 THEN
                Dot(plx + ply * MCols) = 0
                eaten = eaten + 1
                score = score + 10
                IF eaten >= dots THEN win = TRUE : over = TRUE
            END IF
        END IF
        gvtick = gvtick + 1
        IF gvtick >= 11 THEN
            gvtick = 0
            dx = 0 : dy = 0
            IF ghx < plx THEN dx = 1
            IF ghx > plx THEN dx = -1
            IF dx = 0 THEN
                IF ghy < ply THEN dy = 1
                IF ghy > ply THEN dy = -1
            END IF
            tx = ghx + dx : ty = ghy + dy
            moved = FALSE
            IF tx >= 0 AND tx < MCols AND ty >= 0 AND ty < MRows THEN
                IF Wall(tx + ty * MCols) = 0 THEN
                    ghx = tx : ghy = ty
                    moved = TRUE
                END IF
            END IF
            IF NOT moved THEN
                dx = 0 : dy = 0
                IF ghy < ply THEN dy = 1
                IF ghy > ply THEN dy = -1
                IF dy = 0 THEN
                    IF ghx < plx THEN dx = 1
                    IF ghx > plx THEN dx = -1
                END IF
                tx = ghx + dx : ty = ghy + dy
                IF tx >= 0 AND tx < MCols AND ty >= 0 AND ty < MRows THEN
                    IF Wall(tx + ty * MCols) = 0 THEN
                        ghx = tx : ghy = ty
                    END IF
                END IF
            END IF
            IF ghx = plx AND ghy = ply THEN over = TRUE
        END IF
    END IF
    PaintScreen()
    IF over THEN
        EXIT TICK
    END IF
    IF frames > 600 THEN
        EXIT TICK
    END IF
END TICK

DRAW TEXT "GAME OVER", 8, 300
DRAW TEXT STR$(score), 8, 320
PRINT "PAC "; score; " "; eaten
SCREEN 0

SUB InitMaze
    DIM x AS INTEGER, y AS INTEGER, n AS INTEGER
    n = 0
    FOR y = 0 TO MRows - 1
        FOR x = 0 TO MCols - 1
            Wall(x + y * MCols) = 0
            IF x = 0 OR x = MCols - 1 OR y = 0 OR y = MRows - 1 THEN Wall(x + y * MCols) = 1
            IF (y = 5 AND x >= 3 AND x <= 17) THEN Wall(x + y * MCols) = 1
            IF (y = 9 AND x >= 3 AND x <= 17) THEN Wall(x + y * MCols) = 1
            IF (x = 10 AND y >= 6 AND y <= 8) THEN Wall(x + y * MCols) = 1
        NEXT x
    NEXT y
    dots = 0
    FOR y = 1 TO MRows - 2
        FOR x = 1 TO MCols - 2
            IF Wall(x + y * MCols) = 0 THEN
                Dot(x + y * MCols) = 1
                dots = dots + 1
            END IF
        NEXT x
    NEXT y
    plx = 1 : ply = 1
    ghx = 19 : ghy = 13
    Dot(plx + ply * MCols) = 0
    Dot(ghx + ghy * MCols) = 0
    dots = dots - 2
    eaten = 0
    score = 0
    mdir = 2 : want = 2
    mvtick = 0 : gvtick = 0
END SUB

SUB PaintScreen
    DIM x AS INTEGER, y AS INTEGER
    CLS 0
    DRAW TEXT "PAC-MAN", 8, 8
    DRAW TEXT "Score:", 8, 26
    DRAW TEXT STR$(score), 60, 26
    DRAW TEXT "Pallini:", 120, 26
    DRAW TEXT STR$(dots - eaten), 180, 26
    FOR y = 0 TO MRows - 1
        FOR x = 0 TO MCols - 1
            IF Wall(x + y * MCols) = 1 THEN
                LINE (x * WCell, y * WCell + 40)-(x * WCell + WCell - 1, y * WCell + WCell + 39), 1, "BF"
            ELSE
                IF Dot(x + y * MCols) = 1 THEN PSET (x * WCell + 7, y * WCell + 47), 14
            END IF
        NEXT x
    NEXT y
    LINE (ghx * WCell + 2, ghy * WCell + 42)-(ghx * WCell + WCell - 3, ghy * WCell + WCell + 37), 12, "BF"
    LINE (plx * WCell + 1, ply * WCell + 41)-(plx * WCell + WCell - 2, ply * WCell + WCell + 38), 10, "BF"
    COLOR 15, 0

END SUB
