' TAG: interactive
' PRINT di valori numerici (target @GET DYNAMIC e DIM): formattazione identica alle altre gambe
' (" " + G15 + " ") — nessun troncamento di tipo.
' EXPECT: qta=[ 5 ]|qtb=[ 42 ]
DYNAMIC q AS DOUBLE = 0
@ 2, 10 GET q
READ
PRINT "qta=["; q; "]"
DIM p AS DOUBLE
p = 42
PRINT "qtb=["; p; "]"
