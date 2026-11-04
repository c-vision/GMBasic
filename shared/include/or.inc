' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' or.inc — namespace OPTIMIZE: ricerca operativa combinatoria (pacchetto FFI gmbor).
' Prima slice: PACK (9) + ASSIGN (9) + COVER (14) + CP (3) = 35 funzioni.
'
' Convenzioni:
'   - matrici dense row-major; array out() dimensionati dal chiamante;
'   - output scalari singoli passati come array di 1 (es. nChosen(), uncovered(), flowValue());
'   - dove la firma C supera 8 argomenti, i parametri di servizio sono compattati o fissati
'     nello shim (documentato per funzione).
' Nomi specifici per non collidere con l'IMPORT wildcard di OPTIMIZE/GRAPH/BRAINS.

NAMESPACE OPTIMIZE

' ── PACK: bin packing / knapsack / cutting stock ─────────────────────────────
' algo: 0=NF, 1=FF, 2=BF, 3=WF, 4=FFD, 5=BFD; binOf() = n.
DECLARE FUNCTION BINPACK LIB "gmbor" ALIAS "ofx_bin_packing" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE, algo AS LONG, binOf() AS LONG) AS LONG
DECLARE FUNCTION BINPACKLB LIB "gmbor" ALIAS "ofx_bin_packing_lb" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE) AS LONG
' Knapsack frazionario: x() = n (0..1).
DECLARE FUNCTION KNAPSACKFRAC LIB "gmbor" ALIAS "ofx_knapsack_frac" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, x() AS DOUBLE) AS DOUBLE
' 0/1 knapsack (capacità intera): take() = n.
DECLARE FUNCTION KNAPSACK01 LIB "gmbor" ALIAS "ofx_knapsack_01" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, take() AS LONG) AS DOUBLE
DECLARE FUNCTION KNAPSACK01MITM LIB "gmbor" ALIAS "ofx_knapsack_01_mitm" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, take() AS LONG) AS DOUBLE
DECLARE FUNCTION KNAPSACKUNBOUNDED LIB "gmbor" ALIAS "ofx_knapsack_unbounded" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, counts() AS LONG) AS DOUBLE
DECLARE FUNCTION SUBSETSUM LIB "gmbor" ALIAS "ofx_subset_sum" (items() AS DOUBLE, n AS LONG, target AS DOUBLE, take() AS LONG) AS LONG
DECLARE FUNCTION CUTTINGSTOCK LIB "gmbor" ALIAS "ofx_cutting_stock" (pieces() AS DOUBLE, n AS LONG, stock AS DOUBLE, pattern() AS LONG) AS LONG
DECLARE FUNCTION BINPACK3D LIB "gmbor" ALIAS "ofx_bin_packing_3d" (items() AS DOUBLE, n AS LONG, W AS DOUBLE, H AS DOUBLE, D AS DOUBLE, binOf() AS LONG) AS LONG

' ── ASSIGN: assegnazione / matching / trasporto ──────────────────────────────
DECLARE FUNCTION HUNGARIAN LIB "gmbor" ALIAS "ofx_hungarian" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE
DECLARE FUNCTION AUCTION LIB "gmbor" ALIAS "ofx_auction" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG, eps AS DOUBLE) AS DOUBLE
DECLARE FUNCTION BOTTLENECK LIB "gmbor" ALIAS "ofx_bottleneck" (cost() AS DOUBLE, n AS LONG, assign() AS LONG) AS DOUBLE
' Gale-Shapley: prefMen()/prefWomen() = n*n; matchMen() = n; 1 se ok.
DECLARE FUNCTION STABLEMARRIAGE LIB "gmbor" ALIAS "ofx_stable_marriage" (prefMen() AS LONG, prefWomen() AS LONG, n AS LONG, matchMen() AS LONG) AS LONG
' Trasporto: supply(m), demand(n), cost(m*n), x(m*n) = piano.
DECLARE FUNCTION TRANSPORTVAM LIB "gmbor" ALIAS "ofx_transport_vam" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORTNW LIB "gmbor" ALIAS "ofx_transport_northwest" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORTLEASTCOST LIB "gmbor" ALIAS "ofx_transport_least_cost" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORTMODI LIB "gmbor" ALIAS "ofx_transport_modi" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE, maxit AS LONG) AS DOUBLE
' QAP tabu: A/B n*n, p() = n (permutazione), seed LONG.
DECLARE FUNCTION QAPTABU LIB "gmbor" ALIAS "ofx_qap_tabu" (A() AS DOUBLE, B() AS DOUBLE, n AS LONG, p() AS LONG, maxit AS LONG, seed AS LONG) AS DOUBLE

' ── COVER: set cover / facility location / p-median / k-means ────────────────
' elem() = m*n (elemento i in insieme j); chosen() = n; nChosen(0) = quanti.
DECLARE FUNCTION SETCOVER LIB "gmbor" ALIAS "ofx_set_cover" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
' chosen() booleano in ingresso; uncovered(0) in uscita.
DECLARE FUNCTION SETCOVERSOFT LIB "gmbor" ALIAS "ofx_set_cover_soft" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, pen AS DOUBLE, uncovered() AS LONG) AS DOUBLE
DECLARE FUNCTION SETCOVERPRICING LIB "gmbor" ALIAS "ofx_set_cover_pricing" (y() AS DOUBLE, m AS LONG, baseCost AS DOUBLE, costPerElem AS DOUBLE, chosenSet() AS LONG) AS DOUBLE
' Simulated annealing (t0=1.0, alpha=0.995 fissati nello shim).
DECLARE FUNCTION SETCOVERSA LIB "gmbor" ALIAS "ofx_set_cover_sa" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, pen AS DOUBLE, maxit AS LONG, seed AS LONG, chosen() AS LONG) AS DOUBLE
DECLARE FUNCTION SETCOVERLOCAL LIB "gmbor" ALIAS "ofx_set_cover_local" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG, maxit AS LONG) AS DOUBLE
' GRASP (seed fisso 12345 nello shim).
DECLARE FUNCTION SETCOVERGRASP LIB "gmbor" ALIAS "ofx_set_cover_grasp" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alpha AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
' Reactive GRASP: alphas() = [nalpha, a0, a1, ...] (nalpha compatto in testa; seed fisso).
DECLARE FUNCTION SETCOVERRGRASP LIB "gmbor" ALIAS "ofx_set_cover_rgrasp" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alphas() AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
DECLARE FUNCTION MAXCOVERAGE LIB "gmbor" ALIAS "ofx_max_coverage" (elem() AS LONG, m AS LONG, n AS LONG, k AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG
' p-median: dist(n*n); medians(p), assign(n).
DECLARE FUNCTION PMEDIAN LIB "gmbor" ALIAS "ofx_pmedian" (dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG, assign() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION PCENTER LIB "gmbor" ALIAS "ofx_pcenter" (dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG) AS DOUBLE
' Vertex cover: edges() = 2*ne; cover() = n.
DECLARE FUNCTION VERTEXCOVER LIB "gmbor" ALIAS "ofx_vertex_cover" (edges() AS LONG, n AS LONG, ne AS LONG, cover() AS LONG) AS LONG
DECLARE FUNCTION HITTINGSET LIB "gmbor" ALIAS "ofx_hitting_set" (elem() AS LONG, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG
DECLARE FUNCTION FACILITYLOCATION LIB "gmbor" ALIAS "ofx_facility_location" (openCost() AS DOUBLE, connCost() AS DOUBLE, m AS LONG, n AS LONG, open() AS LONG) AS DOUBLE
' k-means: points(n*dim), centroids(k*dim), labels(n); seed fisso nello shim.
DECLARE FUNCTION KMEANS LIB "gmbor" ALIAS "ofx_kmeans" (points() AS DOUBLE, n AS LONG, dim AS LONG, k AS LONG, centroids() AS DOUBLE, labels() AS LONG, maxit AS LONG) AS DOUBLE

' ── CP: SAT / all-different / CSP binario ────────────────────────────────────
' clauses() flat; clauseStart()/clauseLen() = nclauses; assignment() = nvars; 1 = SAT.
DECLARE FUNCTION SAT LIB "gmbor" ALIAS "ofx_sat" (clauses() AS LONG, clauseStart() AS LONG, clauseLen() AS LONG, nvars AS LONG, nclauses AS LONG, assignment() AS LONG) AS LONG
' dom() = n*D; sol() = n.
DECLARE FUNCTION ALLDIFF LIB "gmbor" ALIAS "ofx_alldiff" (dom() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG
DECLARE FUNCTION CPBINARY LIB "gmbor" ALIAS "ofx_cp_binary" (dom() AS LONG, allowed() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG
