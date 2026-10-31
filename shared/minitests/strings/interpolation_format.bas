' EXPECT: avg=40.8333333333333|n=10|t=[1, 2]|neg=-2.5
' MiniTest: formattazione in interpolazione $"..." (e concatenazione)
' - numeri: G15 in cultura INVARIANTE (40.8333333333333, non 40,833… con la virgola)
' - interi: senza decimali (10, non 10.0)
' - collezioni/tuple: forma compatta "[e0, e1]" (non "System.Object[]")
' Copre la divergenza di formattazione corretta su IL/PCode/VM-C (ConcatFormat /
' gmb_format_value_compact / CollectionToString).
IMPORT CONSOLE
SUB Main()
    DIM x = 245 / 6
    DIM t = (1, 2)
    PRINT $"avg={x}"
    PRINT $"n={10}"
    PRINT $"t={t}"
    PRINT $"neg={-2.5}"
END SUB
