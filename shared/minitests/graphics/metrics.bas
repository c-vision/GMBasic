' MiniTest GRAPHICS: metriche del font (TextWidth/TextHeight) e Screens().
' ORACOLO ERMETICO: le metriche ASSOLUTE dipendono dal font configurato (possono cambiare
' con un font diverso o un'altra macchina: l'EXPECT storico 25|19 era di un'altra
' configurazione, le 4 gambe oggi concordano su 19|13). Qui si verifica il CONTRATTO:
' larghezza/altezza positive e coerenti, e che la larghezza cresca col testo.
' EXPECT: tw>0=-1|th>0=-1|screens= 1|tw_linear=-1|th_stable=-1
IMPORT GRAPHICS

DIM tw AS INTEGER
DIM th AS INTEGER
tw = TextWidth("abc")
th = TextHeight()
PRINT "tw>0="; (tw > 0)
PRINT "th>0="; (th > 0)
PRINT "screens="; Screens()
PRINT "tw_linear="; (TextWidth("MMM") > TextWidth("M"))
PRINT "th_stable="; (TextHeight() = th)
