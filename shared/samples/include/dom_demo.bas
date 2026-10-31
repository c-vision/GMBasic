' Sample pacchetto dom: parse HTML inline via XPath.
REQUIRE "dom"
IMPORT DOM WITH ALIAS

SUB Main()
    PRINT "dom available="; DOM.AVAILABLE()
    DIM d AS LONG
    d = DOM.PARSE("<title>Demo</title><a href=""https://a.example"">Alfa</a>")
    PRINT "title="; DOM.TEXT(d, "//title")
    DIM ns AS LONG
    ns = DOM.SELECT(d, "//a")
    PRINT "nlinks="; DOM.NUMNODES(ns)
    PRINT "href="; DOM.NODEATTR(ns, 0, "href")
    DOM.FREENODESET(ns)
    DOM.FREE(d)
END SUB
