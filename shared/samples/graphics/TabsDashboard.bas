' TabsDashboard.bas — dashboard GRAFICA con tutte le famiglie di controlli su 5 schede.
' Layout a griglia in coordinate PIXEL (nessuna cella), font di sistema della UI.
' Ogni controllo si registra nel MANIFESTO (ctl$/ctlp): il manifesto pilota la
' visibilità delle schede E il CHECK LAYOUT (fuori area + sovrapposizioni), così un
' errore di posizionamento non passa inosservato: finisce nel log come LAYOUT-FAIL.
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12, 1280, 940, 32
WINDOW THEME "glass"
COLOR 15, 0
CLS 0

DIM i AS INTEGER, pg AS INTEGER, k AS INTEGER, nt AS INTEGER
DIM v1 AS INTEGER, v2 AS INTEGER, v3 AS INTEGER, v4 AS INTEGER, v5 AS INTEGER
DIM v6 AS INTEGER, v7 AS INTEGER, v8 AS INTEGER, v9 AS INTEGER, v10 AS INTEGER
DIM v11 AS INTEGER, v12 AS INTEGER, v13 AS INTEGER, v14 AS INTEGER, v15 AS INTEGER
DIM v16 AS INTEGER, v17 AS INTEGER, v18 AS INTEGER, v19 AS INTEGER, v20 AS INTEGER
DIM s AS STRING, s3 AS STRING, s4 AS STRING, s5 AS STRING, ps AS STRING
DIM nct AS INTEGER, viol AS INTEGER
DIM curpage AS INTEGER, switchok AS INTEGER
DIM badmsg AS STRING
DIM ctl$(120) AS STRING
DIM ctlp(120) AS INTEGER
nct = 0

' ── manifesto: registra un controllo nella pagina p ──────────────────────
SUB Reg(nm AS STRING, p AS INTEGER)
  ctl$(nct) = nm
  ctlp(nct) = p
  nct = nct + 1
END SUB

' ── commutazione scheda (pagina 0 = cornice, sempre visibile) ────────────
SUB ShowPage(p AS INTEGER)
  DIM q AS INTEGER
  FOR q = 0 TO nct - 1
    IF ctlp(q) = p OR ctlp(q) = 0 THEN
      CONTROL SET ctl$(q), "visible", 1
    ELSE
      CONTROL SET ctl$(q), "visible", 0
    END IF
  NEXT q
  CONTROL SET "p0_d", "value", p - 1
END SUB

' ── verifica geometrica della pagina: 0 = tutto a posto ──────────────────
' Fuori area (20,120)-(1260,884) = violazione. Due rect si sovrappongono solo se
' nessuno dei due CONTIENE l'altro (pannello e figlio, invece, sono annidati).
FUNCTION CheckLayout(p AS INTEGER) AS INTEGER
  DIM q AS INTEGER, r AS INTEGER, v AS INTEGER
  DIM ax AS INTEGER, ay AS INTEGER, aw AS INTEGER, ah AS INTEGER, av AS INTEGER
  DIM bx AS INTEGER, by AS INTEGER, bw AS INTEGER, bh AS INTEGER, bv AS INTEGER
  DIM inside AS INTEGER, ov AS INTEGER, bad AS INTEGER
  v = 0
  FOR q = 0 TO nct - 1
    IF ctlp(q) = p THEN
      CONTROL GET ctl$(q), "left" INTO ax
      CONTROL GET ctl$(q), "top" INTO ay
      CONTROL GET ctl$(q), "width" INTO aw
      CONTROL GET ctl$(q), "height" INTO ah
      CONTROL GET ctl$(q), "visible" INTO av
      IF aw > 0 AND ah > 0 AND av > 0 THEN
        bad = 0
        IF ax < 20 OR ay < 120 OR ax + aw > 1260 OR ay + ah > 884 THEN bad = 1
        FOR r = 0 TO q - 1
          IF ctlp(r) = p THEN
            CONTROL GET ctl$(r), "left" INTO bx
            CONTROL GET ctl$(r), "top" INTO by
            CONTROL GET ctl$(r), "width" INTO bw
            CONTROL GET ctl$(r), "height" INTO bh
            CONTROL GET ctl$(r), "visible" INTO bv
            IF bw > 0 AND bh > 0 AND bv > 0 THEN
              inside = 0
              IF ax >= bx AND ay >= by AND ax + aw <= bx + bw AND ay + ah <= by + bh THEN inside = 1
              IF bx >= ax AND by >= ay AND bx + bw <= ax + aw AND by + bh <= ay + ah THEN inside = 1
              ov = 0
              IF ax < bx + bw AND ax + aw > bx AND ay < by + bh AND ay + ah > by THEN ov = 1
              IF ov = 1 AND inside = 0 THEN bad = 1
            END IF
          END IF
        NEXT r
        IF bad = 1 THEN
          badmsg = badmsg + " p" + STR$(p) + ":" + ctl$(q)
          v = v + 1
        END IF
      END IF
    END IF
  NEXT q
  CheckLayout = v
END FUNCTION

' ── cornice fissa (pagina 0: sempre visibile) ────────────────────────────
PANEL 0, 0, 1280, 64, "" NAME "p0_a"
CALL Reg("p0_a", 0)
BIGTEXT 20, 8, "GMBASIC · Dashboard" STYLE "font=22" NAME "p0_b"
CALL Reg("p0_b", 0)
LABEL 20, 43, 900, "tutte le famiglie di controlli · tema glass · font di sistema" NAME "p0_c"
CALL Reg("p0_c", 0)
TAB 20, 72, 1240, "Overview|Data|Files & Print|Clipper|Charts" INTO nt NAME "p0_d"
CALL Reg("p0_d", 0)
STATUSBAR 20, 892, 1240, "ready · 5 schede · 96 controlli" NAME "p0_e"
CALL Reg("p0_e", 0)

' ══ PAGINA 1 · OVERVIEW ════════════════════════════════════════════════
PANEL 20, 120, 400, 764, "KPI" NAME "p1_a"
CALL Reg("p1_a", 1)
LABEL 40, 164, 360, "MRR · mese corrente" NAME "p1_b"
CALL Reg("p1_b", 1)
BIGTEXT 40, 182, "48.2k" STYLE "font=34" NAME "p1_c"
CALL Reg("p1_c", 1)
PROGRESS 40, 244, 360, 78 NAME "p1_d"
CALL Reg("p1_d", 1)
LABEL 40, 268, 360, "target 60k · 80% del piano" NAME "p1_e"
CALL Reg("p1_e", 1)
LABEL 40, 318, 360, "Throughput · req/s" NAME "p1_f"
CALL Reg("p1_f", 1)
BIGTEXT 40, 336, "2.4k" STYLE "font=34" NAME "p1_g"
CALL Reg("p1_g", 1)
PROGRESS 40, 398, 360, 41 NAME "p1_h"
CALL Reg("p1_h", 1)
LABEL 40, 422, 360, "p95 38 ms · errori 0.01%" NAME "p1_i"
CALL Reg("p1_i", 1)
LABEL 40, 472, 360, "Sessioni attive" NAME "p1_j"
CALL Reg("p1_j", 1)
LISTBOX 40, 496, 360, 340, "web-01|web-02|web-03|api-01|api-02|worker-1|worker-2|worker-3" INTO v1 NAME "p1_k"
CALL Reg("p1_k", 1)

PANEL 436, 120, 400, 764, "CONTROLLI" NAME "p1_l"
CALL Reg("p1_l", 1)
LABEL 456, 164, 360, "Aggiornamento live" NAME "p1_m"
CALL Reg("p1_m", 1)
TOGGLE 456, 186, 360, "live updates" INTO v2 NAME "p1_n"
CALL Reg("p1_n", 1)
LABEL 456, 240, 360, "Autoscaling" NAME "p1_o"
CALL Reg("p1_o", 1)
CHECKBOX 456, 262, 360, "autoscaling abilitato" INTO v3 NAME "p1_p"
CALL Reg("p1_p", 1)
LABEL 456, 316, 360, "Modalità di deploy" NAME "p1_q"
CALL Reg("p1_q", 1)
RADIO 456, 338, 360, "Fast|Balanced|Safe" INTO v4 NAME "p1_r"
CALL Reg("p1_r", 1)
LABEL 456, 440, 360, "Soglia CPU" NAME "p1_s"
CALL Reg("p1_s", 1)
SLIDER 456, 462, 360, 0, 100, 72 INTO v5 NAME "p1_t"
CALL Reg("p1_t", 1)
LABEL 456, 530, 360, "Utilizzo CPU" NAME "p1_u"
CALL Reg("p1_u", 1)
PROGRESS 456, 552, 360, 72 NAME "p1_v"
CALL Reg("p1_v", 1)
LABEL 456, 600, 360, "Repliche" NAME "p1_w"
CALL Reg("p1_w", 1)
NUMERICUPDOWN 456, 620, 180, 1, 32, 3 INTO v6 NAME "p1_x"
CALL Reg("p1_x", 1)
BUTTON 660, 620, 156, 38, "Applica" STYLE "primary" NAME "p1_y"
CALL Reg("p1_y", 1)
LABEL 456, 684, 360, "I valori restano sincronizzati nelle variabili INTO" NAME "p1_z"
CALL Reg("p1_z", 1)

PANEL 852, 120, 408, 764, "ATTIVITÀ" NAME "p1_aa"
CALL Reg("p1_aa", 1)
LABEL 872, 164, 368, "Eventi recenti" NAME "p1_ab"
CALL Reg("p1_ab", 1)
LISTBOX 872, 186, 368, 300, "deploy web-02 ok|scale api 3-4|cache primed|sync db replica|alert cpu 91%|rollback dns|probe ok|report inviato" INTO v7 NAME "p1_ac"
CALL Reg("p1_ac", 1)
LABEL 872, 506, 368, "Stato canali" NAME "p1_ad"
CALL Reg("p1_ad", 1)
PROGRESS 872, 528, 368, 64 NAME "p1_ae"
CALL Reg("p1_ae", 1)
PROGRESS 872, 562, 368, 88 NAME "p1_af"
CALL Reg("p1_af", 1)
LABEL 872, 590, 368, "repliche 3/3 · lag 12 ms" NAME "p1_ag"
CALL Reg("p1_ag", 1)
BIGTEXT 872, 630, "99.98%" STYLE "font=28" NAME "p1_ah"
CALL Reg("p1_ah", 1)
LABEL 872, 684, 368, "uptime ultimi 30 giorni" NAME "p1_ai"
CALL Reg("p1_ai", 1)
LABEL 872, 730, 368, "SLA" NAME "p1_aj"
CALL Reg("p1_aj", 1)
SPINNER 1090, 752, 150, "idle|busy|done" INTO v8 NAME "p1_al"
CALL Reg("p1_al", 1)

' ══ PAGINA 2 · DATA ════════════════════════════════════════════════════
PANEL 20, 120, 600, 400, "RECORDS" NAME "p2_a"
CALL Reg("p2_a", 2)
GRID 36, 160, 568, 344, "Node|Region|Load|Status", "alpha|eu-west|62%|ok;beta|us-east|48%|ok;gamma|ap-south|71%|warn;delta|eu-north|33%|ok;epsilon|us-west|89%|crit;zeta|eu-west|54%|ok" INTO v9 NAME "p2_b"
CALL Reg("p2_b", 2)
PANEL 636, 120, 624, 400, "TREE" NAME "p2_c"
CALL Reg("p2_c", 2)
TREEVIEW 652, 160, 592, 344, "root/api|root/api/v1|root/api/v2|root/db|root/db/primary|root/db/replica" INTO v10 NAME "p2_d"
CALL Reg("p2_d", 2)
PANEL 20, 536, 600, 348, "SCROLLVIEW" NAME "p2_e"
CALL Reg("p2_e", 2)
SCROLLVIEW 36, 576, 528, 240, "alpha|beta|gamma|delta|epsilon|zeta|eta|theta|iota|kappa|lambda|mu" NAME "p2_f"
CALL Reg("p2_f", 2)
SCROLLBAR 36, 824, 528, 0, 100, 40 INTO v11 NAME "p2_g"
CALL Reg("p2_g", 2)
PANEL 636, 536, 624, 348, "FILTERS" NAME "p2_h"
CALL Reg("p2_h", 2)
LABEL 652, 572, 280, "Azione" NAME "p2_i"
CALL Reg("p2_i", 2)
COMBOBOX 652, 594, 280, "deploy|restart|scale|rollback" INTO s3 NAME "p2_j"
CALL Reg("p2_j", 2)
LABEL 652, 640, 280, "Coda" NAME "p2_k"
CALL Reg("p2_k", 2)
DATALIST 652, 662, 280, 120, "restart|drain|scale" INTO v12 NAME "p2_l"
CALL Reg("p2_l", 2)
LABEL 952, 572, 292, "Policy" NAME "p2_m"
CALL Reg("p2_m", 2)
PROPERTYGRID 952, 594, 292, 92, "policy=rolling|window=5m|strategy=canary" NAME "p2_n"
CALL Reg("p2_n", 2)
LABEL 952, 706, 292, "Note" NAME "p2_o"
CALL Reg("p2_o", 2)
LISTBOX 952, 728, 292, 120, "canary 25%|hold 5m|auto-rollback" INTO v13 NAME "p2_p"
CALL Reg("p2_p", 2)

' ══ PAGINA 3 · FILES & PRINT ════════════════════════════════════════════
FILEDIALOG "*.bas", "Apri", "*.bas" NAME "p3_a"
CONTROL SET "p3_a", "left", 20
CONTROL SET "p3_a", "top", 120
CALL Reg("p3_a", 3)
PRINTERDIALOG "Stampa", ps, v14, v15 NAME "p3_b"
CONTROL SET "p3_b", "left", 556
CONTROL SET "p3_b", "top", 120
CALL Reg("p3_b", 3)
PANEL 1092, 120, 168, 420, "AZIONI" NAME "p3_c"
CALL Reg("p3_c", 3)
IMAGEBUTTON 1106, 164, 140, 40, "Apri file" NAME "p3_d"
CALL Reg("p3_d", 3)
TEXTBOX 1106, 222, 140, 34 INTO s4 NAME "p3_e"
CALL Reg("p3_e", 3)
QR 1106, 276, 96, 96, "GMBASIC" NAME "p3_f"
CALL Reg("p3_f", 3)
STARRATING 1106, 392, 140, 5, 4 INTO v16 NAME "p3_g"
CALL Reg("p3_g", 3)
LABEL 1106, 444, 140, "valutazione" NAME "p3_h"
CALL Reg("p3_h", 3)
PANEL 20, 556, 1232, 328, "FILE EXPLORER" NAME "p3_i"
CALL Reg("p3_i", 3)
FILEEXPLORER 36, 596, 1200, 236, "." NAME "p3_j"
CALL Reg("p3_j", 3)
SPLITTER 36, 846, 1200, 20, 0 NAME "p3_k"
CALL Reg("p3_k", 3)

' ══ PAGINA 4 · CLIPPER ═════════════════════════════════════════════════
PANEL 20, 120, 760, 560, "TBROWSE · @SELECT (Clipper)" NAME "p4_a"
CALL Reg("p4_a", 4)
TBROWSE 36, 160, 728, 504, [["Name", 22], ["Qty", 8], ["Status", 12]], [["alpha", 12, "ok"], ["beta", 7, "ok"], ["gamma", 3, "warn"], ["delta", 21, "ok"], ["epsilon", 9, "crit"], ["zeta", 15, "ok"], ["eta", 4, "ok"], ["theta", 18, "ok"]] INTO v17 NAME "p4_b"
CALL Reg("p4_b", 4)
PANEL 796, 120, 464, 300, "CLIPPER · @SAY / @GET / MENU" NAME "p4_c"
CALL Reg("p4_c", 4)
LABEL 812, 158, 432, "Cliente: Mario Rossi · Saldo 1.240,00" NAME "p4_d"
CALL Reg("p4_d", 4)
TEXTBOX 812, 184, 432, 34 INTO s5 NAME "p4_e"
CALL Reg("p4_e", 4)
MENU 812, 234, 432, "File|Edit|View|Help" NAME "p4_f"
CALL Reg("p4_f", 4)
CONTEXTMENU 812, 282, 432, "Copy|Paste|Delete" NAME "p4_g"
CALL Reg("p4_g", 4)
LABEL 812, 336, 432, "menubar + context menu" NAME "p4_h"
CALL Reg("p4_h", 4)
PANEL 796, 436, 464, 244, "DIALOGHI" NAME "p4_i"
CALL Reg("p4_i", 4)
ALERTBOX 812, 470, 210, 120, "Deploy completato" NAME "p4_j"
CALL Reg("p4_j", 4)
CONFIRMATIONBOX 1036, 470, 208, 120, "Procedere?" NAME "p4_k"
CALL Reg("p4_k", 4)
LABEL 812, 606, 432, "ALERTBOX / CONFIRMATIONBOX" NAME "p4_l"
CALL Reg("p4_l", 4)
PANEL 20, 696, 1232, 188, "FUNCTION KEY BAR · COLOR PICKER" NAME "p4_m"
CALL Reg("p4_m", 4)
FUNCTIONKEYBAR 730, 36, 1200, "F1 Aiuto|F2 Salva|F3 Annulla|F4 Esci" NAME "p4_n"
CALL Reg("p4_n", 4)
COLORPICKER 36, 780, 300, 48 INTO v18 NAME "p4_o"
CALL Reg("p4_o", 4)

' ══ PAGINA 5 · CHARTS ══════════════════════════════════════════════════
PANEL 20, 120, 600, 380, "LINE CHART" NAME "p5_a"
CALL Reg("p5_a", 5)
CHART 36, 160, 568, 324, "1,18;2,22;3,19;4,28;5,26;6,34;7,30;8,38;9,35;10,44;11,40;12,48" NAME "p5_b"
CALL Reg("p5_b", 5)
PANEL 636, 120, 624, 380, "CANDLESTICK · DASHBOARD" NAME "p5_c"
CALL Reg("p5_c", 5)
CANDLESTICK 652, 160, 592, 240, "10,13,9,12;12,14,11,13;13,15,12,11;11,12,10,9;9,11,8,10;10,13,9,12" NAME "p5_d"
CALL Reg("p5_d", 5)
DASHBOARD 652, 412, 592, 76, "CPU|72|100|%|MEM|58|100|%|DISK|31|100|%" INTO v19 NAME "p5_e"
CALL Reg("p5_e", 5)
PANEL 20, 516, 600, 368, "LOGVIEW" NAME "p5_f"
CALL Reg("p5_f", 5)
LOGVIEW 36, 556, 568, 312, "boot ok|warmup done|cache primed|sync complete|gfx ready" NAME "p5_g"
CALL Reg("p5_g", 5)
PANEL 636, 516, 300, 368, "HEXVIEW" NAME "p5_h"
CALL Reg("p5_h", 5)
HEXVIEW 652, 556, 268, 312, "474D42617369634F70733A206F6E6C696E650A" NAME "p5_i"
CALL Reg("p5_i", 5)
PANEL 952, 516, 308, 368, "CODE · RICHTEXT" NAME "p5_j"
CALL Reg("p5_j", 5)
CODEAREA 968, 556, 276, 120, "x = 1|y = 2|PRINT x + y" NAME "p5_k"
CALL Reg("p5_k", 5)
RICHTEXT 968, 688, 276, 180, "Deploy completato su 3 nodi|Nessun errore|Rollback disponibile" NAME "p5_l"
CALL Reg("p5_l", 5)

' ── 5 schede: mostra, verifica, salva (fase automatica) ─────────────────
FOR pg = 1 TO 5
  CALL ShowPage(pg)
  curpage = pg
  FOR i = 1 TO 24 : WINDOW PUMP 16 : NEXT
  IF pg = 1 THEN SAVE SCREEN "work/tab1.png"
  IF pg = 2 THEN SAVE SCREEN "work/tab2.png"
  IF pg = 3 THEN SAVE SCREEN "work/tab3.png"
  IF pg = 4 THEN SAVE SCREEN "work/tab4.png"
  IF pg = 5 THEN SAVE SCREEN "work/tab5.png"
NEXT pg

' ── check layout su ogni pagina (solo i controlli VISIBILI della pagina) ─
FOR pg = 1 TO 5
  CALL ShowPage(pg)
  curpage = pg
  FOR i = 1 TO 4 : WINDOW PUMP 16 : NEXT
  k = CheckLayout(pg)
  viol = viol + k
NEXT pg

' ── CAMBIO SCHEDA: il valore del TAB (0..4) è la pagina. È lo stesso percorso
'    che fa il CLICK sulla scheda, quindi la pagina cambia davvero in finestra. ──
SUB PumpPages(n AS INTEGER)
  DIM z AS INTEGER, np AS INTEGER
  FOR z = 1 TO n
    WINDOW PUMP 16
    np = INT(nt + 0.5) + 1
    IF np >= 1 AND np <= 5 THEN
      IF np <> curpage THEN
        curpage = np
        CALL ShowPage(np)
      END IF
    END IF
    IF WINDOWCLOSED() <> 0 THEN EXIT FOR
  NEXT z
END SUB

' prova headless del cambio pagina: come un click sulla terza scheda
CALL ShowPage(1)
curpage = 1
CONTROL SET "p0_d", "value", 2
CALL PumpPages(60)
IF curpage = 3 THEN switchok = 1
SAVE SCREEN "work/tab_switch.png"

' ── finestra INTERATTIVA: clic sulle schede, fino a chiusura o ~30 s ────
CALL PumpPages(1800)

SCREEN 0
PRINT "CONTROLLI="; nct
PRINT "SWITCH-OK="; switchok
IF viol = 0 THEN PRINT "LAYOUT-TOTAL=0" ELSE PRINT "LAYOUT-TOTAL="; viol; badmsg
PRINT "TABS-OK"
