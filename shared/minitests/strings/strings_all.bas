' MiniTest NAMESPACE STRINGS — tutti i 48 simboli della superficie STRINGS.
'
' Superficie (shared/spec/registry/surface.csv): ASC, CAMEL, CHR, CONTAINS, DUP,
' ENDSWITH, FORMAT, HTMLENCODE, ISCREDITCARD, ISEMAIL, ISLENGTH, ISREQUIRED, ISURL,
' INSTR, JSENCODE, JOIN, KEBAB, LCASE, LTRIM, LEFT, LEN, LENB, LOWER, MID, PADC,
' PADL, PADR, PASCAL, PETSCII, RTRIM, REMOVE, REPEAT, REPLACE, REVERSE, RIGHT,
' SEARCH, SENTENCE, SNAKE, SPACE, SPLIT, STARTSWITH, STR, TITLE, TRIM, UCASE,
' URLENCODE, UPPER, WRAP.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Solo ASCII/UTF-8 deterministico (niente locale o orologio).
' EXPECT: asc= 65 |chr=[A]|len= 3 |lenb= 3|left=[ab]|right=[ef]|mid=[bcd]|instr= 7|upper=[ABC]|lower=[abc]|ucase=[ABC]|lcase=[abc]|trim=[ab]|ltrim=[ab]|rtrim=[ab]|padl=[...ab]|padr=[ab---]|padc=[**ab**]|repeat=[ababab]|dup=[aaa]|space=[   ]|replace=[cia0 m0nd0]|remove=[cia mnd]|reverse=[cba]|contains=-1 |startswith=-1 |endswith=-1|search=-1|camel=[ciaoMondo]|pascal=[CiaoMondo]|kebab=[ciao-mondo]|snake=[ciao_mondo]|sentence=[Ciao mondo]|title=[Ciao Mondo]|html=[&lt;a&amp;b&gt;]|js=[a<b&c\'x]|url=[a%20b%26c]|format=[1234.50]|str=[ 42]|valid=-1 |-1 |-1|validlen=-1 |cc=-1|splitN= 3 |join=[a-b-c]|petscii= 16|wrap=[ciao|mondo|lungo]
IMPORT STRINGS

SUB Main()
  ' ── caratteri e lunghezze ────────────────────────────────────────────────
  PRINT "asc="; ASC("A"); "|chr=["; CHR$(65); "]"
  PRINT "len="; LEN("abc"); "|lenb="; LENB("abc")

  ' ── sottostringhe ────────────────────────────────────────────────────────
  PRINT "left=["; LEFT("abcdef", 2); "]|right=["; RIGHT("abcdef", 2); "]|mid=["; MID("abcdef", 2, 3); "]"
  PRINT "instr="; INSTR("hello world", "world")

  ' ── maiuscole/minuscole e trim ───────────────────────────────────────────
  PRINT "upper=["; UPPER("aBc"); "]|lower=["; LOWER("aBc"); "]"
  PRINT "ucase=["; UCASE("aBc"); "]|lcase=["; LCASE("aBc"); "]"
  PRINT "trim=["; TRIM("  ab  "); "]|ltrim=["; LTRIM("  ab"); "]|rtrim=["; RTRIM("ab  "); "]"

  ' ── padding, ripetizione, sostituzione ───────────────────────────────────
  PRINT "padl=["; PADL("ab", 5, "."); "]|padr=["; PADR("ab", 5, "-"); "]|padc=["; PADC("ab", 6, "*"); "]"
  PRINT "repeat=["; REPEAT("ab", 3); "]|dup=["; DUP(3, "ab"); "]|space=["; SPACE(3); "]"
  PRINT "replace=["; REPLACE("ciao mondo", "o", "0"); "]"
  PRINT "remove=["; REMOVE("ciao mondo", "o"); "]"
  PRINT "reverse=["; REVERSE("abc"); "]"

  ' ── ricerca e predicati ──────────────────────────────────────────────────
  PRINT "contains="; CONTAINS("abc", "b"); "|startswith="; STARTSWITH("abc", "a"); "|endswith="; ENDSWITH("abc", "c")
  PRINT "search="; SEARCH("ciao mondo", "mondo")

  ' ── stili di casing ──────────────────────────────────────────────────────
  PRINT "camel=["; CAMEL("ciao mondo"); "]|pascal=["; PASCAL("ciao mondo"); "]"
  PRINT "kebab=["; KEBAB("CiaoMondo"); "]|snake=["; SNAKE("CiaoMondo"); "]"
  PRINT "sentence=["; SENTENCE("ciao MONDO"); "]|title=["; TITLE("ciao mondo"); "]"

  ' ── codifiche e formato ──────────────────────────────────────────────────
  PRINT "html=["; HTMLENCODE("<a&b>"); "]"
  PRINT "js=["; JSENCODE("a<b&c'x"); "]"
  PRINT "url=["; URLENCODE("a b&c"); "]"
  PRINT "format=["; FORMAT(1234.5, "#.##"); "]"
  PRINT "str=["; STR(42); "]"

  ' ── validatori ───────────────────────────────────────────────────────────
  PRINT "valid="; ISEMAIL("a@b.it"); "|"; ISURL("https://x.it"); "|"; ISREQUIRED("x")
  PRINT "validlen="; ISLENGTH("ab", 2, 2); "|cc="; ISCREDITCARD("4111111111111111")

  ' ── split / join ─────────────────────────────────────────────────────────
  DIM parts() AS STRING
  parts = SPLIT("a,b,c", ",")
  PRINT "splitN="; UBOUND(parts) + 1; "|join=["; JOIN(parts, "-"); "]"

  ' ── PETSCII e WRAP ───────────────────────────────────────────────────────
  PRINT "petscii="; LEN(PETSCII("{clr}Hello{home}"))
  PRINT "wrap=[";
  PRINT WRAP("ciao mondo lungo", 5);
  PRINT "]"
END SUB
