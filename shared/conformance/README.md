# conformance — `.bas` corpus + `EXPECT` oracle

`run_conformance.c` runs a MiniTest/sample on the **C99Native** leg
(`GMBasic/tools/gmb_c99native_run.sh`) and compares the output with the file's `' EXPECT:` line
(lines joined with `|`).

```
cmake -S conformance -B conformance/build && cmake --build conformance/build
shared/conformance/build/run_conformance GMBasic shared/minitests/bits/bitclear.bas shared/minitests/bits/bitcount.bas
```

Exit: 0 PASS/SKIP (no EXPECT), 1 FAIL, 2 setup. It does not modify the file; the C99Native runner
writes the artifact into `bin/` next to the file (the compiler's default output; `work/` is only
for samples).
