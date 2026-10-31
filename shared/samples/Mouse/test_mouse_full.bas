IMPORT MOUSE
IMPORT CONSOLE

SUB Main()
    CLS
    PRINT "MOUSE SYSTEM TEST"

    IF PRESENT() THEN
        PRINT "Mouse detected"
    ELSE
        PRINT "Mouse NOT detected"
    END IF

    ' Legacy syntax
    MOUSE ON
    MOUSE SHOW
    MOUSE SET 10, 10
    MOUSE LIMIT 0, 0, 40, 10
    MOUSE UNLIMIT
    MOUSE CURSOR 0
    MOUSE WAIT
    MOUSE HIDE
    MOUSE OFF

    ' Qualified syntax
    MOUSE ON
    MOUSE SHOW
    MOUSE SET 10, 10
    MOUSE LIMIT 0, 0, 40, 10
    MOUSE UNLIMIT
    MOUSE CURSOR 0
    MOUSE WAIT
    MOUSE HIDE
    MOUSE OFF

    ' Unqualified functions
    PRINT X()
    PRINT Y()
    PRINT BUTTON()
    PRINT WHEEL()
    PRINT PRESENT()
    PRINT LEFT()
    PRINT RIGHT()
    PRINT MIDDLE()
    PRINT MOVE()
    PRINT CLICK()
    PRINT DBLCLICK()

    ' Qualified functions
    PRINT X()
    PRINT Y()
    PRINT BUTTON()
    PRINT WHEEL()
    PRINT PRESENT()
    PRINT LEFT()
    PRINT RIGHT()
    PRINT MIDDLE()
    PRINT MOVE()
    PRINT CLICK()
    PRINT DBLCLICK()
END SUB
