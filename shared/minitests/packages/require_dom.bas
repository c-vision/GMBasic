' TAG: unsupported-c99native
' EXPECT: available= 1|title=Demo|nlinks= 1|href=https://a.example
' REQUIRE (package manager) + FFI DOM (namespace DOM, HTML/XML su libxml2).
'   REQUIRE "dom" risolve il pacchetto (~/.gmbasic/packages) e include dom.inc.
'   Deterministico: parse di stringhe HTML inline (nessuna rete).
REQUIRE "dom"
IMPORT DOM WITH ALIAS

PRINT "available="; DOM.AVAILABLE()

DIM d AS LONG
d = DOM.PARSE("<title>Demo</title><a href=""https://a.example"">Alfa</a>")
PRINT "title="; DOM.TEXT(d, "//title")
DIM ns AS LONG
ns = DOM.SELECT(d, "//a")
PRINT "nlinks="; DOM.NUMNODES(ns)
PRINT "href="; DOM.NODEATTR(ns, 0, "href")
DOM.FREENODESET(ns)
DOM.FREE(d)
