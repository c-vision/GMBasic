' VAULT WATCH — policy di indicizzazione del vault (Second Brain F7 P2).
' Il watchdog è OPZIONALE: VAULT INDEX manuale resta il primario; AUTO avvia un
' FileSystemWatcher che reindicizza on-change (create/change/delete/rename).
' Per-sessione: il watcher vive finché il programma gira.

IMPORT AI WITH ALIAS

' Crea un vault temporaneo e lo indicizza (primario).
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$
VAULT$ = WORK$ + "/watch-vault"
IF IO.DirExists(VAULT$) = 0 THEN MKDIR VAULT$

AI VAULT CREATE "watchv", VAULT$
AI VAULT INDEX "watchv"

' AI.VAULTPOLICY(name) — policy corrente (MANUAL di default).
PRINT "POLICY default:", AI.VAULTPOLICY("watchv")

' AI VAULT WATCH name, AUTO — avvia il watchdog (reindicizza on-change).
AI VAULT WATCH "watchv", AUTO
PRINT "POLICY after AUTO:", AI.VAULTPOLICY("watchv")

' AI VAULT WATCH name, MANUAL — ferma il watchdog (VAULT INDEX esplicito).
AI VAULT WATCH "watchv", MANUAL
PRINT "POLICY after MANUAL:", AI.VAULTPOLICY("watchv")

PRINT "VaultWatch ok"
