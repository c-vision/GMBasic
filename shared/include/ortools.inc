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
DECLARE FUNCTION BINPACKING LIB "gmbor" ALIAS "ofx_bin_packing" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE, algo AS LONG, binOf() AS LONG) AS LONG
DECLARE FUNCTION BINPACKINGLB LIB "gmbor" ALIAS "ofx_bin_packing_lb" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE) AS LONG
' Knapsack frazionario: x() = n (0..1).
DECLARE FUNCTION KNAPSACKFRACTIONAL LIB "gmbor" ALIAS "ofx_knapsack_frac" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, x() AS DOUBLE) AS DOUBLE
' 0/1 knapsack (capacità intera): take() = n.
DECLARE FUNCTION KNAPSACK01 LIB "gmbor" ALIAS "ofx_knapsack_01" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, take() AS LONG) AS DOUBLE
DECLARE FUNCTION KNAPSACK01MITM LIB "gmbor" ALIAS "ofx_knapsack_01_mitm" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, take() AS LONG) AS DOUBLE
DECLARE FUNCTION KNAPSACKUNBOUNDED LIB "gmbor" ALIAS "ofx_knapsack_unbounded" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, counts() AS LONG) AS DOUBLE
DECLARE FUNCTION SUBSETSUM LIB "gmbor" ALIAS "ofx_subset_sum" (items() AS DOUBLE, n AS LONG, target AS DOUBLE, take() AS LONG) AS LONG
DECLARE FUNCTION CUTTINGSTOCK LIB "gmbor" ALIAS "ofx_cutting_stock" (pieces() AS DOUBLE, n AS LONG, stock AS DOUBLE, pattern() AS LONG) AS LONG
DECLARE FUNCTION BINPACKING3D LIB "gmbor" ALIAS "ofx_bin_packing_3d" (items() AS DOUBLE, n AS LONG, W AS DOUBLE, H AS DOUBLE, D AS DOUBLE, binOf() AS LONG) AS LONG

' ── ASSIGN: assegnazione / matching / trasporto ──────────────────────────────
DECLARE FUNCTION ASSIGNHUNGARIAN LIB "gmbor" ALIAS "ofx_hungarian" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE
DECLARE FUNCTION ASSIGNAUCTION LIB "gmbor" ALIAS "ofx_auction" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG, eps AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ASSIGNBOTTLENECK LIB "gmbor" ALIAS "ofx_bottleneck" (cost() AS DOUBLE, n AS LONG, assign() AS LONG) AS DOUBLE
' Gale-Shapley: prefMen()/prefWomen() = n*n; matchMen() = n; 1 se ok.
DECLARE FUNCTION STABLEMARRIAGE LIB "gmbor" ALIAS "ofx_stable_marriage" (prefMen() AS LONG, prefWomen() AS LONG, n AS LONG, matchMen() AS LONG) AS LONG
' Trasporto: supply(m), demand(n), cost(m*n), x(m*n) = piano.
DECLARE FUNCTION TRANSPORTVAM LIB "gmbor" ALIAS "ofx_transport_vam" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORTNORTHWEST LIB "gmbor" ALIAS "ofx_transport_northwest" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORTLEASTCOST LIB "gmbor" ALIAS "ofx_transport_least_cost" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORTMODI LIB "gmbor" ALIAS "ofx_transport_modi" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE, maxit AS LONG) AS DOUBLE
' QAP tabu: A/B n*n, p() = n (permutazione), seed LONG.
DECLARE FUNCTION ASSIGNQAPTABU LIB "gmbor" ALIAS "ofx_qap_tabu" (A() AS DOUBLE, B() AS DOUBLE, n AS LONG, p() AS LONG, maxit AS LONG, seed AS LONG) AS DOUBLE

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

' ── CP: SATDPLL / all-different / CSP binario ────────────────────────────────────
' clauses() flat; clauseStart()/clauseLen() = nclauses; assignment() = nvars; 1 = SATDPLL.
DECLARE FUNCTION SATDPLL LIB "gmbor" ALIAS "ofx_sat" (clauses() AS LONG, clauseStart() AS LONG, clauseLen() AS LONG, nvars AS LONG, nclauses AS LONG, assignment() AS LONG) AS LONG
' dom() = n*D; sol() = n.
DECLARE FUNCTION CPALLDIFF LIB "gmbor" ALIAS "ofx_alldiff" (dom() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG
DECLARE FUNCTION CPBINARY LIB "gmbor" ALIAS "ofx_cp_binary" (dom() AS LONG, allowed() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG

' ── ROUTING: TSP / VRP / VRPPICKUPDELIVERY / arc routing (23) ──────────────────────────────
' Turistica: tour() = n (ordine di visita); dist() = n*n. Seed fissi dove la firma C
' supererebbe gli 8 argomenti FFI.
DECLARE FUNCTION TSPNEAREST LIB "gmbor" ALIAS "ofx_tsp_nn" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE FUNCTION TSP2OPT LIB "gmbor" ALIAS "ofx_tsp_2opt" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSPTOURCOST LIB "gmbor" ALIAS "ofx_tsp_tour_cost" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE FUNCTION TSPEXTRAMILEAGE LIB "gmbor" ALIAS "ofx_tsp_extra_mileage" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
' GRASP: alpha=0.3 e seed fissi nello shim.
DECLARE FUNCTION TSPGRASP LIB "gmbor" ALIAS "ofx_tsp_grasp" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSPVNS LIB "gmbor" ALIAS "ofx_tsp_vns" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSP3OPT LIB "gmbor" ALIAS "ofx_tsp_3opt" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSPSA LIB "gmbor" ALIAS "ofx_tsp_sa" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, T0 AS DOUBLE, cooling AS DOUBLE, iters AS LONG) AS DOUBLE
' tenure_kind: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth.
DECLARE FUNCTION TSPTABU LIB "gmbor" ALIAS "ofx_tsp_tabu" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG, tenureKind AS LONG) AS DOUBLE
DECLARE FUNCTION TSPONETREE LIB "gmbor" ALIAS "ofx_tsp_one_tree" (dist() AS DOUBLE, n AS LONG, root AS LONG, edges() AS LONG) AS DOUBLE
DECLARE FUNCTION TSPHKBOUND LIB "gmbor" ALIAS "ofx_tsp_hk_bound" (dist() AS DOUBLE, n AS LONG, maxit AS LONG, pi() AS DOUBLE) AS DOUBLE
' succ()/comp()/sizes() = n; ritorna il numero di sottotour.
DECLARE FUNCTION TSPSUBTOURS LIB "gmbor" ALIAS "ofx_tsp_subtours" (succ() AS LONG, n AS LONG, comp() AS LONG, sizes() AS LONG) AS LONG
DECLARE FUNCTION TSPDFJ LIB "gmbor" ALIAS "ofx_tsp_dfj" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxcuts AS LONG) AS DOUBLE
' ERX: p1()/p2()/child() = n; ritorna 1.
DECLARE FUNCTION TSPERX LIB "gmbor" ALIAS "ofx_tsp_erx" (p1() AS LONG, p2() AS LONG, n AS LONG, child() AS LONG) AS LONG
DECLARE FUNCTION TSPGA LIB "gmbor" ALIAS "ofx_tsp_ga" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, pop AS LONG, gens AS LONG) AS DOUBLE
DECLARE FUNCTION TSPOROPT LIB "gmbor" ALIAS "ofx_tsp_or_opt" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSPDOUBLETREE LIB "gmbor" ALIAS "ofx_tsp_double_tree" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE FUNCTION TSPHELDKARP LIB "gmbor" ALIAS "ofx_tsp_held_karp" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE

' CVRP: dist (n+1)x(n+1) col deposito; demand() = n; routeOf() = n; total(0).
DECLARE FUNCTION VRPCLARKEWRIGHT LIB "gmbor" ALIAS "ofx_vrp_clarke_wright" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRPTIMEWINDOWS: tw() = ready[n] | due[n] | service[n] (11 parametri C compattati).
DECLARE FUNCTION VRPTIMEWINDOWS LIB "gmbor" ALIAS "ofx_vrp_time_windows" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, tw() AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRPMULTIDEPOT: depositi agli indici 0..ndepots-1.
DECLARE FUNCTION VRPMULTIDEPOT LIB "gmbor" ALIAS "ofx_mdvrp" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, ndepots AS LONG, cap AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRPPICKUPDELIVERY: pickup()/delivery() = nreq; route() = 2*nreq; total(0).
DECLARE FUNCTION VRPPICKUPDELIVERY LIB "gmbor" ALIAS "ofx_pdp" (dist() AS DOUBLE, demand() AS DOUBLE, nreq AS LONG, cap AS DOUBLE, pickup() AS LONG, delivery() AS LONG, route() AS LONG, total() AS DOUBLE) AS LONG
' Arc routing (Chinese postman): euler() = n*n+2; elen(0) = lunghezza.
DECLARE FUNCTION ARCROUTING LIB "gmbor" ALIAS "ofx_arc_routing" (w() AS DOUBLE, n AS LONG, euler() AS LONG, elen() AS LONG) AS DOUBLE

' ── SCHEDULING: macchina singola, flow shop, progetto, job shop (18) ─────────
DECLARE FUNCTION SCHEDSPT LIB "gmbor" ALIAS "ofx_sched_spt" (p() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
DECLARE FUNCTION SCHEDWSPT LIB "gmbor" ALIAS "ofx_sched_wspt" (p() AS DOUBLE, w() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
DECLARE FUNCTION SCHEDEDD LIB "gmbor" ALIAS "ofx_sched_edd" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
' Min #tardivi (Moore-Hodgson): ritorna il numero di job tardivi; order() = n.
DECLARE FUNCTION SCHEDMOORE LIB "gmbor" ALIAS "ofx_sched_moore" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS LONG
DECLARE FUNCTION SCHEDLAWLER LIB "gmbor" ALIAS "ofx_sched_lawler" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
' Intervalli: selected()/start()/end() = n.
DECLARE FUNCTION INTERVALMAX LIB "gmbor" ALIAS "ofx_interval_max" (start() AS DOUBLE, finish() AS DOUBLE, n AS LONG, selected() AS LONG) AS LONG
DECLARE FUNCTION INTERVALPARTITION LIB "gmbor" ALIAS "ofx_interval_partition" (start() AS DOUBLE, finish() AS DOUBLE, n AS LONG) AS LONG
' Flow shop: seq() = n.
DECLARE FUNCTION FLOWSHOPJOHNSON LIB "gmbor" ALIAS "ofx_johnson" (p1() AS DOUBLE, p2() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE
' FLOWSHOPNEH: p() = n*m (job i, macchina k = p(i*m+k)).
DECLARE FUNCTION FLOWSHOPNEH LIB "gmbor" ALIAS "ofx_neh" (p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE
DECLARE FUNCTION FLOWSHOPJOHNSON3 LIB "gmbor" ALIAS "ofx_johnson3" (p1() AS DOUBLE, p2() AS DOUBLE, p3() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE
DECLARE FUNCTION FLOWSHOPCDS LIB "gmbor" ALIAS "ofx_cds" (p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE
DECLARE FUNCTION PARALLELLPT LIB "gmbor" ALIAS "ofx_lpt" (p() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE
' PROJECTCPM: preds() = n*npred (predecessori, -1 padding); est()/lst()/slack() = n.
DECLARE FUNCTION PROJECTCPM LIB "gmbor" ALIAS "ofx_cpm" (dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, lst() AS DOUBLE, slack() AS DOUBLE) AS DOUBLE
' PROJECTPERT: to()/tm()/tp() = n; est()/variance() = n.
DECLARE FUNCTION PROJECTPERT LIB "gmbor" ALIAS "ofx_pert" (to() AS DOUBLE, tm() AS DOUBLE, tp() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, variance() AS DOUBLE) AS DOUBLE
' PROJECTRCPSP: demand() = n*nr; capacity() = nr; start() = n.
DECLARE FUNCTION PROJECTRCPSP LIB "gmbor" ALIAS "ofx_rcpsp" (dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, demand() AS DOUBLE, capacity() AS DOUBLE, nr AS LONG, start() AS LONG) AS DOUBLE
' Job shop: route()/dur()/start() = njobs*nmach.
DECLARE FUNCTION JOBSHOP LIB "gmbor" ALIAS "ofx_job_shop" (route() AS LONG, dur() AS DOUBLE, njobs AS LONG, nmach AS LONG, start() AS LONG) AS DOUBLE
' Timetabling: conflicts() = n*n; slotOf() = n.
DECLARE FUNCTION TIMETABLING LIB "gmbor" ALIAS "ofx_timetabling" (conflicts() AS LONG, n AS LONG, slotOf() AS LONG) AS LONG
' Rostering: demand() = ndays; schedule() = nnurses*ndays; ritorna i turni non coperti.
DECLARE FUNCTION ROSTERING LIB "gmbor" ALIAS "ofx_rostering" (demand() AS LONG, nnurses AS LONG, ndays AS LONG, maxWork AS LONG, schedule() AS LONG) AS LONG

' ── META: meta-euristiche e ottimizzazione globale (12) ─────────────────────
' Callback vettoriale `f(x() AS DOUBLE) AS DOUBLE`; x() = d valori (in/out).
' Seed fissi dove la firma C supera gli 8 argomenti FFI.
DECLARE FUNCTION METAHILLCLIMB LIB "gmbor" ALIAS "ofx_meta_hill_climb" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION METASA LIB "gmbor" ALIAS "ofx_meta_sa" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG, T0 AS DOUBLE, alpha AS DOUBLE) AS DOUBLE
' tenure: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth.
DECLARE FUNCTION METATABU LIB "gmbor" ALIAS "ofx_meta_tabu" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG, tenure AS LONG) AS DOUBLE
DECLARE FUNCTION METAGA LIB "gmbor" ALIAS "ofx_meta_ga" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG) AS DOUBLE
DECLARE FUNCTION METAPSO LIB "gmbor" ALIAS "ofx_meta_pso" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, swarm AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION METADE LIB "gmbor" ALIAS "ofx_meta_de" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG) AS DOUBLE
DECLARE FUNCTION METACMAES LIB "gmbor" ALIAS "ofx_meta_cmaes" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG) AS DOUBLE
' ES/ARS: nessun box (perturbazioni gaussiane attorno a x).
DECLARE FUNCTION METAES LIB "gmbor" ALIAS "ofx_meta_es" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, sigma AS DOUBLE, alpha AS DOUBLE, pop AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION METAARS LIB "gmbor" ALIAS "ofx_meta_ars" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, sigma AS DOUBLE, alpha AS DOUBLE, topB AS LONG, pop AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION METAACOTSP LIB "gmbor" ALIAS "ofx_meta_aco_tsp" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, ants AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TABUTENURE LIB "gmbor" ALIAS "ofx_tabu_tenure" (kind AS LONG, it AS LONG, n AS LONG) AS LONG
' NSGA-II (2 obiettivi): out() = pareto[pop*d] | fv1[pop] | fv2[pop].
DECLARE FUNCTION METANSGA2 LIB "gmbor" ALIAS "ofx_meta_nsga2" (f1 AS FUNCTION(x() AS DOUBLE) AS DOUBLE, f2 AS FUNCTION(x() AS DOUBLE) AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG, out() AS DOUBLE) AS LONG

' ── OR-UTIL: code, inventario, MCDM, MDP, simulazione (15) ──────────────────
' out() = L, Lq, W, Wq; ritorna rho.
DECLARE FUNCTION QUEUEMM1 LIB "gmbor" ALIAS "ofx_queue_mm1" (lambda AS DOUBLE, mu AS DOUBLE, out() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION QUEUEMMC LIB "gmbor" ALIAS "ofx_queue_mmc" (lambda AS DOUBLE, mu AS DOUBLE, c AS LONG, out() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION LITTLESLAW LIB "gmbor" ALIAS "ofx_littles_law" (lambda AS DOUBLE, W AS DOUBLE) AS DOUBLE
DECLARE FUNCTION EOQ LIB "gmbor" ALIAS "ofx_eoq" (demand AS DOUBLE, setup AS DOUBLE, holding AS DOUBLE) AS DOUBLE
DECLARE FUNCTION EPQ LIB "gmbor" ALIAS "ofx_epq" (demand AS DOUBLE, setup AS DOUBLE, holding AS DOUBLE, rate AS DOUBLE) AS DOUBLE
DECLARE FUNCTION NEWSVENDOR LIB "gmbor" ALIAS "ofx_newsvendor" (cu AS DOUBLE, co AS DOUBLE, mean AS DOUBLE, std AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SAFETYSTOCK LIB "gmbor" ALIAS "ofx_safety_stock" (z AS DOUBLE, sigma AS DOUBLE) AS DOUBLE
' AHP: weights() = n; consistency(0) = rapporto di consistenza.
DECLARE FUNCTION AHP LIB "gmbor" ALIAS "ofx_ahp" (pairwise() AS DOUBLE, n AS LONG, weights() AS DOUBLE, consistency() AS DOUBLE) AS LONG
' TOPSIS: X() = m*n; benefit() = n; scores() = m.
DECLARE FUNCTION TOPSIS LIB "gmbor" ALIAS "ofx_topsis" (X() AS DOUBLE, m AS LONG, n AS LONG, w() AS DOUBLE, benefit() AS LONG, scores() AS DOUBLE) AS LONG
' MDP: out() = V[ns] | policy[ns] (policy come double); ritorna le iterazioni.
DECLARE FUNCTION MDPVALUE LIB "gmbor" ALIAS "ofx_mdp_value" (P() AS DOUBLE, R() AS DOUBLE, ns AS LONG, na AS LONG, gamma AS DOUBLE, maxit AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MDPPOLICY LIB "gmbor" ALIAS "ofx_mdp_policy" (P() AS DOUBLE, R() AS DOUBLE, ns AS LONG, na AS LONG, gamma AS DOUBLE, maxit AS LONG, out() AS DOUBLE) AS LONG
' Monte Carlo: out(0) = deviazione standard.
DECLARE FUNCTION MONTECARLO LIB "gmbor" ALIAS "ofx_montecarlo" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS DOUBLE
' CVaR: out(0) = VaR.
DECLARE FUNCTION CVAR LIB "gmbor" ALIAS "ofx_cvar" (returns() AS DOUBLE, n AS LONG, alpha AS DOUBLE, out() AS DOUBLE) AS DOUBLE
' DES M/M/1: out(0) = utilizzazione.
DECLARE FUNCTION DESMM1 LIB "gmbor" ALIAS "ofx_des_mm1" (lambda AS DOUBLE, mu AS DOUBLE, ncust AS LONG, out() AS DOUBLE) AS DOUBLE
' Chance constraint: out(0) = z.
DECLARE FUNCTION CHANCECONSTRAINT LIB "gmbor" ALIAS "ofx_chance_constraint" (sigma() AS DOUBLE, x() AS DOUBLE, n AS LONG, alpha AS DOUBLE, out() AS DOUBLE) AS DOUBLE

' ── DP: programmazione dinamica classica (3) ────────────────────────────────
' Optimal BST: p() = n; root() = n*n (-1 se vuoto).
DECLARE FUNCTION DPOPTIMALBST LIB "gmbor" ALIAS "ofx_dp_optimal_bst" (p() AS DOUBLE, n AS LONG, root() AS LONG) AS DOUBLE
' Equipment replacement: opCost()/resale() = maxAge; schedule() = years.
DECLARE FUNCTION DPEQUIPMENT LIB "gmbor" ALIAS "ofx_dp_equipment" (years AS LONG, buyCost AS DOUBLE, maxAge AS LONG, opCost() AS DOUBLE, resale() AS DOUBLE, schedule() AS LONG) AS LONG
' Sports series: isHome() = nGames.
DECLARE FUNCTION DPSPORTSSERIES LIB "gmbor" ALIAS "ofx_dp_sports" (nGames AS LONG, needed AS LONG, pHome AS DOUBLE, pAway AS DOUBLE, isHome() AS LONG) AS DOUBLE
