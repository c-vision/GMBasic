' ==============================================================================
' skills_attribution.bas — build shared/skills/ATTRIBUTION.md + attribution.json.
'
' Le skill in `shared/skills/` sono materiale TERZO raccolto da installazioni locali di
' altri strumenti AI. Prima della pubblicazione ognuna deve essere tracciabile a un autore
' e a una licenza che ne consenta la ridistribuzione. Questo strumento NON inventa nulla:
' riporta SOLO cio' che i file dichiarano (frontmatter license/author/metadata.author, URL)
' e classifica ogni skill come
'
'   DECLARED-PERMISSIVE   licenza che consente la ridistribuzione (MIT/Apache/BSD/...)
'   DECLARED-PROPRIETARY  licenza che la vieta -> deve uscire dall'albero pubblico
'   UNDECLARED            nessuna licenza nel file -> "tutti i diritti riservati"
'
' Uso (dalla root del repo, gamba nativa):
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/skills_attribution.bas ska
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/skills_attribution.bas ska -- --check
'
' I due file prodotti sono GENERATI: si modifica questo strumento, non loro.
' ==============================================================================
IMPORT IO
IMPORT SYSTEM

DIM g_names(512) AS STRING
DIM g_n AS LONG

DIM g_skill(512) AS STRING
DIM g_status(512) AS STRING
DIM g_lic(512) AS STRING
DIM g_author(512) AS STRING
DIM g_version(512) AS STRING
DIM g_vendor(512) AS STRING
DIM g_home(512) AS STRING
DIM g_url(512, 3) AS STRING
DIM g_nurl(512) AS LONG
DIM g_rows AS LONG

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

FUNCTION Lower(s AS STRING) AS STRING
  RETURN LCASE$(s)
END FUNCTION

FUNCTION Contains(s AS STRING, sub AS STRING) AS LONG
  IF INSTR(LCASE$(s), LCASE$(sub)) > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION StartsWith(s AS STRING, pre AS STRING) AS LONG
  IF LEN(s) >= LEN(pre) THEN
    IF LEFT$(s, LEN(pre)) = pre THEN RETURN -1
  END IF
  RETURN 0
END FUNCTION

FUNCTION StripQuotes(s AS STRING) AS STRING
  DIM v AS STRING
  v = TrimS(s)
  IF LEN(v) >= 2 THEN
    IF (LEFT$(v, 1) = CHR$(34) AND RIGHT$(v, 1) = CHR$(34)) OR (LEFT$(v, 1) = CHR$(39) AND RIGHT$(v, 1) = CHR$(39)) THEN
      v = MID$(v, 2, LEN(v) - 2)
    END IF
  END IF
  RETURN v
END FUNCTION

' ---- frontmatter ------------------------------------------------------------
DIM g_fmKey(128) AS STRING
DIM g_fmVal(128) AS STRING
DIM g_fmN AS LONG

SUB FmAdd(k AS STRING, v AS STRING)
  IF g_fmN < 128 THEN
    g_fmKey(g_fmN) = k
    g_fmVal(g_fmN) = v
    g_fmN = g_fmN + 1
  END IF
END SUB

SUB ParseFrontmatter(t AS STRING)
  DIM ln$() AS STRING, i AS LONG, s AS STRING, inFm AS LONG, parent AS STRING
  DIM k AS LONG, key AS STRING, val AS STRING, indented AS LONG
  g_fmN = 0
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN EXIT SUB
  IF TrimS(ln$(0)) <> "---" THEN EXIT SUB
  inFm = 0
  parent = ""
  FOR i = 1 TO UBOUND(ln$)
    s = ln$(i)
    IF TrimS(s) = "---" THEN EXIT DO
    IF LEN(TrimS(s)) > 0 AND LEFT$(LTRIM$(s), 1) <> "#" THEN
      indented = 0
      IF LEFT$(s, 1) = " " OR LEFT$(s, 1) = CHR$(9) THEN indented = -1
      k = INSTR(s, ":")
      IF k > 0 THEN
        key = LCASE$(TrimS(LEFT$(s, k - 1)))
        val = StripQuotes(MID$(s, k + 1))
        IF indented = 0 THEN
          parent = key
          IF LEN(val) > 0 THEN
            FmAdd(key, val)
          ELSE
            FmAdd(key, "")
          END IF
        ELSEIF LEN(parent) > 0 THEN
          FmAdd(parent + "." + key, val)
        END IF
      END IF
    END IF
  NEXT i
END SUB

FUNCTION FmGet(k AS STRING) AS STRING
  DIM i AS LONG
  FOR i = 0 TO g_fmN - 1
    IF g_fmKey(i) = k THEN RETURN g_fmVal(i)
  NEXT i
  RETURN ""
END FUNCTION

' ---- classificazione --------------------------------------------------------
FUNCTION PermissiveName(raw AS STRING) AS STRING
  DIM l AS STRING
  l = LCASE$(raw)
  IF INSTR(l, "mit") > 0 THEN RETURN "MIT"
  IF INSTR(l, "apache") > 0 THEN RETURN "Apache-2.0"
  IF INSTR(l, "bsd-3") > 0 THEN RETURN "BSD-3-Clause"
  IF INSTR(l, "bsd") > 0 THEN RETURN "BSD"
  IF INSTR(l, "isc") > 0 THEN RETURN "ISC"
  IF INSTR(l, "cc0") > 0 THEN RETURN "CC0-1.0"
  IF INSTR(l, "cc-by-4") > 0 THEN RETURN "CC-BY-4.0"
  IF INSTR(l, "cc-by") > 0 THEN RETURN "CC-BY"
  IF INSTR(l, "unlicense") > 0 THEN RETURN "Unlicense"
  RETURN ""
END FUNCTION

FUNCTION VendorOf(name AS STRING) AS STRING
  IF StartsWith(name, "goose") <> 0 THEN RETURN "GooseWorks"
  IF StartsWith(name, "gsd") <> 0 THEN RETURN "GSD (~/.agents/skills)"
  RETURN ""
END FUNCTION

' URL http(s) nei primi 4000 caratteri, unici, ordinati, max 3.
DIM g_tmpUrl(256) AS STRING
DIM g_ntmp AS LONG

SUB CollectUrls(t AS STRING, idx AS LONG)
  DIM head AS STRING, i AS LONG, p AS LONG, u AS STRING, c AS STRING
  DIM k AS LONG, j AS LONG, tmp AS STRING, dup AS LONG
  g_nurl(idx) = 0
  g_ntmp = 0
  IF LEN(t) > 4000 THEN head = LEFT$(t, 4000) ELSE head = t
  i = 1
  DO
    p = INSTR(i, head, "http")
    IF p = 0 THEN EXIT DO
    IF MID$(head, p, 7) = "http://" OR MID$(head, p, 8) = "https://" THEN
      u = ""
      k = p
      DO WHILE k <= LEN(head)
        c = MID$(head, k, 1)
        IF c = " " OR c = ")" OR c = "]" OR c = CHR$(34) OR c = "'" OR c = "<" OR c = ">" OR c = "`" OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) THEN EXIT DO
        u = u + c
        k = k + 1
      LOOP
      dup = 0
      FOR j = 0 TO g_ntmp - 1
        IF g_tmpUrl(j) = u THEN dup = -1
      NEXT j
      IF LEN(u) > 8 AND dup = 0 AND g_ntmp < 256 THEN
        g_tmpUrl(g_ntmp) = u
        g_ntmp = g_ntmp + 1
      END IF
      i = k
    ELSE
      i = p + 4
    END IF
  LOOP
  ' ordinamento crescente (insertion sort) e prime 3
  FOR i = 0 TO g_ntmp - 2
    FOR k = 0 TO g_ntmp - 2 - i
      IF g_tmpUrl(k) > g_tmpUrl(k + 1) THEN
        tmp = g_tmpUrl(k)
        g_tmpUrl(k) = g_tmpUrl(k + 1)
        g_tmpUrl(k + 1) = tmp
      END IF
    NEXT k
  NEXT i
  FOR i = 0 TO g_ntmp - 1
    IF g_nurl(idx) < 3 THEN
      g_url(idx, g_nurl(idx)) = g_tmpUrl(i)
      g_nurl(idx) = g_nurl(idx) + 1
    END IF
  NEXT i
END SUB

SUB Classify(idx AS LONG, name AS STRING, t AS STRING)
  DIM rawLic AS STRING, stat AS STRING, licName AS STRING, a AS STRING
  DIM ver AS STRING, vd AS STRING, home AS STRING
  ParseFrontmatter(t)
  rawLic = ""
  IF LEN(FmGet("license")) > 0 THEN rawLic = FmGet("license")
  IF LEN(rawLic) = 0 AND LEN(FmGet("metadata.license")) > 0 THEN rawLic = FmGet("metadata.license")
  rawLic = TrimS(rawLic)

  stat = "UNDECLARED"
  licName = ""
  IF LEN(rawLic) > 0 THEN
    licName = PermissiveName(rawLic)
    IF LEN(licName) > 0 THEN
      stat = "DECLARED-PERMISSIVE"
    ELSEIF Contains(rawLic, "proprietary") <> 0 OR Contains(rawLic, "all rights reserved") <> 0 THEN
      stat = "DECLARED-PROPRIETARY"
      IF LEN(rawLic) > 60 THEN licName = LEFT$(rawLic, 60) ELSE licName = rawLic
    END IF
  END IF

  a = FmGet("author")
  IF LEN(a) = 0 THEN a = FmGet("metadata.author")
  ver = FmGet("version")
  IF LEN(ver) = 0 THEN ver = FmGet("metadata.version")
  vd = VendorOf(name)
  home = FmGet("homepage")
  IF LEN(home) = 0 THEN home = FmGet("metadata.homepage")
  IF LEN(home) = 0 THEN home = FmGet("repository")
  IF LEN(home) = 0 THEN home = FmGet("metadata.repository")

  g_skill(idx) = name
  g_status(idx) = stat
  g_lic(idx) = licName
  g_author(idx) = TrimS(a)
  g_version(idx) = TrimS(ver)
  g_vendor(idx) = vd
  g_home(idx) = TrimS(home)
  CollectUrls(t, idx)
END SUB

' ---- emissione --------------------------------------------------------------
DIM g_md AS STRING

SUB Md(s AS STRING)
  g_md = g_md + s + CHR$(10)
END SUB

FUNCTION CountStatus(st AS STRING) AS LONG
  DIM i AS LONG, n AS LONG
  FOR i = 0 TO g_rows - 1
    IF g_status(i) = st THEN n = n + 1
  NEXT i
  RETURN n
END FUNCTION

SUB TablePermissive()
  DIM i AS LONG, a AS STRING, v AS STRING
  Md("| skill | author | license | version |")
  Md("|---|---|---|---|")
  FOR i = 0 TO g_rows - 1
    IF g_status(i) = "DECLARED-PERMISSIVE" THEN
      a = g_author(i)
      IF LEN(a) = 0 THEN a = "—"
      v = g_version(i)
      IF LEN(v) = 0 THEN v = "—"
      Md("| `" + g_skill(i) + "` | " + a + " | " + g_lic(i) + " | " + v + " |")
    END IF
  NEXT i
END SUB

SUB TableOther(st AS STRING)
  DIM i AS LONG, l AS STRING, vd AS STRING
  Md("| skill | declared | family |")
  Md("|---|---|---|")
  FOR i = 0 TO g_rows - 1
    IF g_status(i) = st THEN
      l = g_lic(i)
      IF LEN(l) = 0 THEN l = "—"
      vd = g_vendor(i)
      IF LEN(vd) = 0 THEN vd = "—"
      Md("| `" + g_skill(i) + "` | " + l + " | " + vd + " |")
    END IF
  NEXT i
END SUB

FUNCTION JsonEsc(s AS STRING) AS STRING
  DIM out AS STRING, i AS LONG, c AS STRING, bs AS STRING
  bs = CHR$(92)
  out = ""
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c = CHR$(34) THEN
      out = out + bs + CHR$(34)
    ELSEIF c = bs THEN
      out = out + bs + bs
    ELSEIF c = CHR$(9) THEN
      out = out + bs + "t"
    ELSE
      out = out + c
    END IF
  NEXT i
  RETURN out
END FUNCTION

FUNCTION BuildJson() AS STRING
  DIM out AS STRING, i AS LONG, u AS STRING, sep AS STRING, k AS LONG
  out = "[" + CHR$(10)
  FOR i = 0 TO g_rows - 1
    out = out + "  {" + CHR$(10)
    out = out + "    " + Q("skill") + ": " + Q(JsonEsc(g_skill(i))) + "," + CHR$(10)
    out = out + "    " + Q("status") + ": " + Q(JsonEsc(g_status(i))) + "," + CHR$(10)
    out = out + "    " + Q("license") + ": " + Q(JsonEsc(g_lic(i))) + "," + CHR$(10)
    out = out + "    " + Q("author") + ": " + Q(JsonEsc(g_author(i))) + "," + CHR$(10)
    out = out + "    " + Q("version") + ": " + Q(JsonEsc(g_version(i))) + "," + CHR$(10)
    out = out + "    " + Q("vendor") + ": " + Q(JsonEsc(g_vendor(i))) + "," + CHR$(10)
    out = out + "    " + Q("homepage") + ": " + Q(JsonEsc(g_home(i))) + "," + CHR$(10)
    IF g_nurl(i) = 0 THEN
      u = "    " + Q("urls") + ": []"
    ELSE
      u = "    " + Q("urls") + ": [" + CHR$(10)
      FOR k = 0 TO g_nurl(i) - 1
        u = u + "      " + Q(JsonEsc(g_url(i, k)))
        IF k < g_nurl(i) - 1 THEN u = u + ","
        u = u + CHR$(10)
      NEXT k
      u = u + "    ]"
    END IF
    out = out + u + CHR$(10)
    IF i < g_rows - 1 THEN
      out = out + "  }," + CHR$(10)
    ELSE
      out = out + "  }" + CHR$(10)
    END IF
  NEXT i
  out = out + "]" + CHR$(10)
  RETURN out
END FUNCTION

SUB WriteIf(dst AS STRING, body AS STRING)
  DIM h AS LONG, wr AS LONG
  wr = SYSTEM.SHELLOUT("rm -f " + CHR$(34) + dst + CHR$(34))
  h = IO.FCREATE(dst)
  wr = IO.FWRITE(h, body)
  IO.FCLOSE(h)
END SUB

FUNCTION Main() AS INTEGER
  DIM a AS STRING, i AS LONG, root AS STRING, sk AS STRING, base AS STRING, lastSlash AS LONG, kk AS LONG
  DIM t AS STRING, ln$() AS STRING, f AS STRING, checkOnly AS LONG
  DIM nPerm AS LONG, nProp AS LONG, nUndecl AS LONG, nBroken AS LONG
  DIM nWithAuthor AS LONG, rc AS LONG, blocking AS LONG

  SET SHELL ON
  IF DIREXISTS(IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/skills") THEN
    root = IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/skills"
  ELSE
    root = "shared/skills"
  END IF
  checkOnly = 0
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--check" THEN checkOnly = -1
    IF a = "--skills" AND i < COMMANDCOUNT() THEN
      i = i + 1
      root = COMMAND$(i)
    END IF
    i = i + 1
  LOOP

  t = SYSTEM.SHELLOUT("find " + Q(root) + " -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sort")
  ln$ = SPLIT(t, CHR$(10))
  g_n = 0
  IF UBOUND(ln$) >= 0 THEN
    FOR i = 0 TO UBOUND(ln$)
      IF LEN(TrimS(ln$(i))) > 0 AND g_n < 512 THEN
        g_names(g_n) = TrimS(ln$(i))
        g_n = g_n + 1
      END IF
    NEXT i
  END IF

  g_rows = 0
  FOR i = 0 TO g_n - 1

    base = g_names(i)
    sk = base

    lastSlash = 0

    FOR kk = 1 TO LEN(sk)
      IF MID$(sk, kk, 1) = "/" THEN lastSlash = kk
    NEXT kk
    IF lastSlash > 0 THEN sk = MID$(sk, lastSlash + 1)
    IF sk = "licenses" THEN
      ' cartella di servizio, non skill
    ELSE
      f = base + "/SKILL.md"
      IF NOT FILEEXISTS(f) THEN
        g_skill(g_rows) = sk
        g_status(g_rows) = "NO-SKILL.md"
        g_lic(g_rows) = ""
        g_author(g_rows) = ""
        g_version(g_rows) = ""
        g_vendor(g_rows) = ""
        g_home(g_rows) = ""
        g_nurl(g_rows) = 0
        g_rows = g_rows + 1
      ELSE
        t = IO.READFILE(f)
        Classify(g_rows, sk, t)
        g_rows = g_rows + 1
      END IF
    END IF
  NEXT i

  nPerm = CountStatus("DECLARED-PERMISSIVE")
  nProp = CountStatus("DECLARED-PROPRIETARY")
  nUndecl = CountStatus("UNDECLARED")
  nBroken = CountStatus("NO-SKILL.md")
  nWithAuthor = 0
  FOR i = 0 TO g_rows - 1
    IF g_status(i) = "DECLARED-PERMISSIVE" AND LEN(g_author(i)) > 0 THEN nWithAuthor = nWithAuthor + 1
  NEXT i

  g_md = ""
  Md("# Skills — attribution and license status")
  Md("")
  Md("> **GENERATED** by [`tools/basic/skills_attribution.bas`](../../tools/basic/skills_attribution.bas) —")
  Md("> do not edit by hand. Re-run it after adding or removing a skill.")
  Md("")
  Md("The skills in this folder are **third-party material**, collected from local installs of other")
  Md("AI tools (Claude Code, Codex, opencode, Gemini, `~/.agents/skills`). This file records **only")
  Md("what each skill's own file declares** (`author`, `license`, `version`); nothing is inferred.")
  Md("")
  Md("## Status summary")
  Md("")
  Md("| status | meaning | n. |")
  Md("|---|---|---|")
  Md("| `DECLARED-PERMISSIVE` | declares a license that allows redistribution | " + LTRIM$(STR$(nPerm)) + " |")
  Md("| `DECLARED-PROPRIETARY` | declares a license that **forbids** redistribution | " + LTRIM$(STR$(nProp)) + " |")
  Md("| `UNDECLARED` | no license in the file — legally **all rights reserved** | " + LTRIM$(STR$(nUndecl)) + " |")
  Md("")
  Md("Of the " + LTRIM$(STR$(nPerm)) + " redistributable skills, **" + LTRIM$(STR$(nWithAuthor)) + "** also declare the")
  Md("**author** (`author:` / `metadata.author:`); for the remaining " + LTRIM$(STR$(nPerm - nWithAuthor)))
  Md("the author is not stated in the file.")
  Md("")
  Md("## Redistributable skills (declared permissive license)")
  Md("")
  Md("Each row keeps the license its own file declares. The **MIT** ones need the MIT text and")
  Md("the copyright line of the author named in the row: see")
  Md("[`licenses/MIT.txt`](licenses/MIT.txt).")
  Md("")
  TablePermissive
  Md("")
  Md("## Skills that must NOT be published as they are")
  Md("")
  Md("These carry a **proprietary** license or **no license at all**. With no explicit grant,")
  Md("redistribution is not allowed: the entry is here so the decision is explicit (remove from")
  Md("the public tree, or obtain permission from the author) instead of accidental.")
  Md("")
  Md("### Proprietary (" + LTRIM$(STR$(nProp)) + ")")
  Md("")
  TableOther("DECLARED-PROPRIETARY")
  Md("")
  Md("### No license declared (" + LTRIM$(STR$(nUndecl)) + ")")
  Md("")
  TableOther("UNDECLARED")
  IF nBroken > 0 THEN
    Md("")
    Md("### Missing `SKILL.md` (" + LTRIM$(STR$(nBroken)) + ")")
    Md("")
    FOR i = 0 TO g_rows - 1
      IF g_status(i) = "NO-SKILL.md" THEN Md("- `" + g_skill(i) + "`")
    NEXT i
  END IF

  IF checkOnly = 0 THEN
    WriteIf(root + "/ATTRIBUTION.md", g_md)
    WriteIf(root + "/attribution.json", BuildJson())
  END IF

  PRINT "skills: "; g_rows
  PRINT "  DECLARED-PERMISSIVE  "; nPerm; "  (con autore dichiarato: "; nWithAuthor; ")"
  PRINT "  DECLARED-PROPRIETARY "; nProp
  PRINT "  UNDECLARED           "; nUndecl

  rc = 0
  IF checkOnly <> 0 THEN
    blocking = nProp + nUndecl + nBroken
    IF blocking > 0 THEN
      PRINT ""
      PRINT "ATTENZIONE: "; blocking; " skill non ridistribuibili nel tree:"
      FOR i = 0 TO g_rows - 1
        IF g_status(i) = "DECLARED-PROPRIETARY" OR g_status(i) = "UNDECLARED" OR g_status(i) = "NO-SKILL.md" THEN
          PRINT "  "; g_skill(i)
        END IF
      NEXT i
      rc = 1
    END IF
  END IF
  RETURN rc
END FUNCTION
