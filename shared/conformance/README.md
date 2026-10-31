# conformance — corpus `.bas` + oracolo `EXPECT`

`run_conformance.c` esegue un MiniTest/sample con la gamba **C99Native**
(`GMBasic/tools/gmb_c99native_run.sh`) e confronta l'output con la riga
`' EXPECT:` del file (righe unite con `|`).

```
cmake -S conformance -B conformance/build && cmake --build conformance/build
shared/conformance/build/run_conformance GMBasic shared/minitests/bits/bitclear.bas shared/minitests/bits/bitcount.bas
```

Exit: 0 PASS/SKIP (senza EXPECT), 1 FAIL, 2 setup. Non modifica il file; il runner C99Native
scrive l'artefatto in `bin/` accanto al file (output di default del compilatore; `work/` è solo per i sample).
