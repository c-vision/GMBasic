' FILE: Strings/StringsInterpolation.bas
' =================================================================================================
'       GUIDA TEORICA: STRINGHE INTERPOLATE ($"...")
' =================================================================================================
' GM-BASIC supporta l'interpolazione di stringhe, che permette di inserire espressioni direttamente
' dentro una stringa template usando la sintassi `$"..."`.
'
' 1. SINTASSI
'    - `$"testo {espressione} testo"`: Le parentesi graffe racchiudono un'espressione BASIC che verrà
'      valutata e convertita in stringa.
'    - `$"testo {{letterale}}"`: Le doppie parentesi graffe producono parentesi graffe letterali.
'
' 2. ESPRESSIONI SUPPORTATE
'    - Variabili: `$"Hello {name}"`
'    - Aritmetiche: `$"Result: {a + b}"`
'    - Chiamate funzione: `$"Length: {LEN(text)}"`
'    - Ternary: `$"Status: {IIF(active, "ON", "OFF")}"`
'    - Format: `$"{value:##0.00}"` (format string opzionale dopo `:`)
'
' 3. REGOLE
'    - Il `$"` apre la stringa interpolata, il `"` la chiude.
'    - `{{` produce `{`, `}}` produce `}`.
'    - Le espressioni dentro `{}` possono essere qualsiasi espressione valida.
'    - Le stringhe interpolate possono essere concatenate con `+` o `&`.
'    - Funzionano con PRINT, assegnazione, e come argomento di funzione.
'
' =================================================================================================

IMPORT STRINGS
IMPORT CONSOLE

SUB Main()

    CLS
    PRINT "=========================================="
    PRINT "  TEST: Stringhe Interpolate"
    PRINT "=========================================="
    PRINT

    ' --- 1. Interpolazione base con variabili ---
    PRINT "--- 1. Interpolazione base ---"
    DIM name AS STRING = "Alice"
    DIM age AS INTEGER = 30
    PRINT $"Hello {name}"
    PRINT $"Age: {age}"
    PRINT

    ' --- 2. Espressioni aritmetiche ---
    PRINT "--- 2. Espressioni aritmetiche ---"
    DIM a = 10, b = 20
    PRINT $"{a} + {b} = {a + b}"
    PRINT $"Product: {a} * {b} = {a * b}"
    PRINT

    ' --- 3. Chiamate di funzione ---
    PRINT "--- 3. Chiamate di funzione ---"
    DIM msg = "Hello World"
    PRINT $"Message: '{msg}' has {LEN(msg)} chars"
    PRINT $"Upper: {UPPER$(msg)}"
    PRINT

    ' --- 4. Format strings ---
    PRINT "--- 4. Format strings ---"
    DIM value = 1234.5678
    PRINT $"Currency: {value:##,##0.00}"
    PRINT $"Percent: {0.856:0.0%}"
    PRINT $"Hex: {255:X}"
    PRINT

    ' --- 5. Ternary e logiche ---
    PRINT "--- 5. Espressioni condizionali ---"
    DIM active = TRUE
    PRINT $"Status: {IIF(active, "ACTIVE", "INACTIVE")}"
    PRINT $"Check: {IIF(a > b, "a > b", "a <= b")}"
    PRINT

    ' --- 6. Parentesi graffe letterali ---
    PRINT "--- 6. Escape parentesi ---"
    PRINT $"Use {{braces}} for placeholders"
    PRINT $"JSON-like: {{key: value}}"
    PRINT

    ' --- 7. Concatenazione ---
    PRINT "--- 7. Concatenazione ---"
    DIM first = "John", last = "Doe"
    DIM full = $"First: {first}" + $" | Last: {last}"
    PRINT full
    PRINT

    ' --- 8. Nested interpolation ---
    PRINT "--- 8. Nested expressions ---"
    DIM items = 5
    DIM total = 150.50
    PRINT $"Order: {items} items, total ${total:##0.00}, avg ${total / items:##0.00}"
    PRINT

    PRINT "=========================================="
    PRINT "  TEST COMPLETATO"
    PRINT "=========================================="

END SUB
