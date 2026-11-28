' Animazione — proprietà `anim` di CONTROL SET (motore tick-driven).
' `CONTROL SET nome, "anim", 0, "prop:valore:ms[:easing]"`: il controllo va dal
' valore corrente al target in `ms` millisecondi. L'avanzamento lo fa il FRAME
' (nessun thread, nessun timer): qui si verifica il caso DETERMINISTICO `ms=0`,
' che porta subito il valore al target (stessa semantica di ui.reduce_motion),
' più il fatto che con durata > 0 il valore NON salta nell'istante dello SET.
' Parità 4 backend: sono chiavi dati, nessun comando nuovo.
' EXPECT: anim-left-ok|anim-top-ok|anim-width-ok|anim-nojump-ok
IMPORT WINDOWS
IMPORT CONTROLS

DIM gx AS DOUBLE
DIM gy AS DOUBLE
DIM gw AS DOUBLE
DIM gh AS DOUBLE

SUB Rect(nome AS STRING)
    CONTROL GET nome, "left"   INTO gx
    CONTROL GET nome, "top"    INTO gy
    CONTROL GET nome, "width"  INTO gw
    CONTROL GET nome, "height" INTO gh
END SUB

SUB Main()
    WINDOW RESIZE 640, 320
    BUTTON 10, 10, 20, 2, "a" NAME "anA"

    ' durata 0 = snap immediato sul target
    CONTROL SET "anA", "anim", 0, "left:30:0"
    CALL Rect("anA")
    IF gx = 30 THEN PRINT "anim-left-ok" ELSE PRINT "anim-left-no"

    CONTROL SET "anA", "anim", 0, "top:12:0:out"
    CALL Rect("anA")
    IF gy = 12 THEN PRINT "anim-top-ok" ELSE PRINT "anim-top-no"

    CONTROL SET "anA", "anim", 0, "width:40:0"
    CALL Rect("anA")
    IF gw = 40 THEN PRINT "anim-width-ok" ELSE PRINT "anim-width-no"

    ' durata 500 ms: il valore di partenza NON cambia nello stesso istante
    ' (il movimento lo produce il frame, non la SET)
    CONTROL SET "anA", "anim", 0, "left:200:500:out"
    CALL Rect("anA")
    IF gx = 30 THEN PRINT "anim-nojump-ok" ELSE PRINT "anim-nojump-no"
END SUB
