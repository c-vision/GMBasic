REM FILE: 12-SubsetSumERX.bas — due algoritmi didattici (pacchetto `or`): SUBSETSUM con
REM ricostruzione della combinazione e TSPERX (edge recombination crossover) usato dentro un
REM piccolo ciclo genetico. Mostra come si legge un vettore di decisioni e come si compone un
REM operatore genetico, con la verifica esplicita di ciò che l'algoritmo restituisce.
IMPORT OPTIMIZE

' Lunghezza del ciclo t() sulla matrice distanze d() (n*n).
FUNCTION Lung(t() AS LONG, n AS LONG, d() AS DOUBLE) AS DOUBLE
  s = 0
  FOR i = 0 TO n - 1
    a = t(i)
    IF i = n - 1 THEN
      b = t(0)
    ELSE
      b = t(i + 1)
    END IF
    s = s + d(a*n+b)
  NEXT i
  RETURN s
END FUNCTION

' TRUE se t() è una permutazione di 0..n-1 (un giro valido).
FUNCTION IsPerm(t() AS LONG, n AS LONG) AS LONG
  DIM visto(63) AS LONG
  FOR i = 0 TO n - 1
    v = t(i)
    IF v < 0 OR v > n - 1 THEN RETURN 0
    IF visto(v) <> 0 THEN RETURN 0
    visto(v) = 1
  NEXT i
  RETURN 1
END FUNCTION

SUB Main()
  ' ═══ 1. SUBSET SUM: trovare il sottoinsieme che somma esattamente a un target ═════
  ' Esempio didattico: preventivo di spesa. Il vettore take() dice QUALI voci compongono
  ' la somma: va sempre verificato sommando gli item, non solo guardando l'esito 1/0.
  DIM voci(5) AS DOUBLE
  voci(0)=3 : voci(1)=34 : voci(2)=4 : voci(3)=12 : voci(4)=5 : voci(5)=2
  DIM take(5) AS LONG
  target = 9
  r = OPTIMIZE.SUBSETSUM(voci, 6, target, take)
  somma = 0
  PRINT "subset: trovato="; r; " target="; FORMAT(target, "#");
  PRINT "|scelte=";
  FOR i = 0 TO 5
    IF take(i) <> 0 THEN
      PRINT FORMAT(voci(i), "#"); " ";
      somma = somma + voci(i)
    END IF
  NEXT i
  PRINT "|somma="; FORMAT(somma, "#"); ":"; (somma = target)

  ' ═══ 2. TSPERX: crossover di edge recombination fra due genitori ══════════════════
  DIM d(24) AS DOUBLE
  FOR i = 0 TO 4
    FOR j = 0 TO 4
      d(i*5+j) = ABS(i-j)
    NEXT j
  NEXT i
  DIM p1(4) AS LONG
  DIM p2(4) AS LONG
  DIM figlio(4) AS LONG
  FOR i = 0 TO 4
    p1(i) = i
    p2(i) = 4 - i
  NEXT i
  PRINT "erx: padre1="; Lung(p1, 5, d); " padre2="; Lung(p2, 5, d);
  r = OPTIMIZE.TSPERX(p1, p2, 5, figlio)
  PRINT "|figlio="; Lung(figlio, 5, d); "|valido="; IsPerm(figlio, 5)

  ' ═══ 3. Piccolo ciclo genetico: ERX + mutazione, selezione del migliore ═══════════
  ' Due genitori fissi, 20 generazioni: il figlio migliore non deve mai peggiorare il
  ' migliore della generazione precedente (elitismo sul minimo).
  DIM best(4) AS LONG
  FOR i = 0 TO 4 : best(i) = p2(i) : NEXT i
  cbest = Lung(best, 5, d)
  DIM kid(4) AS LONG
  FOR g = 1 TO 20
    r = OPTIMIZE.TSPERX(p1, best, 5, kid)
    IF Lung(kid, 5, d) < cbest THEN
      FOR i = 0 TO 4 : best(i) = kid(i) : NEXT i
      cbest = Lung(best, 5, d)
    END IF
  NEXT g
  PRINT "ga: costo_migliore="; FORMAT(cbest, "#.0"); "|ottimo="; (cbest = 8.0)
  PRINT ""
END SUB
