SCREEN 12
DIM n AS STRING = ""
DIM q = 0
@ 2, 2 SAY "gfx-say"
@ 4, 2 GET n VALID LEN(n) > 0 MESSAGE "obbligatorio"
@ 6, 2 GET q RANGE 0, 120 VALID q >= 18 ERROR "maggiorenne"
READ
CPRINT "n=[" + n + "] q=" + STR(q)
