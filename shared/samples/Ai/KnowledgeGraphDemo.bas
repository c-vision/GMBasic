' ==============================================================================
' FILE: AI/KnowledgeGraphDemo.bas
' Knowledge Graph in-memory — AI.KGADDNODE / AI.KGADDEDGE / AI.KGQUERY (B3.5, 2026-09-07)
' ==============================================================================
' Grafo conoscenza di SOLA STRUTTURA: nessun LLM, nessun provider, nessuna rete.
' I nodi sono stringhe, gli archi sono "knows" pesati 1, il grafo e' un singleton
' in-process. Risposte deterministiche (JSON testuale o 1 di conferma).
'   AI.KGADDNODE(nome)         → 1 (crea il nodo; idempotente)
'   AI.KGADDEDGE(a, b)         → 1 (crea i nodi se mancano; idempotente, no duplicati)
'   AI.KGQUERY(nodo)           → JSON {node, neighbors, incident}
'   AI.KGQUERY("path a->b")    → JSON {from, to, path} (cammino minimo BFS)
' Verificato: IL, PCodeVM, VM/C, C99 (parita' fase C) — nessun modello richiesto.
' ==============================================================================

IMPORT AI WITH ALIAS

SUB Main()
    ' 1) Nodi del dominio
    AI.KGADDNODE("GMBasic")
    AI.KGADDNODE("Lexer")
    AI.KGADDNODE("Parser")
    AI.KGADDNODE("IL")
    AI.KGADDNODE("PCodeVM")
    AI.KGADDNODE("VM-C")
    AI.KGADDNODE("C99Native")

    ' 2) Archi: come GMBasic "conosce" i suoi componenti
    AI.KGADDEDGE("GMBasic", "Lexer")
    AI.KGADDEDGE("Lexer", "Parser")
    AI.KGADDEDGE("Parser", "IL")
    AI.KGADDEDGE("Parser", "PCodeVM")
    AI.KGADDEDGE("Parser", "VM-C")
    AI.KGADDEDGE("Parser", "C99Native")
    ' idempotenza: ripetere un arco non crea duplicati
    AI.KGADDEDGE("GMBasic", "Lexer")

    ' 3) Query su un nodo: vicini uscenti + archi incidenti (JSON)
    PRINT "VICINI DI parser:"
    PRINT AI.KGQUERY("Parser")

    ' 4) Cammino minimo sul grafo
    PRINT "CAMMINO GMBasic -> IL:"
    PRINT AI.KGQUERY("path GMBasic->IL")

    PRINT "CAMMINO GMBasic -> C99Native:"
    PRINT AI.KGQUERY("path GMBasic->C99Native")

    ' 5) Errore esplicito su nodo mai inserito
    PRINT "NODO ASSENTE:"
    PRINT AI.KGQUERY("Assembler")

    PRINT "OK KnowledgeGraphDemo"
END SUB
