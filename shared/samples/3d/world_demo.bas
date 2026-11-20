' world_demo.bas — il dominio WORLD del motore 3D, in un programma solo.
'
' Il mondo è quello demo del piano 3D (docs/Piano-Motore-3D.md §6 P2): 5 settori, 8 picture,
' una lampada. Il BASIC non maneggia handle: c'è UN mondo corrente per processo, come per
' DBF/ISAM — WORLD LOAD lo apre, WORLD CLOSE lo chiude e da lì WORLD.TIME() torna 0.
'
' `IMPORT WORLD` è OBBLIGATORIA: il dominio non è implicito. Senza l'IMPORT il programma non
' compila — `namespace non importato: WORLD (aggiungi IMPORT WORLD).` — sia per i comandi sia
' per le letture qualificate (WORLD.TIME() ecc.).
'
' Le quote si stampano arrotondate al centesimo: la simulazione avanza a passi FISSI di 1/60 s,
' quindi la stessa sequenza di TICK dà sempre gli stessi numeri su tutte e quattro le gambe
' (C99Native, IL, PCodeVM, VM/C).
IMPORT WORLD

SUB Main()
  ' ── 1. Caricamento ───────────────────────────────────────────────────────────
  ' LOAD è atomico: se il documento non è valido il mondo precedente resta al suo posto.
  WORLD LOAD "worlds/demo.gmbscene"
  PRINT "atrio: pavimento="; Round2(WORLD.FLOORZ(0)); " soffitto="; Round2(WORLD.CEILZ(0))
  PRINT "atrio in moto? "; WORLD.MOVING(0)

  ' ── 2. Orologio ──────────────────────────────────────────────────────────────
  ' TICK accoda i secondi e il mondo procede a passi fissi; TIME è il tempo simulato.
  WORLD TICK 0.5
  PRINT "dopo 0,5 s: tick="; INT(WORLD.TIME() * 60 + 0.5)

  ' ── 3. Ascensore (SECMOVE a 3 argomenti) ─────────────────────────────────────
  ' Settore 1: il pavimento va da 0 a 32 a 64 unità/s — mezzo secondo di corsa.
  WORLD SECMOVE 1, 64, 32
  PRINT "ascensore in moto? "; WORLD.MOVING(1)
  WORLD TICK 0.25
  PRINT "a metà corsa: pavimento="; Round2(WORLD.FLOORZ(1))
  WORLD TICK 0.5
  PRINT "arrivato: in moto? "; WORLD.MOVING(1); " pavimento="; Round2(WORLD.FLOORZ(1))

  ' ── 4. Porta con soffitto (SECMOVE a 4 argomenti) ────────────────────────────
  ' Settore 2: pavimento 0 -> 16 e soffitto 64 -> 48 alla stessa velocità. Il mover
  ' resta in moto finché ENTRAMBE le quote sono arrivate.
  WORLD SECMOVE 2, 128, 16, 48
  WORLD TICK 0.5
  PRINT "porta: pavimento="; Round2(WORLD.FLOORZ(2)); " soffitto="; Round2(WORLD.CEILZ(2))
  PRINT "porta in moto? "; WORLD.MOVING(2)

  ' ── 5. Trituratore: HEIGHT ferma il mover ────────────────────────────────────
  ' Settore 3: pavimento verso 100 a 60 unità/s; dopo 0,5 s è a 30 e HEIGHT lo blocca lì.
  WORLD SECMOVE 3, 60, 100
  WORLD TICK 0.5
  WORLD HEIGHT 3
  PRINT "trituratore fermato: in moto? "; WORLD.MOVING(3); " pavimento="; Round2(WORLD.FLOORZ(3))

  ' ── 6. Animazione delle picture ──────────────────────────────────────────────
  ' Settore 0: cicla le picture 0..3 a 4 fps (8 picture disponibili nel mondo demo).
  WORLD ANIM 0, 0, 4, 4

  ' ── 7. Scorrimento di piani e pareti ─────────────────────────────────────────
  ' what 0 = piani, what 1 = pareti; u/s e v/s.
  WORLD SCROLL 0, 0, 8, 0
  WORLD SCROLL 0, 1, 0, 4

  ' ── 8. Luce dinamica ─────────────────────────────────────────────────────────
  ' Settore 4: pulse al 50% a 1 Hz; dopo un quarto di periodo il livello è 1,5.
  WORLD LIGHTFX 4, 2, 0.5, 1
  WORLD TICK 0.25
  PRINT "luce pulse="; Round2(WORLD.LIGHTNOW(4))
  ' modo 0 = spegne l'effetto e riporta la luce a 1.
  WORLD LIGHTFX 4, 0, 0, 1
  PRINT "luce normale="; Round2(WORLD.LIGHTNOW(4))

  ' ── 9. Lampada puntiforme ────────────────────────────────────────────────────
  ' x, y, z, raggio, livello (0..1). Il mondo demo ne ha già una: il massimo è 8.
  WORLD LAMP 200, 64, 58, 56, 0.35
  WORLD TICK 0.25
  PRINT "dopo LAMP: tick="; INT(WORLD.TIME() * 60 + 0.5)

  ' ── 10. Chiusura ────────────────────────────────────────────────────────────
  ' Dopo CLOSE non c'è nessun mondo: TIME torna 0 e le letture danno i valori "senza mondo".
  WORLD CLOSE
  PRINT "dopo CLOSE: tick="; INT(WORLD.TIME() * 60); " movente? "; WORLD.MOVING(0)
END SUB

' Arrotonda al centesimo per una stampa stabile (la simulazione è a passi fissi, ma i
' valori intermedi possono avere rumore di float nell'ultima cifra).
FUNCTION Round2(v AS DOUBLE) AS DOUBLE
  Round2 = INT(v * 100 + 0.5) / 100
END FUNCTION
