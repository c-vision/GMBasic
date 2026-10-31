' FILE: Strings/StringsStringBuilder.bas
' ======================================================================================================================
' GUIDA TEORICA: STRINGBUILDER — COSTRUZIONE EFFICIENTE DI STRINGHE
' ======================================================================================================================
' La concatenazione ripetuta di stringhe con l'operatore + o & in BASIC crea una nuova stringa
' ad ogni operazione, con un costo O(n*m). STRINGBUILDER risolve questo problema usando un
' buffer interno pre-allocato che cresce dinamicamente, portando il costo ammortizzato a O(n+m).
'
' 1. CREAZIONE
'    DIM sb AS STRINGBUILDER        — Crea un StringBuilder vuoto (capacità default: 16)
'    DIM sb AS STRINGBUILDER(256)    — Crea con capacità iniziale specificata
'
' 2. METODI DI ACCODAMENTO
'    sb.Append(value)               — Accode qualsiasi valore (convertito in stringa)
'    sb.AppendLine(value)           — Accode un valore seguito da un a capo (CRLF)
'    sb.AppendFormat(fmt, arg1, …)  — Accode usando una format string ({0}, {1:F2}, ecc.)
'
' 3. METODI DI MODIFICA
'    sb.Insert(index, value)        — Inserisce un valore alla posizione specificata
'    sb.Replace(oldStr, newStr)     — Sostituisce tutte le occorrenze di una sottostringa
'    sb.Remove(startIndex, length)  — Rimuove i caratteri da startIndex per length
'    sb.Clear()                     — Rimuove tutto il contenuto
'
' 4. PROPRIETÀ E OUTPUT
'    sb.Length                      — Lunghezza corrente del contenuto
'    sb.Capacity                    — Capacità del buffer interno
'    sb.ToString()                  — Restituisce il contenuto come stringa
' ======================================================================================================================

IMPORT STRINGS

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ StringsStringBuilder - Costruzione Efficiente di Stringhe              ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT

' --- TEST 1: Append basilare ---
10 PRINT "--- 1. TEST Append ---"
15 DIM sb AS STRINGBUILDER
20 sb.Append("Hello")
25 sb.Append(" ")
30 sb.Append("World")
35 PRINT "  Risultato: '"; sb.ToString(); "'"; " (Atteso: 'Hello World')"
40 PRINT

' --- TEST 2: AppendLine ---
45 PRINT "--- 2. TEST AppendLine ---"
50 DIM sb2 AS STRINGBUILDER
55 sb2.AppendLine("Riga 1")
60 sb2.AppendLine("Riga 2")
65 PRINT "  ToString: '"; sb2.ToString(); "'"
70 PRINT "  Length:"; sb2.Length; " (Atteso: 14 con CRLF)"
75 PRINT

' --- TEST 3: AppendFormat ---
80 PRINT "--- 3. TEST AppendFormat ---"
85 DIM sb3 AS STRINGBUILDER
90 sb3.AppendFormat("Nome: {0}, Età: {1}", "Mario", 30)
95 PRINT "  Risultato: '"; sb3.ToString(); "'"; " (Atteso: 'Nome: Mario, Età: 30')"
100 PRINT

' --- TEST 4: Insert ---
105 PRINT "--- 4. TEST Insert ---"
110 DIM sb4 AS STRINGBUILDER
115 sb4.Append("Hello World")
120 sb4.Insert(5, " Beautiful")
125 PRINT "  Risultato: '"; sb4.ToString(); "'"; " (Atteso: 'Hello Beautiful World')"
130 PRINT

' --- TEST 5: Replace ---
135 PRINT "--- 5. TEST Replace ---"
140 DIM sb5 AS STRINGBUILDER
145 sb5.Append("gatto gatto gatto")
150 sb5.Replace("gatto", "cane")
155 PRINT "  Risultato: '"; sb5.ToString(); "'"; " (Atteso: 'cane cane cane')"
160 PRINT

' --- TEST 6: Remove ---
165 PRINT "--- 6. TEST Remove ---"
170 DIM sb6 AS STRINGBUILDER
175 sb6.Append("0123456789")
180 sb6.Remove(3, 4)
185 PRINT "  Risultato: '"; sb6.ToString(); "'"; " (Atteso: '012789')"
190 PRINT

' --- TEST 7: Proprietà Length e Capacity ---
195 PRINT "--- 7. TEST Length e Capacity ---"
200 DIM sb7 AS STRINGBUILDER
205 sb7.Append("ABCDEFGH")
210 PRINT "  Length:"; sb7.Length; " (Atteso: 8)"
215 PRINT "  Capacity:"; sb7.Capacity; " (>= 8)"
220 PRINT

' --- TEST 8: Clear ---
225 PRINT "--- 8. TEST Clear ---"
230 DIM sb8 AS STRINGBUILDER
235 sb8.Append("Contenuto da cancellare")
240 sb8.Clear()
245 PRINT "  Length dopo Clear:"; sb8.Length; " (Atteso: 0)"
250 PRINT "  ToString: '"; sb8.ToString(); "'"; " (Atteso: '')"
255 PRINT

' --- TEST 9: Costruzione progressiva (benchmark pattern) ---
260 PRINT "--- 9. TEST Costruzione Progressiva ---"
265 DIM csv AS STRINGBUILDER
270 FOR i = 0 TO 4
275     IF i > 0 THEN csv.Append(",")
280     csv.AppendFormat("item{0}", i)
285 NEXT i
290 PRINT "  CSV: '"; csv.ToString(); "'"; " (Atteso: 'item0,item1,item2,item3,item4')"
295 PRINT

' --- TEST 10: Concatenazione con tipi misti ---
300 PRINT "--- 10. TEST Tipi Misti ---"
305 DIM sb10 AS STRINGBUILDER
310 DIM pi = 3.14159
315 DIM flag = 1
320 sb10.Append("Valore: ")
325 sb10.Append(pi)
330 sb10.Append(" / Flag: ")
335 sb10.Append(flag)
340 PRINT "  Risultato: '"; sb10.ToString(); "'"; " (Atteso: 'Valore: 3.14159 / Flag: 1')"
345 PRINT

350 PRINT "Test completato."
355 END
