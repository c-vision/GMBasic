' FILE: Types/TypesJson.bas
' ======================================================================================================================
' GUIDA TEORICA: MANIPOLAZIONE DI DATI JSON
' ======================================================================================================================
' GM-BASIC fornisce un supporto nativo di prima classe per la manipolazione di dati in formato JSON (JavaScript
' Object Notation), un formato standard per lo scambio di dati.
'
' 1. IL TIPO DI DATO `JSON`
'    - `DIM myJson AS JSON`: Dichiara una variabile in grado di contenere un oggetto o un array JSON.
'    - A differenza degli UDT, il tipo `JSON` è dinamico. Si possono aggiungere, modificare o rimuovere
'      proprietà a runtime senza una definizione di struttura predeterminata.
'
' 2. FUNZIONI E COMANDI DEL NAMESPACE `JSON`
'    - `JSON.Parse(stringa$) AS JSON`: Converte una stringa di testo in formato JSON in un oggetto `JSON`
'      manipolabile.
'    - `JSON.Stringify(oggetto_json) AS STRING`: Converte un oggetto `JSON` in una stringa di testo formattata.
'    - `JSON.SAVE oggetto_json, percorso$`: Comando per salvare un oggetto `JSON` direttamente su un file di testo.
'    - `JSON.LOAD(percorso$) AS JSON`: Funzione per caricare e parsare un oggetto `JSON` da un file.
'
' 3. SINTASSI DI ACCESSO
'    - Accesso a Proprietà (Oggetti): Si utilizza la notazione punto (`.`), proprio come per gli UDT.
'      Esempio: `myJson.name`
'    - Accesso a Elementi (Array): Si utilizza la notazione con parentesi quadre (`[]`), con indici a base 0.
'      Esempio: `myJson.courses[0]`
'    - Accesso a Catena: La sintassi può essere combinata per navigare strutture complesse.
'      Esempio: `myJson.address.street`
'
' 4. MANIPOLAZIONE
'    - Modifica: `myJson.age = 43`
'    - Aggiunta: `myJson.city = "Milano"`
'    - L'Executor gestisce dinamicamente la struttura interna dell'oggetto `JSON` a runtime.
' ======================================================================================================================

' Importa il namespace JSON per accedere a Parse, Stringify, SAVE, LOAD.
IMPORT JSON
IMPORT IO

REM === TEST COMPLETO DEL TIPO E DELLE FUNZIONI JSON ===

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ TypesJson - Tipo JSON                                                   ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
20 PRINT "--- 1. Parsing di una stringa JSON con JSON.Parse ---"
30 DIM DATA$ AS STRING
' Stringa JSON complessa con oggetti, array e tipi diversi.
40 DATA$ = "{ ""name"": ""Mario"", ""age"": 42, ""isStudent"": false, ""courses"": [""CS101"", ""MA203""], ""address"": { ""street"": ""Via Roma 10"" } }"
50 PRINT "Stringa JSON di input:"
60 PRINT DATA$
70 PRINT

' Dichiara una variabile di tipo JSON.
80 DIM myJson AS JSON
' Parsa la stringa e assegna l'oggetto risultante alla variabile.
90 myJson = JSON.Parse(DATA$)
100 PRINT "Parsing completato."
110 PRINT

120 PRINT "--- 2. Lettura dei valori dall'oggetto JSON ---"
' Accesso alle proprietà con la notazione punto.
130 PRINT "Nome: "; myJson.name; " (Atteso: Mario)"
140 PRINT "Età: "; myJson.age; " (Atteso: 42)"
150 PRINT "E' studente: "; myJson.isStudent; " (Atteso: False)"
' Accesso a catena: prima l'oggetto 'address', poi la sua proprietà 'street'.
155 PRINT "Indirizzo: "; myJson.address.street; " (Atteso: Via Roma 10)"
' Accesso a un elemento di un array JSON.
160 PRINT "Primo corso: "; myJson.courses[0]; " (Atteso: CS101)"
170 PRINT

180 PRINT "--- 3. Modifica dei valori e aggiunta di un nuovo campo ---"
' Modifica una proprietà esistente.
190 myJson.age = 43
' Aggiunge una nuova proprietà che non esisteva prima.
200 myJson.city = "Milano"
210 PRINT "Modificata età a 43 e aggiunta città 'Milano'."
220 PRINT "Nuova età: "; myJson.age; " (Atteso: 43)"
230 PRINT "Nuova città: "; myJson.city; " (Atteso: Milano)"
240 PRINT

250 PRINT "--- 4. Serializzazione dell'oggetto modificato con JSON.Stringify ---"
260 DIM MODIFIED_DATA$ AS STRING
' Converte l'oggetto JSON in memoria di nuovo in una stringa.
270 MODIFIED_DATA$ = JSON.Stringify(myJson)
280 PRINT "Stringa risultante:"
290 PRINT MODIFIED_DATA$
300 PRINT

310 PRINT "--- 5. Salvataggio su file con JSON.Save ---"
' Comando per scrivere l'oggetto JSON su disco.
315 IF NOT IO.DIREXISTS("work") THEN MKDIR "work"
320 JSON SAVE myJson, "work/test_output.json"
330 PRINT "File salvato."
370 PRINT

380 PRINT "--- 6. Caricamento da file con JSON.Load ---"
390 DIM loadedJson AS JSON
400 loadedJson = JSON.Load("work/test_output.json")
410 PRINT "Caricamento completato."
420 PRINT "Verifica del dato caricato:"
430 PRINT "Nome dal file: "; loadedJson.name; " (Atteso: Mario)"
440 PRINT "Età dal file: "; loadedJson.age; " (Atteso: 43)"
450 PRINT

500 PRINT "--- Test JSON completato. ---"
510 END