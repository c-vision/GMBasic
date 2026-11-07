' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' ortools.inc — namespace OPTIMIZE: ricerca operativa combinatoria (pacchetto FFI gmbor).
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
DECLARE NATIVE FUNCTION BINPACKING LIB "gmbor" ALIAS "ofx_bin_packing__nat" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE, algo AS LONG, binOf() AS LONG) AS LONG
DECLARE NATIVE FUNCTION BINPACKINGLB LIB "gmbor" ALIAS "ofx_bin_packing_lb__nat" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE) AS LONG
' Knapsack frazionario: x() = n (0..1).
DECLARE NATIVE FUNCTION KNAPSACKFRACTIONAL LIB "gmbor" ALIAS "ofx_knapsack_frac__nat" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, x() AS DOUBLE) AS DOUBLE
' 0/1 knapsack (capacità intera): take() = n.
DECLARE NATIVE FUNCTION KNAPSACK01 LIB "gmbor" ALIAS "ofx_knapsack_01__nat" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, take() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION KNAPSACK01MITM LIB "gmbor" ALIAS "ofx_knapsack_01_mitm__nat" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, take() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION KNAPSACKUNBOUNDED LIB "gmbor" ALIAS "ofx_knapsack_unbounded__nat" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, counts() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION SUBSETSUM LIB "gmbor" ALIAS "ofx_subset_sum__nat" (items() AS DOUBLE, n AS LONG, target AS DOUBLE, take() AS LONG) AS LONG
DECLARE NATIVE FUNCTION CUTTINGSTOCK LIB "gmbor" ALIAS "ofx_cutting_stock__nat" (pieces() AS DOUBLE, n AS LONG, stock AS DOUBLE, pattern() AS LONG) AS LONG
DECLARE NATIVE FUNCTION BINPACKING3D LIB "gmbor" ALIAS "ofx_bin_packing_3d__nat" (items() AS DOUBLE, n AS LONG, W AS DOUBLE, H AS DOUBLE, D AS DOUBLE, binOf() AS LONG) AS LONG

' ── ASSIGN: assegnazione / matching / trasporto ──────────────────────────────
DECLARE NATIVE FUNCTION ASSIGNHUNGARIAN LIB "gmbor" ALIAS "ofx_hungarian__nat" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION ASSIGNAUCTION LIB "gmbor" ALIAS "ofx_auction__nat" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG, eps AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION ASSIGNBOTTLENECK LIB "gmbor" ALIAS "ofx_bottleneck__nat" (cost() AS DOUBLE, n AS LONG, assign() AS LONG) AS DOUBLE
' Gale-Shapley: prefMen()/prefWomen() = n*n; matchMen() = n; 1 se ok.
DECLARE NATIVE FUNCTION STABLEMARRIAGE LIB "gmbor" ALIAS "ofx_stable_marriage__nat" (prefMen() AS LONG, prefWomen() AS LONG, n AS LONG, matchMen() AS LONG) AS LONG
' Trasporto: supply(m), demand(n), cost(m*n), x(m*n) = piano.
DECLARE NATIVE FUNCTION TRANSPORTVAM LIB "gmbor" ALIAS "ofx_transport_vam__nat" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION TRANSPORTNORTHWEST LIB "gmbor" ALIAS "ofx_transport_northwest__nat" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION TRANSPORTLEASTCOST LIB "gmbor" ALIAS "ofx_transport_least_cost__nat" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION TRANSPORTMODI LIB "gmbor" ALIAS "ofx_transport_modi__nat" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE, maxit AS LONG) AS DOUBLE
' QAP tabu: A/B n*n, p() = n (permutazione), seed LONG.
DECLARE NATIVE FUNCTION ASSIGNQAPTABU LIB "gmbor" ALIAS "ofx_qap_tabu__nat" (A() AS DOUBLE, B() AS DOUBLE, n AS LONG, p() AS LONG, maxit AS LONG, seed AS LONG) AS DOUBLE

' ── COVER: set cover / facility location / p-median / k-means ────────────────
' elem() = m*n (elemento i in insieme j); chosen() = n; nChosen(0) = quanti.
DECLARE NATIVE FUNCTION SETCOVER LIB "gmbor" ALIAS "ofx_set_cover__nat" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
' chosen() booleano in ingresso; uncovered(0) in uscita.
DECLARE NATIVE FUNCTION SETCOVERSOFT LIB "gmbor" ALIAS "ofx_set_cover_soft__nat" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, pen AS DOUBLE, uncovered() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION SETCOVERPRICING LIB "gmbor" ALIAS "ofx_set_cover_pricing__nat" (y() AS DOUBLE, m AS LONG, baseCost AS DOUBLE, costPerElem AS DOUBLE, chosenSet() AS LONG) AS DOUBLE
' Simulated annealing (t0=1.0, alpha=0.995 fissati nello shim).
DECLARE NATIVE FUNCTION SETCOVERSA LIB "gmbor" ALIAS "ofx_set_cover_sa__nat" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, pen AS DOUBLE, maxit AS LONG, seed AS LONG, chosen() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION SETCOVERLOCAL LIB "gmbor" ALIAS "ofx_set_cover_local__nat" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG, maxit AS LONG) AS DOUBLE
' GRASP (seed fisso 12345 nello shim).
DECLARE NATIVE FUNCTION SETCOVERGRASP LIB "gmbor" ALIAS "ofx_set_cover_grasp__nat" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alpha AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
' Reactive GRASP: alphas() = [nalpha, a0, a1, ...] (nalpha compatto in testa; seed fisso).
DECLARE NATIVE FUNCTION SETCOVERRGRASP LIB "gmbor" ALIAS "ofx_set_cover_rgrasp__nat" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alphas() AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION MAXCOVERAGE LIB "gmbor" ALIAS "ofx_max_coverage__nat" (elem() AS LONG, m AS LONG, n AS LONG, k AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG
' p-median: dist(n*n); medians(p), assign(n).
DECLARE NATIVE FUNCTION PMEDIAN LIB "gmbor" ALIAS "ofx_pmedian__nat" (dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG, assign() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION PCENTER LIB "gmbor" ALIAS "ofx_pcenter__nat" (dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG) AS DOUBLE
' Vertex cover: edges() = 2*ne; cover() = n.
DECLARE NATIVE FUNCTION VERTEXCOVER LIB "gmbor" ALIAS "ofx_vertex_cover__nat" (edges() AS LONG, n AS LONG, ne AS LONG, cover() AS LONG) AS LONG
DECLARE NATIVE FUNCTION HITTINGSET LIB "gmbor" ALIAS "ofx_hitting_set__nat" (elem() AS LONG, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG
DECLARE NATIVE FUNCTION FACILITYLOCATION LIB "gmbor" ALIAS "ofx_facility_location__nat" (openCost() AS DOUBLE, connCost() AS DOUBLE, m AS LONG, n AS LONG, open() AS LONG) AS DOUBLE
' k-means: points(n*dim), centroids(k*dim), labels(n); seed fisso nello shim.
DECLARE NATIVE FUNCTION KMEANS LIB "gmbor" ALIAS "ofx_kmeans__nat" (points() AS DOUBLE, n AS LONG, dim AS LONG, k AS LONG, centroids() AS DOUBLE, labels() AS LONG, maxit AS LONG) AS DOUBLE

' ── CP: SATDPLL / all-different / CSP binario ────────────────────────────────────
' clauses() flat; clauseStart()/clauseLen() = nclauses; assignment() = nvars; 1 = SATDPLL.
DECLARE NATIVE FUNCTION SATDPLL LIB "gmbor" ALIAS "ofx_sat__nat" (clauses() AS LONG, clauseStart() AS LONG, clauseLen() AS LONG, nvars AS LONG, nclauses AS LONG, assignment() AS LONG) AS LONG
' dom() = n*D; sol() = n.
DECLARE NATIVE FUNCTION CPALLDIFF LIB "gmbor" ALIAS "ofx_alldiff__nat" (dom() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG
DECLARE NATIVE FUNCTION CPBINARY LIB "gmbor" ALIAS "ofx_cp_binary__nat" (dom() AS LONG, allowed() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG

' ── ROUTING: TSP / VRP / VRPPICKUPDELIVERY / arc routing (23) ──────────────────────────────
' Turistica: tour() = n (ordine di visita); dist() = n*n. Seed fissi dove la firma C
' supererebbe gli 8 argomenti FFI.
DECLARE NATIVE FUNCTION TSPNEAREST LIB "gmbor" ALIAS "ofx_tsp_nn__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSP2OPT LIB "gmbor" ALIAS "ofx_tsp_2opt__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPTOURCOST LIB "gmbor" ALIAS "ofx_tsp_tour_cost__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPEXTRAMILEAGE LIB "gmbor" ALIAS "ofx_tsp_extra_mileage__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
' GRASP: alpha=0.3 e seed fissi nello shim.
DECLARE NATIVE FUNCTION TSPGRASP LIB "gmbor" ALIAS "ofx_tsp_grasp__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPVNS LIB "gmbor" ALIAS "ofx_tsp_vns__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSP3OPT LIB "gmbor" ALIAS "ofx_tsp_3opt__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPSA LIB "gmbor" ALIAS "ofx_tsp_sa__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, T0 AS DOUBLE, cooling AS DOUBLE, iters AS LONG) AS DOUBLE
' tenure_kind: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth.
DECLARE NATIVE FUNCTION TSPTABU LIB "gmbor" ALIAS "ofx_tsp_tabu__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG, tenureKind AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPONETREE LIB "gmbor" ALIAS "ofx_tsp_one_tree__nat" (dist() AS DOUBLE, n AS LONG, root AS LONG, edges() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPHKBOUND LIB "gmbor" ALIAS "ofx_tsp_hk_bound__nat" (dist() AS DOUBLE, n AS LONG, maxit AS LONG, pi() AS DOUBLE) AS DOUBLE
' succ()/comp()/sizes() = n; ritorna il numero di sottotour.
DECLARE NATIVE FUNCTION TSPSUBTOURS LIB "gmbor" ALIAS "ofx_tsp_subtours__nat" (succ() AS LONG, n AS LONG, comp() AS LONG, sizes() AS LONG) AS LONG
DECLARE NATIVE FUNCTION TSPDFJ LIB "gmbor" ALIAS "ofx_tsp_dfj__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxcuts AS LONG) AS DOUBLE
' ERX: p1()/p2()/child() = n; ritorna 1.
DECLARE NATIVE FUNCTION TSPERX LIB "gmbor" ALIAS "ofx_tsp_erx__nat" (p1() AS LONG, p2() AS LONG, n AS LONG, child() AS LONG) AS LONG
DECLARE NATIVE FUNCTION TSPGA LIB "gmbor" ALIAS "ofx_tsp_ga__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, pop AS LONG, gens AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPOROPT LIB "gmbor" ALIAS "ofx_tsp_or_opt__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPDOUBLETREE LIB "gmbor" ALIAS "ofx_tsp_double_tree__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION TSPHELDKARP LIB "gmbor" ALIAS "ofx_tsp_held_karp__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE

' CVRP: dist (n+1)x(n+1) col deposito; demand() = n; routeOf() = n; total(0).
DECLARE NATIVE FUNCTION VRPCLARKEWRIGHT LIB "gmbor" ALIAS "ofx_vrp_clarke_wright__nat" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRPTIMEWINDOWS: tw() = ready[n] | due[n] | service[n] (11 parametri C compattati).
DECLARE NATIVE FUNCTION VRPTIMEWINDOWS LIB "gmbor" ALIAS "ofx_vrp_time_windows__nat" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, tw() AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRPMULTIDEPOT: depositi agli indici 0..ndepots-1.
DECLARE NATIVE FUNCTION VRPMULTIDEPOT LIB "gmbor" ALIAS "ofx_mdvrp__nat" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, ndepots AS LONG, cap AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRPPICKUPDELIVERY: pickup()/delivery() = nreq; route() = 2*nreq; total(0).
DECLARE NATIVE FUNCTION VRPPICKUPDELIVERY LIB "gmbor" ALIAS "ofx_pdp__nat" (dist() AS DOUBLE, demand() AS DOUBLE, nreq AS LONG, cap AS DOUBLE, pickup() AS LONG, delivery() AS LONG, route() AS LONG, total() AS DOUBLE) AS LONG
' Arc routing (Chinese postman): euler() = n*n+2; elen(0) = lunghezza.
DECLARE NATIVE FUNCTION ARCROUTING LIB "gmbor" ALIAS "ofx_arc_routing__nat" (w() AS DOUBLE, n AS LONG, euler() AS LONG, elen() AS LONG) AS DOUBLE

' ── SCHEDULING: macchina singola, flow shop, progetto, job shop (18) ─────────
DECLARE NATIVE FUNCTION SCHEDSPT LIB "gmbor" ALIAS "ofx_sched_spt__nat" (p() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION SCHEDWSPT LIB "gmbor" ALIAS "ofx_sched_wspt__nat" (p() AS DOUBLE, w() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION SCHEDEDD LIB "gmbor" ALIAS "ofx_sched_edd__nat" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
' Min #tardivi (Moore-Hodgson): ritorna il numero di job tardivi; order() = n.
DECLARE NATIVE FUNCTION SCHEDMOORE LIB "gmbor" ALIAS "ofx_sched_moore__nat" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS LONG
DECLARE NATIVE FUNCTION SCHEDLAWLER LIB "gmbor" ALIAS "ofx_sched_lawler__nat" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
' Intervalli: selected()/start()/end() = n.
DECLARE NATIVE FUNCTION INTERVALMAX LIB "gmbor" ALIAS "ofx_interval_max__nat" (start() AS DOUBLE, finish() AS DOUBLE, n AS LONG, selected() AS LONG) AS LONG
DECLARE NATIVE FUNCTION INTERVALPARTITION LIB "gmbor" ALIAS "ofx_interval_partition__nat" (start() AS DOUBLE, finish() AS DOUBLE, n AS LONG) AS LONG
' Flow shop: seq() = n.
DECLARE NATIVE FUNCTION FLOWSHOPJOHNSON LIB "gmbor" ALIAS "ofx_johnson__nat" (p1() AS DOUBLE, p2() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE
' FLOWSHOPNEH: p() = n*m (job i, macchina k = p(i*m+k)).
DECLARE NATIVE FUNCTION FLOWSHOPNEH LIB "gmbor" ALIAS "ofx_neh__nat" (p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FLOWSHOPJOHNSON3 LIB "gmbor" ALIAS "ofx_johnson3__nat" (p1() AS DOUBLE, p2() AS DOUBLE, p3() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FLOWSHOPCDS LIB "gmbor" ALIAS "ofx_cds__nat" (p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION PARALLELLPT LIB "gmbor" ALIAS "ofx_lpt__nat" (p() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE
' PROJECTCPM: preds() = n*npred (predecessori, -1 padding); est()/lst()/slack() = n.
DECLARE NATIVE FUNCTION PROJECTCPM LIB "gmbor" ALIAS "ofx_cpm__nat" (dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, lst() AS DOUBLE, slack() AS DOUBLE) AS DOUBLE
' PROJECTPERT: to()/tm()/tp() = n; est()/variance() = n.
DECLARE NATIVE FUNCTION PROJECTPERT LIB "gmbor" ALIAS "ofx_pert__nat" (to() AS DOUBLE, tm() AS DOUBLE, tp() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, variance() AS DOUBLE) AS DOUBLE
' PROJECTRCPSP: demand() = n*nr; capacity() = nr; start() = n.
DECLARE NATIVE FUNCTION PROJECTRCPSP LIB "gmbor" ALIAS "ofx_rcpsp__nat" (dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, demand() AS DOUBLE, capacity() AS DOUBLE, nr AS LONG, start() AS LONG) AS DOUBLE
' Job shop: route()/dur()/start() = njobs*nmach.
DECLARE NATIVE FUNCTION JOBSHOP LIB "gmbor" ALIAS "ofx_job_shop__nat" (route() AS LONG, dur() AS DOUBLE, njobs AS LONG, nmach AS LONG, start() AS LONG) AS DOUBLE
' Timetabling: conflicts() = n*n; slotOf() = n.
DECLARE NATIVE FUNCTION TIMETABLING LIB "gmbor" ALIAS "ofx_timetabling__nat" (conflicts() AS LONG, n AS LONG, slotOf() AS LONG) AS LONG
' Rostering: demand() = ndays; schedule() = nnurses*ndays; ritorna i turni non coperti.
DECLARE NATIVE FUNCTION ROSTERING LIB "gmbor" ALIAS "ofx_rostering__nat" (demand() AS LONG, nnurses AS LONG, ndays AS LONG, maxWork AS LONG, schedule() AS LONG) AS LONG

' ── META: meta-euristiche e ottimizzazione globale (12) ─────────────────────
' Callback vettoriale `f(x() AS DOUBLE) AS DOUBLE`; x() = d valori (in/out).
' Seed fissi dove la firma C supera gli 8 argomenti FFI.
DECLARE NATIVE FUNCTION METAHILLCLIMB LIB "gmbor" ALIAS "ofx_meta_hill_climb__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION METASA LIB "gmbor" ALIAS "ofx_meta_sa__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG, T0 AS DOUBLE, alpha AS DOUBLE) AS DOUBLE
' tenure: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth.
DECLARE NATIVE FUNCTION METATABU LIB "gmbor" ALIAS "ofx_meta_tabu__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG, tenure AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION METAGA LIB "gmbor" ALIAS "ofx_meta_ga__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION METAPSO LIB "gmbor" ALIAS "ofx_meta_pso__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, swarm AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION METADE LIB "gmbor" ALIAS "ofx_meta_de__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION METACMAES LIB "gmbor" ALIAS "ofx_meta_cmaes__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG) AS DOUBLE
' ES/ARS: nessun box (perturbazioni gaussiane attorno a x).
DECLARE NATIVE FUNCTION METAES LIB "gmbor" ALIAS "ofx_meta_es__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, sigma AS DOUBLE, alpha AS DOUBLE, pop AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION METAARS LIB "gmbor" ALIAS "ofx_meta_ars__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, sigma AS DOUBLE, alpha AS DOUBLE, topB AS LONG, pop AS LONG, maxit AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION METAACOTSP LIB "gmbor" ALIAS "ofx_meta_aco_tsp__nat" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, ants AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TABUTENURE LIB "gmbor" ALIAS "ofx_tabu_tenure" (kind AS LONG, it AS LONG, n AS LONG) AS LONG
' NSGA-II (2 obiettivi): out() = pareto[pop*d] | fv1[pop] | fv2[pop].
DECLARE NATIVE FUNCTION METANSGA2 LIB "gmbor" ALIAS "ofx_meta_nsga2__nat" (f1 AS FUNCTION(x() AS DOUBLE) AS DOUBLE, f2 AS FUNCTION(x() AS DOUBLE) AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG, out() AS DOUBLE) AS LONG

' ── OR-UTIL: code, inventario, MCDM, MDP, simulazione (15) ──────────────────
' out() = L, Lq, W, Wq; ritorna rho.
DECLARE FUNCTION QUEUEMM1 LIB "gmbor" ALIAS "ofx_queue_mm1" (lambda AS DOUBLE, mu AS DOUBLE, out() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION QUEUEMMC LIB "gmbor" ALIAS "ofx_queue_mmc__nat" (lambda AS DOUBLE, mu AS DOUBLE, c AS LONG, out() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LITTLESLAW LIB "gmbor" ALIAS "ofx_littles_law" (lambda AS DOUBLE, W AS DOUBLE) AS DOUBLE
DECLARE FUNCTION EOQ LIB "gmbor" ALIAS "ofx_eoq" (demand AS DOUBLE, setup AS DOUBLE, holding AS DOUBLE) AS DOUBLE
DECLARE FUNCTION EPQ LIB "gmbor" ALIAS "ofx_epq" (demand AS DOUBLE, setup AS DOUBLE, holding AS DOUBLE, rate AS DOUBLE) AS DOUBLE
DECLARE FUNCTION NEWSVENDOR LIB "gmbor" ALIAS "ofx_newsvendor" (cu AS DOUBLE, co AS DOUBLE, mean AS DOUBLE, std AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SAFETYSTOCK LIB "gmbor" ALIAS "ofx_safety_stock" (z AS DOUBLE, sigma AS DOUBLE) AS DOUBLE
' AHP: weights() = n; consistency(0) = rapporto di consistenza.
DECLARE NATIVE FUNCTION AHP LIB "gmbor" ALIAS "ofx_ahp__nat" (pairwise() AS DOUBLE, n AS LONG, weights() AS DOUBLE, consistency() AS DOUBLE) AS LONG
' TOPSIS: X() = m*n; benefit() = n; scores() = m.
DECLARE NATIVE FUNCTION TOPSIS LIB "gmbor" ALIAS "ofx_topsis__nat" (X() AS DOUBLE, m AS LONG, n AS LONG, w() AS DOUBLE, benefit() AS LONG, scores() AS DOUBLE) AS LONG
' MDP: out() = V[ns] | policy[ns] (policy come double); ritorna le iterazioni.
DECLARE NATIVE FUNCTION MDPVALUE LIB "gmbor" ALIAS "ofx_mdp_value__nat" (P() AS DOUBLE, R() AS DOUBLE, ns AS LONG, na AS LONG, gamma AS DOUBLE, maxit AS LONG, out() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION MDPPOLICY LIB "gmbor" ALIAS "ofx_mdp_policy__nat" (P() AS DOUBLE, R() AS DOUBLE, ns AS LONG, na AS LONG, gamma AS DOUBLE, maxit AS LONG, out() AS DOUBLE) AS LONG
' Monte Carlo: out(0) = deviazione standard.
DECLARE NATIVE FUNCTION MONTECARLO LIB "gmbor" ALIAS "ofx_montecarlo__nat" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS DOUBLE
' CVaR: out(0) = VaR.
DECLARE NATIVE FUNCTION CVAR LIB "gmbor" ALIAS "ofx_cvar__nat" (returns() AS DOUBLE, n AS LONG, alpha AS DOUBLE, out() AS DOUBLE) AS DOUBLE
' DES M/M/1: out(0) = utilizzazione.
DECLARE NATIVE FUNCTION DESMM1 LIB "gmbor" ALIAS "ofx_des_mm1__nat" (lambda AS DOUBLE, mu AS DOUBLE, ncust AS LONG, out() AS DOUBLE) AS DOUBLE
' Chance constraint: out(0) = z.
DECLARE NATIVE FUNCTION CHANCECONSTRAINT LIB "gmbor" ALIAS "ofx_chance_constraint__nat" (sigma() AS DOUBLE, x() AS DOUBLE, n AS LONG, alpha AS DOUBLE, out() AS DOUBLE) AS DOUBLE

' ── DP: programmazione dinamica classica (3) ────────────────────────────────
' Optimal BST: p() = n; root() = n*n (-1 se vuoto).
DECLARE NATIVE FUNCTION DPOPTIMALBST LIB "gmbor" ALIAS "ofx_dp_optimal_bst__nat" (p() AS DOUBLE, n AS LONG, root() AS LONG) AS DOUBLE
' Equipment replacement: opCost()/resale() = maxAge; schedule() = years.
DECLARE NATIVE FUNCTION DPEQUIPMENT LIB "gmbor" ALIAS "ofx_dp_equipment__nat" (years AS LONG, buyCost AS DOUBLE, maxAge AS LONG, opCost() AS DOUBLE, resale() AS DOUBLE, schedule() AS LONG) AS DOUBLE
' Sports series: isHome() = nGames.
DECLARE NATIVE FUNCTION DPSPORTSSERIES LIB "gmbor" ALIAS "ofx_dp_sports__nat" (nGames AS LONG, needed AS LONG, pHome AS DOUBLE, pAway AS DOUBLE, isHome() AS LONG) AS DOUBLE
