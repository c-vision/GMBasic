' ClipperDynamicWorkArea.bas — DYNAMIC come work area Clipper/xBase (PRIVATE).
' Cascata lungo la catena di chiamate, shadowing, ripristino e write-back cross-frame.
SUB ApriArea(nome AS STRING)
    DYNAMIC area AS STRING = nome
    DYNAMIC record AS STRING = "0"
    CALL LeggiPrimo()
    PRINT "ApriArea:"; area; " record="; record
END SUB

SUB LeggiPrimo()
    PRINT "LeggiPrimo:"; area          ' cascata: vede la DYNAMIC del chiamante
    record = "1"                       ' scrive sul frame del chiamante
    CALL Ombreggia()
    PRINT "LeggiPrimo:"; area; "/"; record
END SUB

SUB Ombreggia()
    DYNAMIC area AS STRING = "temporanea"   ' shadowing
    PRINT "Ombreggia:"; area
END SUB

PRINT "start"
CALL ApriArea("clienti")
PRINT "end"
