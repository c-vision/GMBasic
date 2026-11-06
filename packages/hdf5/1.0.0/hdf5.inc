' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' hdf5.inc — namespace HDF5 (pacchetto gmblib-hdf5): HDF5 come formato di scambio per
' dataset numerici multidimensionali. Wrapper su libhdf5 2.2.0: il pacchetto porta con sé la
' dylib (rilocabile, nessuna dipendenza dal percorso di brew a runtime).
'
' HANDLE: H5CREATE/H5OPEN ritornano un LONG (0 = errore); H5CLOSE lo libera.
' out() va dimensionato almeno a H5SIZE(h, nome) elementi.
' Ritorni: 0 = ok; -1 = errore (argomenti/handle, dataset assente o tipo inatteso —
' per qualunque tipo numerico usare H5READDBL).

NAMESPACE HDF5

' ── ciclo di vita ────────────────────────────────────────────────────────────
' Crea (o tronca) un file HDF5. Ritorna l'handle, 0 su errore.
DECLARE NATIVE FUNCTION H5CREATE LIB "gmbhdf5" ALIAS "ofx_h5_create" (path AS STRING) AS LONG
' Apre un file HDF5 esistente in lettura/scrittura. Ritorna l'handle, 0 su errore.
DECLARE NATIVE FUNCTION H5OPEN LIB "gmbhdf5" ALIAS "ofx_h5_open" (path AS STRING) AS LONG
' Chiude il file e libera l'handle. 0 = ok.
DECLARE NATIVE FUNCTION H5CLOSE LIB "gmbhdf5" ALIAS "ofx_h5_close" (h AS LONG) AS LONG

' ── scrittura ────────────────────────────────────────────────────────────────
' dataset F64: rank 1..8, dims() = rank valori, data() = prodotto(dims) valori.
DECLARE NATIVE FUNCTION H5WRITEF64 LIB "gmbhdf5" ALIAS "ofx_h5_write_f64" (h AS LONG, name AS STRING, rank AS LONG, dims() AS LONG, data() AS DOUBLE) AS LONG
' dataset I64 (interi a 64 bit).
DECLARE NATIVE FUNCTION H5WRITEI64 LIB "gmbhdf5" ALIAS "ofx_h5_write_i64" (h AS LONG, name AS STRING, rank AS LONG, dims() AS LONG, data() AS LONG) AS LONG
' dataset F64 con compressione deflate+shuffle (compress 0..9, 0 = nessuna).
DECLARE NATIVE FUNCTION H5WRITEF64C LIB "gmbhdf5" ALIAS "ofx_h5_write_f64c" (h AS LONG, name AS STRING, rank AS LONG, dims() AS LONG, data() AS DOUBLE, compress AS LONG) AS LONG
' crea un gruppo (anche annidato: "serie/2026").
DECLARE NATIVE FUNCTION H5MAKEGROUP LIB "gmbhdf5" ALIAS "ofx_h5_make_group" (h AS LONG, path AS STRING) AS LONG
' attributo stringa su un oggetto (dataset o gruppo).
DECLARE NATIVE FUNCTION H5WRITEATTRSTR LIB "gmbhdf5" ALIAS "ofx_h5_write_attr_str" (h AS LONG, obj AS STRING, attr AS STRING, val AS STRING) AS LONG
' attributo numerico (double) su un oggetto.
DECLARE NATIVE FUNCTION H5WRITEATTRF64 LIB "gmbhdf5" ALIAS "ofx_h5_write_attr_f64" (h AS LONG, obj AS STRING, attr AS STRING, val AS DOUBLE) AS LONG

' ── enumerazione ────────────────────────────────────────────────────────────
' numero di oggetti (dataset + gruppi) del file, in ordine di creazione.
DECLARE NATIVE FUNCTION H5COUNT LIB "gmbhdf5" ALIAS "ofx_h5_count" (h AS LONG) AS LONG
' nome completo dell'oggetto k-esimo.
DECLARE NATIVE FUNCTION H5NAME LIB "gmbhdf5" ALIAS "ofx_h5_name" (h AS LONG, k AS LONG) AS STRING
' tipo dell'oggetto k-esimo: 0 = dataset, 1 = gruppo, 2 = altro.
DECLARE NATIVE FUNCTION H5KIND LIB "gmbhdf5" ALIAS "ofx_h5_kind" (h AS LONG, k AS LONG) AS LONG
' dimensione (numero di elementi) del dataset: serve a dimensionare out().
DECLARE NATIVE FUNCTION H5SIZE LIB "gmbhdf5" ALIAS "ofx_h5_size" (h AS LONG, name AS STRING) AS LONG

' ── lettura ─────────────────────────────────────────────────────────────────
' legge un dataset F64 in out() (>= H5SIZE); dims() = rank dimensioni, nelem(0) = elementi.
DECLARE NATIVE FUNCTION H5READF64 LIB "gmbhdf5" ALIAS "ofx_h5_read_f64" (h AS LONG, name AS STRING, out() AS DOUBLE, dims() AS LONG, nelem() AS LONG) AS LONG
' legge un dataset I64 in out().
DECLARE NATIVE FUNCTION H5READI64 LIB "gmbhdf5" ALIAS "ofx_h5_read_i64" (h AS LONG, name AS STRING, out() AS LONG, dims() AS LONG, nelem() AS LONG) AS LONG
' legge QUALSIASI dataset numerico convertendolo in double.
DECLARE NATIVE FUNCTION H5READDBL LIB "gmbhdf5" ALIAS "ofx_h5_read_dbl" (h AS LONG, name AS STRING, out() AS DOUBLE, dims() AS LONG, nelem() AS LONG) AS LONG
' primo elemento di un dataset di stringhe.
DECLARE NATIVE FUNCTION H5READSTR LIB "gmbhdf5" ALIAS "ofx_h5_read_str" (h AS LONG, name AS STRING) AS STRING
' attributo stringa.
DECLARE NATIVE FUNCTION H5READATTRSTR LIB "gmbhdf5" ALIAS "ofx_h5_read_attr_str" (h AS LONG, obj AS STRING, attr AS STRING) AS STRING
' versione della libreria HDF5 sottostante.
DECLARE NATIVE FUNCTION H5VERSION LIB "gmbhdf5" ALIAS "ofx_h5_version" () AS STRING
