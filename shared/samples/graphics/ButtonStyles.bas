' ButtonStyles.bas — stili bottone: flat, rounded, ombre (soft), bordo, colori,
' font, embossed, 3D raised/sunken, gradient, glow. Default dal tema + override STYLE.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM clicks = 0

SUB Main()
    WINDOW THEME "Nord"     ' i default (colori/forma) vengono dal tema

    BUTTON 30, 20, 250, 40, "Flat blu" ON CLICK OnClick STYLE "flat bg=#1e3a8a fg=#e0f2fe border=1,#22d3ee"
    BUTTON 30, 70, 250, 40, "Rounded + soft shadow" ON CLICK OnClick STYLE "rounded radius=10 softshadow shadow=4,4 bg=#24405f fg=#eaf2ff"
    BUTTON 30, 120, 250, 40, "Embossed" ON CLICK OnClick STYLE "flat bevel=1 emboss bg=#3a3f46 fg=#dfe6ef"
    BUTTON 30, 170, 250, 40, "3D raised" ON CLICK OnClick STYLE "raised bevel=2 bg=#4b7bec fg=white"
    BUTTON 30, 220, 250, 40, "3D sunken" ON CLICK OnClick STYLE "sunken bevel=2 bg=#2f3540 fg=#cfd8e3"
    BUTTON 30, 270, 250, 40, "Gradient" ON CLICK OnClick STYLE "flat gradient=#3b82f6,#1e3a8a fg=white"
    BUTTON 30, 320, 250, 40, "Glow" ON CLICK OnClick STYLE "rounded glow=#22d3ee bg=#0b1220 fg=#67e8f9"
    BUTTON 30, 370, 250, 40, "Font grande" ON CLICK OnClick STYLE "flat bg=#111827 fg=#fbbf24 font=,22"

    BUTTON 320, 20, 250, 40, "Danger" ON CLICK OnClick STYLE "flat bg=#dc2626 fg=white border=1,#7f1d1d"
    BUTTON 320, 70, 250, 40, "Success" ON CLICK OnClick STYLE "rounded radius=6 bg=#16a34a fg=white shadow=2,2"
    BUTTON 320, 120, 250, 40, "Warning" ON CLICK OnClick STYLE "flat gradient=#f59e0b,#b45309 fg=#1f2937 border=1,#7c2d12"
    BUTTON 320, 170, 250, 40, "Neon" ON CLICK OnClick STYLE "rounded glow=#a855f7 bg=#1a0b2e fg=#d8b4fe border=1,#a855f7"
    BUTTON 320, 220, 250, 40, "Pill" ON CLICK OnClick STYLE "rounded radius=20 softshadow bg=#6366f1 fg=white"
    BUTTON 320, 270, 250, 40, "Metal" ON CLICK OnClick STYLE "raised bevel=3 gradient=#9ca3af,#4b5563 fg=#111827"
    BUTTON 320, 320, 250, 40, "Ghost" ON CLICK OnClick STYLE "rounded border=1,#94a3b8 bg=#0b1220 fg=#cbd5e1"
    BUTTON 320, 370, 250, 40, "Disabled" ON CLICK OnClick STYLE "flat bg=#334155 fg=#64748b"

    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB

SUB OnClick()
    clicks = clicks + 1
    WINDOW CLOSE
END SUB
