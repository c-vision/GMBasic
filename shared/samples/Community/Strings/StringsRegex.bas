' FILE: Strings/StringsRegex.bas
' ======================================================================================================================
' GUIDA TEORICA: REGEX — ESPRESSIONI REGOLARI NATIVE
' ======================================================================================================================
' GM-BASIC supporta espressioni regolari tramite il tipo nativo REGEX, che offre
' un'interfaccia orientata agli oggetti per il pattern matching e la sostituzione.
'
' 1. CREAZIONE E IMPOSTAZIONE PATTERN
'    DIM r AS REGEX
'    r.Pattern = "\d+"              — Imposta il pattern (regex standard .NET)
'
' 2. METODI DI RICERCA
'    r.IsMatch(input$)              — Restituisce TRUE se il pattern è presente
'    r.Match(input$)                — Restituisce un MatchObject con il primo match
'    r.Matches(input$)              — Restituisce una LIST di MatchObject con tutti i match
'
' 3. METODI DI SOSTITUZIONE E DIVISIONE
'    r.Replace(input$, replacement$) — Sostituisce tutte le occorrenze del pattern
'    r.Split(input$)                — Divide la stringa usando il pattern come separatore
'
' 4. PROPRIETÀ
'    r.Pattern                      — Il pattern regex corrente (stringa)
'
' 5. MATCH OBJECT
'    Quando r.Match() trova un risultato, restituisce un oggetto con:
'    - .Value    — La stringa catturata
'    - .Index    — La posizione (0-based) del match
'    - .Length   — La lunghezza del match
'    - .Success  — TRUE se il match è avvenuto
' ======================================================================================================================

IMPORT STRINGS

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ StringsRegex - Espressioni Regolari Native                             ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT

' --- TEST 1: IsMatch ---
10 PRINT "--- 1. TEST IsMatch ---"
15 DIM r AS REGEX
20 r.Pattern = "\d+"
25 PRINT "  Pattern: "; r.Pattern
30 PRINT "  IsMatch('abc123'):"; r.IsMatch("abc123"); " (Atteso: TRUE)"
35 PRINT "  IsMatch('abcdef'):"; r.IsMatch("abcdef"); " (Atteso: FALSE)"
40 PRINT "  IsMatch('42'):"; r.IsMatch("42"); " (Atteso: TRUE)"
45 PRINT

' --- TEST 2: Match (primo match) ---
50 PRINT "--- 2. TEST Match ---"
55 r.Pattern = "\d+"
60 DIM m = r.Match("hello 123 world 456")
65 PRINT "  Input: 'hello 123 world 456'"
70 PRINT "  Match.Value: '"; m.Value; "'"; " (Atteso: '123')"
75 PRINT "  Match.Index:"; m.Index; " (Atteso: 6)"
80 PRINT "  Match.Length:"; m.Length; " (Atteso: 3)"
85 PRINT "  Match.Success:"; m.Success; " (Atteso: TRUE)"
90 PRINT

' --- TEST 3: Matches (tutti i match) ---
95 PRINT "--- 3. TEST Matches ---"
100 r.Pattern = "\d+"
105 DIM all_matches = r.Matches("a1b22c333")
110 PRINT "  Input: 'a1b22c333'"
115 PRINT "  Numero di match:"; all_matches.Count; " (Atteso: 3)"
120 PRINT "  Match 0: '"; all_matches(0).Value; "'"; " (Atteso: '1')"
125 PRINT "  Match 1: '"; all_matches(1).Value; "'"; " (Atteso: '22')"
130 PRINT "  Match 2: '"; all_matches(2).Value; "'"; " (Atteso: '333')"
135 PRINT

' --- TEST 4: Replace ---
140 PRINT "--- 4. TEST Replace ---"
145 r.Pattern = "\d+"
150 PRINT "  Input: 'a1b22c333'"
155 PRINT "  Replace('XXX'): '"; r.Replace("a1b22c333", "XXX"); "'"; " (Atteso: 'aXXXbXXXcXXX')"
160 PRINT

' --- TEST 5: Split ---
165 PRINT "--- 5. TEST Split ---"
170 r.Pattern = "[,;]+"
175 DIM parts$() = r.Split("uno,due;tre,quattro")
180 PRINT "  Input: 'uno,due;tre,quattro'"
185 PRINT "  Separatori: [,;]+"
190 PRINT "  Parti:"; JOIN(parts$, " | ")
195 PRINT

' --- TEST 6: Pattern con gruppi ---
200 PRINT "--- 6. TEST Gruppi di Cattura ---"
205 r.Pattern = "(\w+)@(\w+)\.(\w+)"
210 DIM email_match = r.Match("contatto user@example.com qui")
215 PRINT "  Input: 'contatto user@example.com qui'"
220 PRINT "  Match.Value: '"; email_match.Value; "'"; " (Atteso: 'user@example.com')"
225 PRINT "  Match.Success:"; email_match.Success; " (Atteso: TRUE)"
230 PRINT

' --- TEST 7: Match fallito ---
235 PRINT "--- 7. TEST Match Fallito ---"
240 r.Pattern = "xyz\d+"
245 DIM fail_match = r.Match("solo lettere qui")
250 PRINT "  Match.Success:"; fail_match.Success; " (Atteso: FALSE)"
255 PRINT "  Match.Value: '"; fail_match.Value; "'"; " (Atteso: '')"
260 PRINT

' --- TEST 8: Replace con backreference ---
265 PRINT "--- 8. TEST Replace con Gruppi ---"
270 r.Pattern = "(\w+)\s(\w+)"
275 PRINT "  Replace '$2 $1': '"; r.Replace("Hello World", "$2 $1"); "'"; " (Atteso: 'World Hello')"
280 PRINT

' --- TEST 9: Validazione formato ---
285 PRINT "--- 9. TEST Validazione Formato ---"
290 r.Pattern = "^\d{2}/\d{2}/\d{4}$"
295 PRINT "  '25/06/2026' valido:"; r.IsMatch("25/06/2026"); " (Atteso: TRUE)"
300 PRINT "  '2026-06-25' valido:"; r.IsMatch("2026-06-25"); " (Atteso: FALSE)"
305 PRINT

310 PRINT "Test completato."
315 END
