' NEURAL (FFI) — slice ricorrente: RNN, LSTM, GRU, BiRNN (BPTT + ottimizzatore) e PBP.
' EXPECT: rnn= 1 |f= 0 |y=0.2515,0.2417,0.4177|b= 0 |s= 0 |zg= 0 |f2= 0 |y2=0.1060|z= 0 |lstm= 0 |gru= 0 |birnn= 0 |pbp= 0 ,0.3989,0.3408|pl= 0 |mz=0.1000,0.2000|vz=0.0250,0.1000|fz= 0
' IMPORTA NEURAL (pacchetto gmbneural): solo ciò che non esiste in GMBasic (TORCH copre MLP).
IMPORT NEURAL

SUB Main()
  DIM x(2) AS DOUBLE, y(2) AS DOUBLE, gy(2) AS DOUBLE
  DIM h AS LONG, lh AS LONG, gh AS LONG, bh AS LONG, ph AS LONG
  x(0) = 0.1 : x(1) = 0.2 : x(2) = 0.3
  gy(0) = 1.0 : gy(1) = 0.0 : gy(2) = 0.0

  ' ── RNN: forward, backward, step SGD, forward ──
  h = NEURAL.RNNINIT(1, 2, 1, 42)
  PRINT "rnn="; h;
  PRINT "|f="; NEURAL.RNNFORWARD(h, x, 3, y);
  PRINT "|y="; FORMAT(y(0), "#.0000"); ","; FORMAT(y(1), "#.0000"); ","; FORMAT(y(2), "#.0000");
  PRINT "|b="; NEURAL.RNNBACKWARD(h, gy, 3);
  PRINT "|s="; NEURAL.STEP(h, 0, 0.1);
  PRINT "|zg="; NEURAL.ZEROGRAD(h);
  PRINT "|f2="; NEURAL.RNNFORWARD(h, x, 3, y);
  PRINT "|y2="; FORMAT(y(0), "#.0000");
  PRINT "|z="; NEURAL.FREE(h);

  ' ── LSTM / GRU / BiRNN: init, forward, backward, step, free ──
  lh = NEURAL.LSTMINIT(1, 2, 1, 7)
  PRINT "|lstm="; NEURAL.LSTMFORWARD(lh, x, 3, y) + NEURAL.LSTMBACKWARD(lh, gy, 3) + NEURAL.STEP(lh, 2, 0.01) + NEURAL.FREE(lh);
  gh = NEURAL.GRUINIT(1, 2, 1, 9)
  PRINT "|gru="; NEURAL.GRUFORWARD(gh, x, 3, y) + NEURAL.GRUBACKWARD(gh, gy, 3) + NEURAL.STEP(gh, 1, 0.01) + NEURAL.FREE(gh);
  bh = NEURAL.BIRNNINIT(2, 1, 2, 1, 11)
  PRINT "|birnn="; NEURAL.BIRNNFORWARD(bh, x, 3, y) + NEURAL.BIRNNBACKWARD(bh, gy, 3) + NEURAL.STEP(bh, 2, 0.01) + NEURAL.FREE(bh);

  ' ── PBP: momenti ReLU su N(0,1) ──
  DIM mo(1) AS DOUBLE
  PRINT "|pbp="; NEURAL.PBPMOMENTS(0.0, 1.0, mo); ","; FORMAT(mo(0), "#.0000"); ","; FORMAT(mo(1), "#.0000");

  ' ── PBP layer lineare 2x1 (relu=0): mz = W*mz_prev, vz = W^2*vz_prev ──
  DIM mw(1) AS DOUBLE, vw(1) AS DOUBLE, mzp(0) AS DOUBLE, vzp(0) AS DOUBLE
  DIM mzo(1) AS DOUBLE, vzo(1) AS DOUBLE
  mw(0) = 0.5 : mw(1) = 1.0
  vw(0) = 0.0 : vw(1) = 0.0
  mzp(0) = 0.2 : vzp(0) = 0.1
  ph = NEURAL.PBPLAYERINIT(mw, vw, 1, 2)
  PRINT "|pl="; NEURAL.PBPLAYER(ph, mzp, vzp, 1.0, 0, mzo, vzo);
  PRINT "|mz="; FORMAT(mzo(0), "#.0000"); ","; FORMAT(mzo(1), "#.0000");
  PRINT "|vz="; FORMAT(vzo(0), "#.0000"); ","; FORMAT(vzo(1), "#.0000");
  PRINT "|fz="; NEURAL.FREE(ph)
END SUB
