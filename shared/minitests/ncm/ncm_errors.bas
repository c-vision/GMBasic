' NCM.LOADMODEL con percorsi o file sbagliati: ritorna 0 e NCM.LASTERROR() spiega perche' (mai un crash, mai un handle valido).
' Quattro casi: generatore inesistente, tokenizer inesistente, vocabolario inesistente, file che non e' un checkpoint
' (vocab.txt passato come generatore). La generazione vera e' nei test ncm_generate e ncm_lifecycle (modello minuscolo)
' e nel sample AiLLM/NcmGenerate.bas (1 GB di pesi).
' EXPECT: gen= 0|tok= 0|vocab= 0|badfile= 0|err= 1
IMPORT NCM

SUB Main()
    DIM FX AS STRING
    FX = "../../samples/Fixtures/ncm_tiny/"
    DIM h AS LONG
    h = NCM.LOADMODEL("/nonexistent/generator.pt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    PRINT "gen="; h
    h = NCM.LOADMODEL(FX + "generator.pt", "/nonexistent/tokenizer.pt", FX + "vocab.txt", 0)
    PRINT "tok="; h
    h = NCM.LOADMODEL(FX + "generator.pt", FX + "tokenizer.pt", "/nonexistent/vocab.txt", 0)
    PRINT "vocab="; h
    h = NCM.LOADMODEL(FX + "vocab.txt", FX + "tokenizer.pt", FX + "vocab.txt", 0)
    PRINT "badfile="; h
    DIM e AS STRING
    e = NCM.LASTERROR()
    PRINT "err="; IIF(LEN(e) > 0, 1, 0)
END SUB
