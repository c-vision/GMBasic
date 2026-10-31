' MiniTest compile/arity per le funzioni BROWSER senza un test dedicato.
' Le chiamate sono in un ramo IF 0: il compilatore/linker deve risolverle su tutti
' i backend, senza avviare un browser reale.
' EXPECT: browser-symbols-ok
IMPORT BROWSER

SUB Main()
  IF 0 THEN
    DIM s1$ = BROWSER.NAVIGATE("about:blank")
    DIM s2$ = BROWSER.SNAPSHOT()
    DIM s3$ = BROWSER.EVALUATE("1+1")
    DIM s4$ = BROWSER.ACCESSTREE()
    DIM s5$ = BROWSER.GETCOOKIES()
    DIM s6$ = BROWSER.SETCOOKIE("n", "v", "http://localhost/")
    DIM s7$ = BROWSER.CLEARCOOKIES()
    DIM s8$ = BROWSER.SETUSERAGENT("gmbasic-test")
    DIM s9$ = BROWSER.HUMANCLICK("#id")
    DIM s10$ = BROWSER.HUMANFILL("#id", "value")
  END IF
  PRINT "browser-symbols-ok"
END SUB
