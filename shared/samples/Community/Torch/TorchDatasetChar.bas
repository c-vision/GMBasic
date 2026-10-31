' ============================================================================
' TorchDatasetChar.bas — pipeline dati testuale toy (L6, char-level).
' Crea un corpus, lo carica in un tensore [n_sample, cutoff_len], codifica/decodifica
' una stringa e mostra le forme. Char-level: ogni byte e' un id 0..255 (toy, non BPE).
' ============================================================================
IMPORT TORCH
IMPORT IO

SUB Main()
  DIM dir AS STRING = PROJECTFOLDER() + "/work"
  IF NOT IO.DIREXISTS(dir) THEN MKDIR dir
  DIM p AS STRING = dir + "/gmb_dataset_txt.txt"
  DIM h AS LONG = IO.FCREATE(p)
  IO.FWRITE(h, "il gatto dorme" + CHR(10) + "il cane corre" + CHR(10) + "la volpe salta" + CHR(10))
  IO.FCLOSE(h)

  ' dataset: una riga per sample, pad a cutoff_len
  DIM ds AS LONG = TORCH.DATASETLOAD(p, "{""cutoff_len"":16}")
  DIM sh() = TORCH.SHAPE(ds)
  PRINT "dataset: "; sh(0); " x "; sh(1)

  ' cutoff_len diverso
  DIM ds2 AS LONG = TORCH.DATASETLOAD(p, "{""cutoff_len"":8,""max_samples"":2}")
  DIM sh2() = TORCH.SHAPE(ds2)
  PRINT "ridotto: "; sh2(0); " x "; sh2(1)

  ' round-trip testo
  DIM e AS LONG = TORCH.TEXTENCODE("il gatto")
  PRINT "decodifica: [" + TORCH.TEXTDECODE(e) + "]"

  ' il tensore codificato e' usabile come input di un modello
  DIM cfg AS STRING = "{""vocab"":256,""d_model"":16,""n_layers"":1,""n_heads"":1,""block_size"":16}"
  DIM m AS LONG = TORCH.MODELCREATE("transformer", cfg)
  DIM logits AS LONG = TORCH.MODELFORWARD(m, e)
  DIM sl() = TORCH.SHAPE(logits)
  PRINT "logits: "; sl(0); " x "; sl(1); " x vocab(256)"

  TORCH.MODELFREE(ds) : TORCH.MODELFREE(ds2) : TORCH.MODELFREE(e)
  TORCH.MODELFREE(m) : TORCH.MODELFREE(logits)
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
  PRINT "=== TorchDatasetChar: done ==="
END SUB
