' FILE: AiLLM/NcmSummarize.bas
' ==============================================================================
'   NCM condizionato: riassunto di un documento + valutazione ROUGE, tutto in GMBasic
' ==============================================================================
' Un modello NCM condizionato (stessa architettura del checkpoint XSum del paper arXiv 2610.14162) legge il documento come
' PREFISSO (i primi P token del BPE di GPT-2) e genera il riassunto dal grossolano al fine: 9 livelli di codici discreti,
' ciascuno in UN solo forward, poi un decoder VQ-VAE riporta i codici ai token. Il sample:
'   1. carica il modello condizionato (NCM.LOADCONDITIONAL) e il BPE;
'   2. riassume tre documenti, senza e con guidance (cfgScale 1.0: costo doppio);
'   3. misura ROUGE-1/2/L contro un riassunto di riferimento (NCM.ROUGE1/2/L, stessa tokenizzazione di rouge_score);
'   4. mostra che i token oltre il prefisso non contano (un documento lungo = il suo troncamento).
'
' MODELLO: per default usa le fixture MINUSCOLE di samples/Fixtures/ncm_tiny_cond (pesi casuali: il testo non ha senso e i ROUGE
' sono ~0, ma tutto il percorso e' lo stesso e il sample e' riproducibile sulle 4 gambe). Per il checkpoint XSum vero (R-1/2/L = 34,6/12,8/27,8
' sui primi 1000 documenti del test set, 0,12 s per riassunto su Apple M5 Max) scarica da https://huggingface.co/hazyresearch/ncm-checkpoints
' la cartella xsum (generator.pt 354 MB, tokenizer.pt 300 MB, config.yaml) in samples/Fixtures/ncm/xsum/, aggiungi vocab.json e merges.txt di GPT-2
' (https://huggingface.co/openai-community/gpt2) e imposta REAL = 1 (device 3 = GPU Apple).
IMPORT NCM

CONST REAL = 0

' Stampa una riga "etichetta: valore" con tre decimali.
SUB Report(label AS STRING, ref AS STRING, hyp AS STRING)
    PRINT label; ": R1="; FORMAT(NCM.ROUGE1(ref, hyp), "0.000"); " R2="; FORMAT(NCM.ROUGE2(ref, hyp), "0.000"); " RL="; FORMAT(NCM.ROUGEL(ref, hyp), "0.000")
END SUB

SUB Main()
    DIM dir AS STRING, gen AS STRING, tok AS STRING, cfg AS STRING, voc AS STRING, mrg AS STRING, dev AS LONG
    IF REAL = 1 THEN
        dir = "../Fixtures/ncm/xsum/"
        voc = dir + "vocab.json"
        mrg = dir + "merges.txt"
        dev = 3
    ELSE
        dir = "../Fixtures/ncm_tiny_cond/"
        voc = dir + "vocab.json"
        mrg = dir + "merges.txt"
        dev = 0
    END IF
    gen = dir + "generator.pt"
    tok = dir + "tokenizer.pt"
    cfg = dir + "config.yaml"

    DIM h AS LONG
    h = NCM.LOADCONDITIONAL(gen, tok, cfg, voc, mrg, dev)
    IF h = 0 THEN
        PRINT "NCM.LOADCONDITIONAL fallita: "; NCM.LASTERROR()
        RETURN
    END IF

    DIM docs(2) AS STRING, refs(2) AS STRING
    docs(0) = "The council has approved plans for a new library in the town centre. Work will start next spring and should take two years."
    refs(0) = "A new library will be built in the town centre."
    docs(1) = "Heavy rain caused flooding on several roads overnight. Police urged drivers to avoid the area and take care."
    refs(1) = "Flooding closes roads after heavy rain."
    docs(2) = "The club has signed a new striker on a three year deal after a long transfer saga that lasted all summer."
    refs(2) = "Club signs striker on three-year deal."

    DIM i AS LONG, plain AS STRING, guided AS STRING
    DIM s1 AS DOUBLE, s2 AS DOUBLE, sl AS DOUBLE
    FOR i = 0 TO 2
        plain = NCM.GENERATEFROM(h, docs(i), 0, -1)        ' -1 = valore del config (greedy, nessuna guidance)
        guided = NCM.GENERATEFROM(h, docs(i), 0, 1.0)      ' con guidance senza classificatore
        PRINT "documento "; i + 1; ": "; LEFT$(docs(i), 48); "..."
        PRINT "  rif   : "; refs(i)
        PRINT "  plain : "; plain
        PRINT "  guided: "; guided
        Report("  plain ", refs(i), plain)
        Report("  guided", refs(i), guided)
        s1 = s1 + NCM.ROUGE1(refs(i), plain)
        s2 = s2 + NCM.ROUGE2(refs(i), plain)
        sl = sl + NCM.ROUGEL(refs(i), plain)
    NEXT i
    PRINT "media plain: R1="; FORMAT(s1 / 3, "0.000"); " R2="; FORMAT(s2 / 3, "0.000"); " RL="; FORMAT(sl / 3, "0.000")

    ' Solo i primi P token del documento contano: allungarlo non cambia il riassunto.
    DIM longdoc AS STRING
    longdoc = docs(0) + " " + docs(1) + " " + docs(2)
    PRINT "troncamento: "; IIF(NCM.GENERATEFROM(h, longdoc, 0, -1) = NCM.GENERATEFROM(h, docs(0) + " zzz", 0, -1), "documenti con lo stesso prefisso = stesso riassunto", "diverso")

    NCM.FREEMODEL(h)
END SUB
