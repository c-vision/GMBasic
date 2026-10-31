' FILE: GameDev/Tetris.bas — mini-game completo in BASIC puro
' Usa: SCREEN 12, TICK/TICKS, INKEY (frecce/ESC), LINE .. BF, PRINT + FONT.
' Tavola 10x20 (cella 20px). 7 pezzi con rotazione. Punteggio e righe.
IMPORT GRAPHICS
IMPORT CONSOLE

SCREEN 12
FONT "font.ttf", 12
COLOR 15, 0
CLS 0

DIM Board(199) AS INTEGER
DIM PcX(3), PcY(3), TmpX(3), TmpY(3) AS INTEGER
DIM K$ AS STRING
DIM score AS LONG, nlines AS LONG, ticks AS LONG, fall AS LONG
DIM bx AS INTEGER, by AS INTEGER, kind AS INTEGER, colr AS INTEGER
DIM over AS BOOL
DIM frames AS INTEGER
DIM I AS INTEGER, J AS INTEGER

NewPiece()
frames = 0

TICK 60
    ticks = ticks + 1
    frames = frames + 1
    INKEY K$
    IF K$ = CHR$(27) THEN over = TRUE
    IF NOT over THEN
        IF K$ = CHR$(28) THEN
            ShiftPiece(-1, 0)
        END IF
        IF K$ = CHR$(29) THEN
            ShiftPiece(1, 0)
        END IF
        IF K$ = CHR$(30) THEN
            RotatePiece()
        END IF
        IF K$ = CHR$(31) THEN fall = fall + 12
        fall = fall + 1
        IF fall >= 22 THEN
            fall = 0
            IF Collides(0, 1) THEN
                LockPiece()
            ELSE
                by = by + 1
            END IF
        END IF
    END IF
    PaintBoard()
    IF over THEN
        EXIT TICK
    END IF
    IF frames > 600 THEN
        EXIT TICK
    END IF
END TICK

DRAW TEXT "GAME OVER", 30, 520
DRAW TEXT "Score:", 30, 540
DRAW TEXT STR$(score), 90, 540
PRINT "TET "; score; " "; nlines
SCREEN 0

' ─────────────── pezzi ───────────────
SUB NewPiece
    kind = INT(RND() * 7)
    colr = kind + 2
    IF colr = 8 THEN colr = 9
    bx = 4 : by = 0
    FOR I = 0 TO 3
        PcX(I) = 0 : PcY(I) = 0
    NEXT I
    IF kind = 0 THEN
        PcX(0) = 0 : PcX(1) = 1 : PcX(2) = 2 : PcX(3) = 3
    ELSEIF kind = 1 THEN
        PcX(0) = 0 : PcY(0) = 0 : PcX(1) = 1 : PcY(1) = 0
        PcX(2) = 0 : PcY(2) = 1 : PcX(3) = 1 : PcY(3) = 1
    ELSEIF kind = 2 THEN
        PcX(0) = 0 : PcY(0) = 0 : PcX(1) = 1 : PcY(1) = 0
        PcX(2) = 2 : PcY(2) = 0 : PcX(3) = 1 : PcY(3) = 1
    ELSEIF kind = 3 THEN
        PcX(0) = 1 : PcY(0) = 0 : PcX(1) = 2 : PcY(1) = 0
        PcX(2) = 0 : PcY(2) = 1 : PcX(3) = 1 : PcY(3) = 1
    ELSEIF kind = 4 THEN
        PcX(0) = 0 : PcY(0) = 0 : PcX(1) = 1 : PcY(1) = 0
        PcX(2) = 1 : PcY(2) = 1 : PcX(3) = 2 : PcY(3) = 1
    ELSEIF kind = 5 THEN
        PcX(0) = 0 : PcY(0) = 0 : PcX(1) = 1 : PcY(1) = 0
        PcX(2) = 2 : PcY(2) = 0 : PcX(3) = 0 : PcY(3) = 1
    ELSE
        PcX(0) = 0 : PcY(0) = 0 : PcX(1) = 1 : PcY(1) = 0
        PcX(2) = 2 : PcY(2) = 0 : PcX(3) = 2 : PcY(3) = 1
    END IF
    IF Collides(0, 0) THEN over = TRUE
END SUB

FUNCTION Collides(dx AS INTEGER, dy AS INTEGER) AS BOOL
    DIM n AS INTEGER, gx AS INTEGER, gy AS INTEGER
    Collides = FALSE
    FOR n = 0 TO 3
        gx = bx + PcX(n) + dx
        gy = by + PcY(n) + dy
        IF gx < 0 OR gx > 9 OR gy > 19 THEN
            Collides = TRUE
        ELSEIF gy >= 0 THEN
            IF Board(gx + gy * 10) <> 0 THEN Collides = TRUE
        END IF
    NEXT n
END FUNCTION

SUB ShiftPiece(dx AS INTEGER, dy AS INTEGER)
    IF NOT Collides(dx, dy) THEN
        bx = bx + dx
        by = by + dy
    END IF
END SUB

SUB RotatePiece
    DIM n AS INTEGER, nx AS INTEGER, ny AS INTEGER, mnx AS INTEGER, mny AS INTEGER
    mnx = 999 : mny = 999
    FOR n = 0 TO 3
        nx = -PcY(n)
        ny = PcX(n)
        TmpX(n) = nx : TmpY(n) = ny
        IF nx < mnx THEN mnx = nx
        IF ny < mny THEN mny = ny
    NEXT n
    FOR n = 0 TO 3
        PcX(n) = TmpX(n) - mnx
        PcY(n) = TmpY(n) - mny
    NEXT n
    IF Collides(0, 0) THEN
        mnx = 999 : mny = 999
        FOR n = 0 TO 3
            nx = PcY(n)
            ny = -PcX(n)
            TmpX(n) = nx : TmpY(n) = ny
            IF nx < mnx THEN mnx = nx
            IF ny < mny THEN mny = ny
        NEXT n
        FOR n = 0 TO 3
            PcX(n) = TmpX(n) - mnx
            PcY(n) = TmpY(n) - mny
        NEXT n
    END IF
END SUB

SUB LockPiece
    DIM n AS INTEGER, gx AS INTEGER, gy AS INTEGER, yy AS INTEGER, xx AS INTEGER
    DIM full AS BOOL
    FOR n = 0 TO 3
        gx = bx + PcX(n)
        gy = by + PcY(n)
        IF gy >= 0 AND gy <= 19 AND gx >= 0 AND gx <= 9 THEN Board(gx + gy * 10) = colr
    NEXT n
    FOR yy = 19 TO 0 STEP -1
        full = TRUE
        FOR xx = 0 TO 9
            IF Board(xx + yy * 10) = 0 THEN full = FALSE
        NEXT xx
        IF full THEN
            FOR I = yy TO 1 STEP -1
                FOR xx = 0 TO 9
                    Board(xx + I * 10) = Board(xx + (I - 1) * 10)
                NEXT xx
            NEXT I
            FOR xx = 0 TO 9
                Board(xx) = 0
            NEXT xx
            nlines = nlines + 1
            score = score + 100
            yy = yy + 1
        END IF
    NEXT yy
    NewPiece()
END SUB

SUB PaintBoard
    DIM xx AS INTEGER, yy AS INTEGER, c AS INTEGER, gx AS INTEGER, gy AS INTEGER, n AS INTEGER
    CLS 0
    LINE (18, 98)-(222, 502), 7, "B"
    FOR yy = 0 TO 19
        FOR xx = 0 TO 9
            c = Board(xx + yy * 10)
            IF c <> 0 THEN
                LINE (20 + xx * 20, 100 + yy * 20)-(38 + xx * 20, 118 + yy * 20), c, "BF"
            END IF
        NEXT xx
    NEXT yy
    FOR n = 0 TO 3
        gx = bx + PcX(n)
        gy = by + PcY(n)
        IF gy >= 0 THEN
            LINE (20 + gx * 20, 100 + gy * 20)-(38 + gx * 20, 118 + gy * 20), colr, "BF"
        END IF
    NEXT n
    COLOR 15, 0
    DRAW TEXT "TETRIS", 240, 40
    DRAW TEXT "Score:", 240, 66
    DRAW TEXT STR$(score), 300, 66
    DRAW TEXT "Lines:", 240, 88
    DRAW TEXT STR$(nlines), 300, 88
    DRAW TEXT "Frecce: muovi", 240, 130
    DRAW TEXT "Su: ruota", 240, 150
    DRAW TEXT "Giu: accelera", 240, 170
    DRAW TEXT "ESC: esci", 240, 190
END SUB
