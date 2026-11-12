' EXPECT: inizio g= 1000|let g= 20|let ripetuto= 21|dopo let g= 1000|implicita g= 30|dopo implicita g= 30|nuova= 7
' LET vs assegnazione IMPLICITA (spec Variables-Scope §3.1/§3.2): due semantiche diverse.
'  - `LET x = v` scrive SEMPRE nello scope corrente: se `x` esiste solo come GLOBAL crea una
'    LOCALE che la ombreggia (il globale resta intatto), e le LET successive restano su quella.
'  - `x = v` (senza LET) cerca risalendo la gerarchia: se trova il globale lo MODIFICA.
' Il caso era divergente: l'emitter C99 scriveva sul globale anche con LET (IL/PCode/VM-C no).
GLOBAL g AS LONG
g = 1000

SUB SoloLet()
    LET g = 20
    PRINT "let g="; g
    LET g = 21
    PRINT "let ripetuto="; g
END SUB

SUB SoloImplicita()
    g = 30
    PRINT "implicita g="; g
END SUB

SUB NuovaLocale()
    LET nuova = 7
    PRINT "nuova="; nuova
END SUB

SUB Main()
    PRINT "inizio g="; g
    SoloLet()
    PRINT "dopo let g="; g
    SoloImplicita()
    PRINT "dopo implicita g="; g
    NuovaLocale()
END SUB
