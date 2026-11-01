' ============================================================================
' TradingPlatform — layout grafico di una piattaforma di trading (DATI DUMMY).
' ============================================================================
' Dimostra la configurazione grafica classica: top-bar, watchlist, chart,
' order book/DOM, order ticket, positions/orders blotter, status bar.
' Tutto costruito su SCREEN grafico + WINDOW (chrome nativo) con contenuto a
' celle renderizzato DENTRO le finestre (testo + eventuali controlli).
'
' Esecuzione / cattura:
'   GMB_GFX_SHOT=/tmp/platform.png tools/gmb_c99native_run.sh \
'     samples/Clipper/TradingPlatform.bas platform
' ============================================================================
IMPORT CONTROLS
IMPORT WINDOWS

SUB Main()
  SCREEN 12, 1600, 1000, 32
  WINDOW THEME "Turbo"

  ' --- Top bar: menu / simbolo / timeframe / equity / ora / stato ---
  WINDOW DEFINE Top FROM 1, 1 TO 3, 80 TITLE "GMB Trader"
  WINDOW Top ACTIVATE
  WINDOW Top PRINT " File  View  Trade  Tools  Help        [ BTCUSDT 1m ]      Equity 102,340.00  PnL +2,340.00  14:32:07  * CONNECTED"

  ' --- Watchlist (sinistra) ---
  WINDOW DEFINE Watch FROM 4, 1 TO 13, 23 TITLE "Watchlist"
  WINDOW Watch PRINT "SYMBOL       LAST     CHG%"
  WINDOW Watch PRINT "BTCUSDT   64,120.5   +1.24"
  WINDOW Watch PRINT "ETHUSDT    3,182.4   +0.87"
  WINDOW Watch PRINT "SOLUSDT      148.2   -0.53"
  WINDOW Watch PRINT "XRPUSDT      0.612   +2.10"
  WINDOW Watch PRINT "BNBUSDT      402.1   -0.21"
  WINDOW Watch PRINT "ADAUSDT      0.451   +0.34"

  ' --- Chart centrale (candele ASCII) ---
  WINDOW DEFINE Chart FROM 4, 24 TO 19, 56 TITLE "BTCUSDT - 1m"
  WINDOW Chart PRINT "  64,300 "
  WINDOW Chart PRINT "        #             #"
  WINDOW Chart PRINT "    #   #     #       #   #"
  WINDOW Chart PRINT "    #   #  #  #    #  #   #"
  WINDOW Chart PRINT " #  #   #  #  # #  #  #   #   #"
  WINDOW Chart PRINT " #  #   #  #  # #  #  #   #   #   #"
  WINDOW Chart PRINT " #  #   #  #  # #  #  #   #   #   #"
  WINDOW Chart PRINT "    #   #  #  # #  #  #       #   #"
  WINDOW Chart PRINT "        #     # #  #  #           #"
  WINDOW Chart PRINT "              # #  #  #"
  WINDOW Chart PRINT "  63,900 "
  WINDOW Chart PRINT "  MA(20) 63,880   RSI 58.4   VOL 1.2M"

  ' --- Order book / DOM (destra) ---
  WINDOW DEFINE Book FROM 4, 57 TO 13, 80 TITLE "Order Book"
  WINDOW Book PRINT "    BID          ASK"
  WINDOW Book PRINT "64,120.0  1.2  64,121.0  0.9"
  WINDOW Book PRINT "64,119.5  3.4  64,121.5  2.1"
  WINDOW Book PRINT "64,119.0  8.1  64,122.0  5.7"
  WINDOW Book PRINT "64,118.5  2.0  64,122.5  1.3"
  WINDOW Book PRINT "64,118.0  6.6  64,123.0  4.2"
  WINDOW Book PRINT "   spread 1.0   tape: 64,120.5 x1.0"

  ' --- Order ticket (destra) ---
  WINDOW DEFINE Ticket FROM 14, 57 TO 19, 80 TITLE "Order Ticket"
  WINDOW Ticket PRINT " Side [BUY] SELL   Type MARKET"
  WINDOW Ticket PRINT " Qty  0.50 BTC     Price 64,120.5"
  WINDOW Ticket PRINT " SL   63,500       TP    65,000"
  WINDOW Ticket PRINT " [ SEND ]   margin 3,206 USD"

  ' --- Positions / Orders blotter (basso) ---
  WINDOW DEFINE Blotter FROM 20, 1 TO 23, 80 TITLE "Positions / Orders"
  WINDOW Blotter PRINT " ID  SYMBOL    SIDE  QTY   ENTRY     MARK      PnL"
  WINDOW Blotter PRINT " 1  BTCUSDT   LONG  0.50  63,900   64,120   +110.25"
  WINDOW Blotter PRINT " 2  ETHUSDT   SHORT 4.00  3,180    3,182    -8.00"

  ' --- Status bar (fondo) ---
  WINDOW DEFINE Status FROM 24, 1 TO 25, 80 TITLE ""
  WINDOW Status PRINT " latency 12ms  tick/s 340  session CRYPTO  threads 8  mem 412MB  risks OK"

  DIM i AS INTEGER
  FOR i = 1 TO 20
    SLEEP 100
  NEXT i
END SUB
