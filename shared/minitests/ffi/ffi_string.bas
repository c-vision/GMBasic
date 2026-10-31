' EXPECT: a=7:alpha|b=9:beta|a_after=7:alpha
' Supporto STRING (2026-09-28): parametro + ritorno, scalare LONG misto nella stessa firma.
' Verifica esplicita del copy-out: vec_label riusa un buffer statico interno — la seconda
' chiamata NON deve mutare la stringa gia' copiata dalla prima (nessun riferimento vivo al
' buffer nativo oltre la chiamata).
DECLARE FUNCTION VecLabel LIB "libvec_module" ALIAS "vec_label" (id AS LONG, name AS STRING) AS STRING

SUB Main()
    DIM a$ = VecLabel(7, "alpha")
    DIM b$ = VecLabel(9, "beta")
    PRINT "a="; a$
    PRINT "b="; b$
    PRINT "a_after="; a$
END SUB
