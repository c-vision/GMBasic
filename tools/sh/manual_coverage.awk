# manual_coverage.awk — genera A1 e il TSV di copertura in una passata.
# stdin: procs.tsv (ns, name, arityMin, arityMax, kind)
# Livelli di esempio: codice qualificato -> codice a nome nudo -> guida -> sintetico.
BEGIN {
  while ((getline l < corpus) > 0) { n = split(l, p, "\t"); if (n >= 2 && !(p[1] in C)) C[p[1]] = p[2] }
  while ((getline l < bare) > 0) { n = split(l, p, "\t"); if (n >= 2 && !(p[1] in BA)) BA[p[1]] = p[2] }
  while ((getline l < guides) > 0) { n = split(l, p, "\t"); if (n >= 2 && !(p[1] in G)) G[p[1]] = p[2] }
  while ((getline l < btok) > 0) if (l != "") B[l] = 1
  while ((getline l < bcode) > 0) if (l != "") BC[l] = 1
  f3 = "```"
  print "# A1 — Reference delle funzioni canoniche" > a1
  print "" > a1
  print "> Generata da `tools/sh/manual_coverage.sh` dal registry canonico" > a1
  print "> (`shared/spec/registry/canonical.registry.json`, schema `gmb.registry@1`)." > a1
  print "> **Una voce per ogni simbolo registrato**, con un esempio: dal codice del progetto" > a1
  print "> quando esiste (chiamata qualificata o a nome nudo), altrimenti da una guida del" > a1
  print "> vault, altrimenti **sintetico** (firma con arita, non eseguito)." > a1
  print "" > a1
  printf "simbolo\ttipo\tarita\tin_libro\tesempio_in_codice\tfonte_esempio\n" > tsv
}
{
  ns = $1; name = $2; amin = $3; amax = $4; kind = $5
  sym = toupper(ns "." name); bare_name = toupper(name)
  inb = (sym in B) ? "si" : "no"
  inc = (sym in BC) ? "si" : "no"
  src = "-"; tot++
  if (inb == "si") nb++
  if (inc == "si") nc++
  if (sym in C) { src = C[sym]; nreal++ }
  else if (bare_name in BA) { src = BA[bare_name]; nbare++ }
  else if (sym in G) { src = "guida del vault"; nguide++ }
  else nsynth++
  printf "%s\t%s\t%s-%s\t%s\t%s\t%s\n", ns "." name, kind, amin, amax, inb, inc, src >> tsv
  printf "#### `%s.%s` — arita %s-%s\n\n", ns, name, amin, amax >> a1
  if (sym in C) {
    printf "%sbasic\n%s   ' dal codice del progetto: %s\n%s\n\n", f3, sym, C[sym], f3 >> a1
  } else if (bare_name in BA) {
    printf "%sbasic\n%s   ' nel codice a nome nudo: %s\n%s\n\n", f3, sym, BA[bare_name], f3 >> a1
  } else if (sym in G) {
    printf "%sbasic\n%s   ' dalla guida del vault: %s\n%s\n\n", f3, sym, G[sym], f3 >> a1
  } else {
    printf "%sbasic\n' %s(...) — esempio sintetico: firma con arita %s-%s, nessun uso nel corpus ne nelle guide\n%s\n\n", f3, sym, amin, amax, f3 >> a1
  }
}
END {
  printf "FUNZIONI/COMANDI QUALIFICATI: %d\n", tot
  printf "  citati nel libro editoriale : %d (%.0f%%)\n", nb, (tot ? nb * 100 / tot : 0)
  printf "  con esempio in un blocco    : %d (%.0f%%)\n", nc, (tot ? nc * 100 / tot : 0)
  printf "  esempio qualificato         : %d (%.0f%%)\n", nreal, (tot ? nreal * 100 / tot : 0)
  printf "  esempio a nome nudo         : %d (%.0f%%)\n", nbare, (tot ? nbare * 100 / tot : 0)
  printf "  esempio da guida del vault  : %d (%.0f%%)\n", nguide, (tot ? nguide * 100 / tot : 0)
  printf "  solo sintetico              : %d (%.0f%%)\n", nsynth, (tot ? nsynth * 100 / tot : 0)
}
