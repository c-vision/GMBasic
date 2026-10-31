' PARITY-EXCLUDE: browser/CDP: AX tree reale Chromium vs mock headless C
' BrowserAccessTreeClickFill.bas — accessibility snapshot con ref stabili (@eN) + azione
' per ref: BROWSER.ACCESSTREE() legge l'albero AX compatto; BROWSER.CLICK("@eN") e
' BROWSER.FILL("@eN", ...) agiscono sui ref (backendDOMNodeId -> DOM.resolveNode -> callFunctionOn).
' NOTA: ACCESSTREE e il click/fill per ref @eN funzionano su tutti i backend (5/5). I @eN sono
' STABILI sullo stesso documento in tutti i backend (vedi verifica cross-backend su browser pulito),
' ma dipendono dalla struttura AX della pagina: leggere l'albero e scegliere il ref giusto.
' Richiede browser CDP attivo (GMBASIC_CDP_PORT, default 9222).
IMPORT BROWSER
SUB Main()
  PRINT "nav="; BROWSER.NAVIGATE("data:text/html,<button onclick='window._cl=true'>OK</button><input id='f' placeholder='x'>")
  PRINT "ax="; BROWSER.ACCESSTREE()
  ' su questa pagina: @e2 = button, @e3 = textbox (verificare con l'albero qui sopra)
  PRINT "click="; BROWSER.CLICK("@e2")
  PRINT "clicked-ok="; BROWSER.EVALUATE("String(window._cl === true)")
  PRINT "fill="; BROWSER.FILL("@e3","hello")
  PRINT "filled-value="; BROWSER.EVALUATE("document.getElementById('f').value")
END SUB
