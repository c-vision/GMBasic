' CLMLocal — AI DECIDE con il provider `clm` in locale: encoder Qwen3-8B (MLX, pooling sull'ULTIMO
' token) + head CLM in C. Niente server, niente CUDA.
'
' Prerequisiti (scaricati una volta, vedi Guide/manuale V.32):
'   ~/ai/Models/Qwen3-8B                       encoder (HF, bf16)
'   ~/ai/Models/CLM-v0.1-8B/clm_head.safetensors   head, prodotta dal checkpoint .pt con
'                                              tools/c/clm_head_convert (Python-free)
'
' Cosa mostra: CLM sceglie fra candidati LIBERI (qui le opzioni di CHOICE sono il testo dei
' candidati). La PROBABILITY e' una distribuzione sulle opzioni date, non una correttezza
' calibrata; non dipende dall'ordine dei candidati.
IMPORT AI

SUB Main()
    DIM r AS STRING
    AI REGISTER "clm", %DECIDE, "clm", "{""head"":""~/ai/Models/CLM-v0.1-8B/clm_head.safetensors"",""encoder"":""~/ai/Models/Qwen3-8B""}"

    ' 1) Ranking di candidati (esempio del README di CLM: prob 0.993 per la Luna).
    AI DECIDE "What causes tides on Earth?", "CHOICE:The Moon's gravitational pull.|Photosynthesis in plants.|Because the Earth is round.", "clm" INTO r
    PRINT "rank:  "; r
    AI DECIDE "What causes tides on Earth?", "CHOICE:Because the Earth is round.|Photosynthesis in plants.|The Moon's gravitational pull.", "clm" INTO r
    PRINT "rank2: "; r

    ' 2) Smistamento di un ticket: la domanda va nel config come `instructions`.
    AI REGISTER "team", %DECIDE, "clm", "{""head"":""~/ai/Models/CLM-v0.1-8B/clm_head.safetensors"",""encoder"":""~/ai/Models/Qwen3-8B"",""instructions"":""Which team should handle this?""}"
    AI DECIDE "Customer: my invoice was charged twice and nobody answers the phone!", "CHOICE:Charges, invoices, refunds|Bugs and outages", "team" INTO r
    PRINT "team:  "; r

    ' 3) RANK: best-of-N su candidati liberi, ordinati per probabilita' (senza limite di 16 opzioni).
    '    Nessuna sintassi nuova: e' un "kind" di AI DECIDE, valido solo col provider clm.
    AI DECIDE "What causes tides on Earth?", "RANK:Because the Earth is round.|Photosynthesis in plants.|The Moon's gravitational pull.", "clm" INTO r
    PRINT "rank3: "; r

    ' 4) VERIFY con le misure del provider: modo, tempi, numero di forward dell'encoder, margine.
    '    I millisecondi dipendono dalla macchina: qui si stampa solo la presenza delle misure.
    AI DECIDE VERIFY "What causes tides on Earth?", "CHOICE:The Moon's gravitational pull.|Photosynthesis in plants.", "clm|rules" INTO r
    ' (-1 = vero in BASIC)
    PRINT "verify-clm-local:  "; INSTR(r, "mode"":""local") > 0
    PRINT "verify-embeddings: "; INSTR(r, "embeddings"":3") > 0
    PRINT "verify-agreement:  "; INSTR(r, "agreement"":true") > 0
END SUB
