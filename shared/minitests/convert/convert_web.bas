' MiniTest CONVERT.WEB — conversione URL→markdown. La rete non è deterministica, quindi
' si verifica il CONTRATTO D'ERRORE (fetch fallita): marcatore ESPLICITO e identico sui
' 4 backend (prima: C# "?Error: <msg>" vs C NULL→"" silent-wrong). Success path: vedi guida.
' EXPECT: web=?Error: web fetch failed
IMPORT CONVERT

SUB Main()
    PRINT "web="; CONVERT.WEB("http://127.0.0.1:1/x")
END SUB
