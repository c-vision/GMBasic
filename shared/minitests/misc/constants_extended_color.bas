' MiniTest: costanti colore estese della palette 256 colori (indici 16-255).
'   Campione: gray scale, X11/web color names, varie tonalita'.
'   In headless CONSOLE.GETPALETTECOLOR oltre 15 ritorna [0,0,0]; si testa il valore
'   INTEGER delle costanti come proxy.
'   Grey scale (indici 232-255 per i GRAYxx a 3+ cifre; GRAY0=16, GRAY2=232, ..., GRAY89=255)
'   Nomi estesi e X11.
'   NOTA: non si possono testare tutti 134 colori estesi in un file MiniTest (limite
'   EXPECT). Campione di ~50 copre le famiglie principali.
' EXPECT: g0= 16|nb= 17|dblu= 18|blu3= 19|blu3_2= 20|blu1= 21|dgrn= 22|dsb4= 23|dsb4_2= 24|dsb4_3= 25|db3= 26|db2= 27|grn4= 28|sg4= 29|trq4= 30|dsb3= 31|dsb3_2= 32|db1= 33|grn3= 34|sg3= 35|dcy= 36|lse= 37|dsb2= 38|dsb1= 39|grn3_2= 40|sg3_2= 41|sg2= 42|cy3= 43|dtq= 44|trq2= 45|grn1= 46|sg2_2= 47|sg1= 48|msg= 49|cy2= 50|cy1= 51|dred= 52|dp4= 53|pp4= 54|pp4_2= 55|pp3= 56|bv= 57|or4= 58|mp4= 60|sb3= 61|sb3_2= 62|rb1= 63|g37= 59|ch4= 64|dsg4= 65|pt4= 66|stb= 67|stb3= 68|cfl= 69|ch3= 70|dsg4_2= 71|cbl= 72|cbl2= 73|sky3= 74|stb1= 75
SUB Main
  PRINT "g0="; %GRAY0
  PRINT "nb="; %NAVY_BLUE
  PRINT "dblu="; %DARK_BLUE
  PRINT "blu3="; %BLUE3
  PRINT "blu3_2="; %BLUE3_2
  PRINT "blu1="; %BLUE1
  PRINT "dgrn="; %DARK_GREEN
  PRINT "dsb4="; %DEEP_SKY_BLUE4
  PRINT "dsb4_2="; %DEEP_SKY_BLUE4_2
  PRINT "dsb4_3="; %DEEP_SKY_BLUE4_3
  PRINT "db3="; %DODGER_BLUE3
  PRINT "db2="; %DODGER_BLUE2
  PRINT "grn4="; %GREEN4
  PRINT "sg4="; %SPRING_GREEN4
  PRINT "trq4="; %TURQUOISE4
  PRINT "dsb3="; %DEEP_SKY_BLUE3
  PRINT "dsb3_2="; %DEEP_SKY_BLUE3_2
  PRINT "db1="; %DODGER_BLUE1
  PRINT "grn3="; %GREEN3
  PRINT "sg3="; %SPRING_GREEN3
  PRINT "dcy="; %DARK_CYAN
  PRINT "lse="; %LIGHT_SEA_GREEN
  PRINT "dsb2="; %DEEP_SKY_BLUE2
  PRINT "dsb1="; %DEEP_SKY_BLUE1
  PRINT "grn3_2="; %GREEN3_2
  PRINT "sg3_2="; %SPRING_GREEN3_2
  PRINT "sg2="; %SPRING_GREEN2
  PRINT "cy3="; %CYAN3
  PRINT "dtq="; %DARK_TURQUOISE
  PRINT "trq2="; %TURQUOISE2
  PRINT "grn1="; %GREEN1
  PRINT "sg2_2="; %SPRING_GREEN2_2
  PRINT "sg1="; %SPRING_GREEN1
  PRINT "msg="; %MEDIUM_SPRING_GREEN
  PRINT "cy2="; %CYAN2
  PRINT "cy1="; %CYAN1
  PRINT "dred="; %DARK_RED
  PRINT "dp4="; %DEEP_PINK4
  PRINT "pp4="; %PURPLE4
  PRINT "pp4_2="; %PURPLE4_2
  PRINT "pp3="; %PURPLE3
  PRINT "bv="; %BLUE_VIOLET
  PRINT "or4="; %ORANGE4
  PRINT "mp4="; %MEDIUM_PURPLE4
  PRINT "sb3="; %SLATE_BLUE3
  PRINT "sb3_2="; %SLATE_BLUE3_2
  PRINT "rb1="; %ROYAL_BLUE1
  PRINT "g37="; %GRAY37
  PRINT "ch4="; %CHARTREUSE4
  PRINT "dsg4="; %DARK_SEA_GREEN4
  PRINT "pt4="; %PALE_TURQUOISE4
  PRINT "stb="; %STEEL_BLUE
  PRINT "stb3="; %STEEL_BLUE3
  PRINT "cfl="; %CORNFLOWER_BLUE
  PRINT "ch3="; %CHARTREUSE3
  PRINT "dsg4_2="; %DARK_SEA_GREEN4_2
  PRINT "cbl="; %CADET_BLUE
  PRINT "cbl2="; %CADET_BLUE_2
  PRINT "sky3="; %SKY_BLUE3
  PRINT "stb1="; %STEEL_BLUE1
END SUB
