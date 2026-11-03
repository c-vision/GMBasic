' MiniTest NAMESPACE IO — tutti i 32 simboli della superficie IO.
'
' Superficie (shared/spec/registry/surface.csv): APPPATH, BINPATH, COUNTLINES, CWD,
' DIR, DIREXISTS, EOF, FCLOSE, FCREATE, FERROR, FINPUT, FOPEN, FREAD, FSEEK,
' FWRITE, FILEEXISTS, FOLDERSIZE, FREEFILE, GETTEMPDIR, GETTEMPFILE, GETFOLDERPATH,
' LOF, NORMALIZEPATH, PARENTDIR, PROJECTFOLDER, READBYTES, READFILE, READLINES,
' RESOLVEFILEPATH, SEEK, TEMPDIR, TEMPFILE.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Path ASSOLUTI in /tmp e fixture create/rimosse dal test:
' l'output non dipende dalla CWD. I path d'ambiente sono verificati come "presenti",
' mai stampati (cambiano fra le gambe).
' EXPECT: lof= 8|seek0= 0|read1=[uno]|read3=[due]|ferror0=-1|finput3=[uno]|exists=-1|count= 2|rflen= 8|linesN= 2 |line0=[uno]|bytesN= 8|eofBefore= 0|readOpen=[uno]|freefile=-1|norm=/a/c|parent=/a/b|resolve=-1|cwd=-1|app=-1 |bin=-1 |proj=-1|folder=-1|tempdir=-1 |tempfile=-1|tempdir2=-1 |tempfile2=-1|direxists=si|foldersize=-1|dirN= 1|seekIni= 0|seek3= 2|cleaned=-1
IMPORT IO

SUB Main()
  DIM p AS STRING
  p = "/tmp/gmb_mt_io_all.txt"
  IF IO.FILEEXISTS(p) THEN KILL p

  ' ── handle: create/write/lof/seek/read/finput/ferror/close ───────────────
  DIM h AS LONG
  h = IO.FCREATE(p)
  IO.FWRITE(h, "uno" + CHR(10) + "due" + CHR(10))
  PRINT "lof="; IO.LOF(h)
  PRINT "seek0="; IO.FSEEK(h, 0)
  PRINT "read1=["; IO.FREAD(h); "]"
  PRINT "read3=["; IO.FREAD(h, 3); "]"
  PRINT "ferror0="; (IO.FERROR() = 0)
  IO.FSEEK(h, 0)
  PRINT "finput3=["; IO.FINPUT(3, h); "]"
  IO.FCLOSE(h)

  ' ── file interi e righe ──────────────────────────────────────────────────
  PRINT "exists="; IO.FILEEXISTS(p)
  PRINT "count="; IO.COUNTLINES(p)
  PRINT "rflen="; LEN(IO.READFILE(p))
  DIM lines = IO.READLINES(p)
  PRINT "linesN="; UBOUND(lines) + 1; "|line0=["; lines(0); "]"
  DIM bytes = IO.READBYTES(p)
  PRINT "bytesN="; UBOUND(bytes) + 1

  ' ── riapertura in lettura + EOF/FREEFILE ─────────────────────────────────
  DIM r AS LONG
  r = IO.FOPEN(p)
  PRINT "eofBefore="; IO.EOF(r)
  PRINT "readOpen=["; IO.FREAD(r); "]"
  IO.FCLOSE(r)
  PRINT "freefile="; (IO.FREEFILE() > 0)

  ' ── path ─────────────────────────────────────────────────────────────────
  PRINT "norm="; IO.NORMALIZEPATH("/a/b/../c")
  PRINT "parent="; IO.PARENTDIR("/a/b/c")
  PRINT "resolve="; (LEN(IO.RESOLVEFILEPATH(p)) > 0)
  PRINT "cwd="; (LEN(IO.CWD()) > 0)
  PRINT "app="; (LEN(IO.APPPATH()) > 0); "|bin="; (LEN(IO.BINPATH()) > 0); "|proj="; (LEN(IO.PROJECTFOLDER()) > 0)
  PRINT "folder="; (LEN(IO.GETFOLDERPATH(".")) > 0)
  PRINT "tempdir="; (LEN(IO.GETTEMPDIR()) > 0); "|tempfile="; (LEN(IO.GETTEMPFILE()) > 0)
  PRINT "tempdir2="; (IO.TEMPDIR() = IO.GETTEMPDIR()); "|tempfile2="; (LEN(IO.TEMPFILE()) > 0)

  ' ── cartelle: direxists/foldersize/dir ──────────────────────────────────
  IF IO.DIREXISTS("/tmp/gmb_mt_io_all_dir") THEN RMDIR "/tmp/gmb_mt_io_all_dir", TRUE
  MKDIR "/tmp/gmb_mt_io_all_dir"
  IF IO.DIREXISTS("/tmp/gmb_mt_io_all_dir") THEN PRINT "direxists=si" ELSE PRINT "direxists=no"
  DIM fh AS LONG
  fh = IO.FCREATE("/tmp/gmb_mt_io_all_dir/f.txt")
  IO.FWRITE(fh, "abc")
  IO.FCLOSE(fh)
  PRINT "foldersize="; (IO.FOLDERSIZE("/tmp/gmb_mt_io_all_dir") >= 3)
  DIM fl = IO.DIR("/tmp/gmb_mt_io_all_dir")
  PRINT "dirN="; UBOUND(fl) + 1
  RMDIR "/tmp/gmb_mt_io_all_dir", TRUE

  ' ── SEEK legacy sul numero di file (#1) ──────────────────────────────────
  OPEN p FOR INPUT AS #1
  PRINT "seekIni="; IO.SEEK(1)
  SEEK 1, 3
  PRINT "seek3="; IO.SEEK(1)
  CLOSE #1

  KILL p
  PRINT "cleaned="; (NOT IO.FILEEXISTS(p))
END SUB
