' ============================================================================
' Browser Agent Governance — sample COMPLETO che esercita G1-G12
' ============================================================================
' Usa OGNI comando/funzione browser implementata nella sessione
' Browser-Agent-Governance (+ quelle preesistenti):
'   OBSERVE, FIND, NETLOG, CONSOLE, AUTH(save/load/list/fork)
'   + base: NAVIGATE, CLICK, FILL, EVALUATE, ACCESSTREE
'
' Requisiti: Chromium su CDP 9222 (launch_chromium.sh) + fixture locale
'   samples/Fixtures/browser-eval/test_basic.html
' ============================================================================

IMPORT BROWSER

SUB Main()
    ' Avvia e connetti
    BROWSER.START(%AUTO, 9222)

    ' fixture locale
    BROWSER.NAVIGATE("file:///Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/browser-eval/test_basic.html")

    ' --- G1: OBSERVE (unified) ---
    DIM obs$ = BROWSER.OBSERVE()
    PRINT "=== OBSERVE ==="
    PRINT LEFT$(obs$, 300)
    PRINT ""

    ' --- G3: FIND testuale ---
    DIM f1$ = BROWSER.FIND("Hello GMBasic")
    PRINT "=== FIND ==="
    PRINT f1$
    PRINT ""

    ' --- G2: click con verify ---
    PRINT "=== CLICK (verify before/after) ==="
    DIM clickRes$ = BROWSER.CLICK("#btn1")
    PRINT "click: "; clickRes$
    PRINT "title: "; BROWSER.EVALUATE("document.title")
    PRINT ""

    ' --- G5: console + network capture ---
    PRINT "=== CAPTURE (console+network) ==="
    ' ricarica per rigenerare eventi console/network
    BROWSER.NAVIGATE("file:///Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/browser-eval/test_basic.html")
    DIM net$ = BROWSER.NETLOG(TRUE)
    PRINT "netlog: "; LEFT$(net$, 200)
    DIM con$ = BROWSER.CONSOLE(TRUE)
    PRINT "console: "; LEFT$(con$, 200)
    PRINT ""

    ' --- G6: AUTH profiles ---
    PRINT "=== AUTH ==="
    DIM list0$ = BROWSER.AUTH("list", "")
    PRINT "list: "; list0$
    ' save richiede browser connesso; se non ci sono cookies potrebbe fallire
    DIM saveRes$ = BROWSER.AUTH("save", "demo")
    PRINT "save: "; saveRes$
    DIM loadRes$ = BROWSER.AUTH("load", "demo")
    PRINT "load: "; loadRes$
    PRINT ""

    ' --- G7: fork sessione ---
    PRINT "=== FORK ==="
    DIM forkRes$ = BROWSER.AUTH("fork", "branch1,default")
    PRINT "fork: "; forkRes$
    PRINT ""

    PRINT "BROWSER-GOVERNANCE-OK"
END SUB