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
DECLARE FUNCTION BIN_PACKING LIB "gmbor" ALIAS "ofx_bin_packing" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE, algo AS LONG, binOf() AS LONG) AS LONG
DECLARE FUNCTION BIN_PACKING_LB LIB "gmbor" ALIAS "ofx_bin_packing_lb" (items() AS DOUBLE, n AS LONG, cap AS DOUBLE) AS LONG
' Knapsack frazionario: x() = n (0..1).
DECLARE FUNCTION KNAPSACK_FRACTIONAL LIB "gmbor" ALIAS "ofx_knapsack_frac" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, x() AS DOUBLE) AS DOUBLE
' 0/1 knapsack (capacità intera): take() = n.
DECLARE FUNCTION KNAPSACK_01 LIB "gmbor" ALIAS "ofx_knapsack_01" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, take() AS LONG) AS DOUBLE
DECLARE FUNCTION KNAPSACK_01_MITM LIB "gmbor" ALIAS "ofx_knapsack_01_mitm" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, take() AS LONG) AS DOUBLE
DECLARE FUNCTION KNAPSACK_UNBOUNDED LIB "gmbor" ALIAS "ofx_knapsack_unbounded" (w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, counts() AS LONG) AS DOUBLE
DECLARE FUNCTION SUBSET_SUM LIB "gmbor" ALIAS "ofx_subset_sum" (items() AS DOUBLE, n AS LONG, target AS DOUBLE, take() AS LONG) AS LONG
DECLARE FUNCTION CUTTING_STOCK LIB "gmbor" ALIAS "ofx_cutting_stock" (pieces() AS DOUBLE, n AS LONG, stock AS DOUBLE, pattern() AS LONG) AS LONG
DECLARE FUNCTION BIN_PACKING_3D LIB "gmbor" ALIAS "ofx_bin_packing_3d" (items() AS DOUBLE, n AS LONG, W AS DOUBLE, H AS DOUBLE, D AS DOUBLE, binOf() AS LONG) AS LONG

' ── ASSIGN: assegnazione / matching / trasporto ──────────────────────────────
DECLARE FUNCTION ASSIGN_HUNGARIAN LIB "gmbor" ALIAS "ofx_hungarian" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE
DECLARE FUNCTION ASSIGN_AUCTION LIB "gmbor" ALIAS "ofx_auction" (cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG, eps AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ASSIGN_BOTTLENECK LIB "gmbor" ALIAS "ofx_bottleneck" (cost() AS DOUBLE, n AS LONG, assign() AS LONG) AS DOUBLE
' Gale-Shapley: prefMen()/prefWomen() = n*n; matchMen() = n; 1 se ok.
DECLARE FUNCTION STABLE_MARRIAGE LIB "gmbor" ALIAS "ofx_stable_marriage" (prefMen() AS LONG, prefWomen() AS LONG, n AS LONG, matchMen() AS LONG) AS LONG
' Trasporto: supply(m), demand(n), cost(m*n), x(m*n) = piano.
DECLARE FUNCTION TRANSPORT_VAM LIB "gmbor" ALIAS "ofx_transport_vam" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORT_NORTHWEST LIB "gmbor" ALIAS "ofx_transport_northwest" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORT_LEAST_COST LIB "gmbor" ALIAS "ofx_transport_least_cost" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TRANSPORT_MODI LIB "gmbor" ALIAS "ofx_transport_modi" (supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE, maxit AS LONG) AS DOUBLE
' QAP tabu: A/B n*n, p() = n (permutazione), seed LONG.
DECLARE FUNCTION ASSIGN_QAP_TABU LIB "gmbor" ALIAS "ofx_qap_tabu" (A() AS DOUBLE, B() AS DOUBLE, n AS LONG, p() AS LONG, maxit AS LONG, seed AS LONG) AS DOUBLE

' ── COVER: set cover / facility location / p-median / k-means ────────────────
' elem() = m*n (elemento i in insieme j); chosen() = n; nChosen(0) = quanti.
DECLARE FUNCTION SET_COVER LIB "gmbor" ALIAS "ofx_set_cover" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
' chosen() booleano in ingresso; uncovered(0) in uscita.
DECLARE FUNCTION SET_COVER_SOFT LIB "gmbor" ALIAS "ofx_set_cover_soft" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, pen AS DOUBLE, uncovered() AS LONG) AS DOUBLE
DECLARE FUNCTION SET_COVER_PRICING LIB "gmbor" ALIAS "ofx_set_cover_pricing" (y() AS DOUBLE, m AS LONG, baseCost AS DOUBLE, costPerElem AS DOUBLE, chosenSet() AS LONG) AS DOUBLE
' Simulated annealing (t0=1.0, alpha=0.995 fissati nello shim).
DECLARE FUNCTION SET_COVER_SA LIB "gmbor" ALIAS "ofx_set_cover_sa" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, pen AS DOUBLE, maxit AS LONG, seed AS LONG, chosen() AS LONG) AS DOUBLE
DECLARE FUNCTION SET_COVER_LOCAL LIB "gmbor" ALIAS "ofx_set_cover_local" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG, maxit AS LONG) AS DOUBLE
' GRASP (seed fisso 12345 nello shim).
DECLARE FUNCTION SET_COVER_GRASP LIB "gmbor" ALIAS "ofx_set_cover_grasp" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alpha AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
' Reactive GRASP: alphas() = [nalpha, a0, a1, ...] (nalpha compatto in testa; seed fisso).
DECLARE FUNCTION SET_COVER_RGRASP LIB "gmbor" ALIAS "ofx_set_cover_rgrasp" (elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alphas() AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE
DECLARE FUNCTION MAX_COVERAGE LIB "gmbor" ALIAS "ofx_max_coverage" (elem() AS LONG, m AS LONG, n AS LONG, k AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG
' p-median: dist(n*n); medians(p), assign(n).
DECLARE FUNCTION P_MEDIAN LIB "gmbor" ALIAS "ofx_pmedian" (dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG, assign() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION P_CENTER LIB "gmbor" ALIAS "ofx_pcenter" (dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG) AS DOUBLE
' Vertex cover: edges() = 2*ne; cover() = n.
DECLARE FUNCTION VERTEX_COVER LIB "gmbor" ALIAS "ofx_vertex_cover" (edges() AS LONG, n AS LONG, ne AS LONG, cover() AS LONG) AS LONG
DECLARE FUNCTION HITTING_SET LIB "gmbor" ALIAS "ofx_hitting_set" (elem() AS LONG, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG
DECLARE FUNCTION FACILITY_LOCATION LIB "gmbor" ALIAS "ofx_facility_location" (openCost() AS DOUBLE, connCost() AS DOUBLE, m AS LONG, n AS LONG, open() AS LONG) AS DOUBLE
' k-means: points(n*dim), centroids(k*dim), labels(n); seed fisso nello shim.
DECLARE FUNCTION KMEANS LIB "gmbor" ALIAS "ofx_kmeans" (points() AS DOUBLE, n AS LONG, dim AS LONG, k AS LONG, centroids() AS DOUBLE, labels() AS LONG, maxit AS LONG) AS DOUBLE

' ── CP: SAT_DPLL / all-different / CSP binario ────────────────────────────────────
' clauses() flat; clauseStart()/clauseLen() = nclauses; assignment() = nvars; 1 = SAT_DPLL.
DECLARE FUNCTION SAT_DPLL LIB "gmbor" ALIAS "ofx_sat" (clauses() AS LONG, clauseStart() AS LONG, clauseLen() AS LONG, nvars AS LONG, nclauses AS LONG, assignment() AS LONG) AS LONG
' dom() = n*D; sol() = n.
DECLARE FUNCTION CP_ALLDIFF LIB "gmbor" ALIAS "ofx_alldiff" (dom() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG
DECLARE FUNCTION CP_BINARY LIB "gmbor" ALIAS "ofx_cp_binary" (dom() AS LONG, allowed() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG

' ── ROUTING: TSP / VRP / VRP_PICKUP_DELIVERY / arc routing (23) ──────────────────────────────
' Turistica: tour() = n (ordine di visita); dist() = n*n. Seed fissi dove la firma C
' supererebbe gli 8 argomenti FFI.
DECLARE FUNCTION TSP_NEAREST LIB "gmbor" ALIAS "ofx_tsp_nn" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_2OPT LIB "gmbor" ALIAS "ofx_tsp_2opt" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_TOUR_COST LIB "gmbor" ALIAS "ofx_tsp_tour_cost" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_EXTRA_MILEAGE LIB "gmbor" ALIAS "ofx_tsp_extra_mileage" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
' GRASP: alpha=0.3 e seed fissi nello shim.
DECLARE FUNCTION TSP_GRASP LIB "gmbor" ALIAS "ofx_tsp_grasp" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_VNS LIB "gmbor" ALIAS "ofx_tsp_vns" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_3OPT LIB "gmbor" ALIAS "ofx_tsp_3opt" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_SA LIB "gmbor" ALIAS "ofx_tsp_sa" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, T0 AS DOUBLE, cooling AS DOUBLE, iters AS LONG) AS DOUBLE
' tenure_kind: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth.
DECLARE FUNCTION TSP_TABU LIB "gmbor" ALIAS "ofx_tsp_tabu" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG, tenureKind AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_ONE_TREE LIB "gmbor" ALIAS "ofx_tsp_one_tree" (dist() AS DOUBLE, n AS LONG, root AS LONG, edges() AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_HK_BOUND LIB "gmbor" ALIAS "ofx_tsp_hk_bound" (dist() AS DOUBLE, n AS LONG, maxit AS LONG, pi() AS DOUBLE) AS DOUBLE
' succ()/comp()/sizes() = n; ritorna il numero di sottotour.
DECLARE FUNCTION TSP_SUBTOURS LIB "gmbor" ALIAS "ofx_tsp_subtours" (succ() AS LONG, n AS LONG, comp() AS LONG, sizes() AS LONG) AS LONG
DECLARE FUNCTION TSP_DFJ LIB "gmbor" ALIAS "ofx_tsp_dfj" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxcuts AS LONG) AS DOUBLE
' ERX: p1()/p2()/child() = n; ritorna 1.
DECLARE FUNCTION TSP_ERX LIB "gmbor" ALIAS "ofx_tsp_erx" (p1() AS LONG, p2() AS LONG, n AS LONG, child() AS LONG) AS LONG
DECLARE FUNCTION TSP_GA LIB "gmbor" ALIAS "ofx_tsp_ga" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, pop AS LONG, gens AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_OR_OPT LIB "gmbor" ALIAS "ofx_tsp_or_opt" (dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_DOUBLE_TREE LIB "gmbor" ALIAS "ofx_tsp_double_tree" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE
DECLARE FUNCTION TSP_HELD_KARP LIB "gmbor" ALIAS "ofx_tsp_held_karp" (dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE

' CVRP: dist (n+1)x(n+1) col deposito; demand() = n; routeOf() = n; total(0).
DECLARE FUNCTION VRP_CLARKE_WRIGHT LIB "gmbor" ALIAS "ofx_vrp_clarke_wright" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRP_TIME_WINDOWS: tw() = ready[n] | due[n] | service[n] (11 parametri C compattati).
DECLARE FUNCTION VRP_TIME_WINDOWS LIB "gmbor" ALIAS "ofx_vrp_time_windows" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, tw() AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRP_MULTI_DEPOT: depositi agli indici 0..ndepots-1.
DECLARE FUNCTION VRP_MULTI_DEPOT LIB "gmbor" ALIAS "ofx_mdvrp" (dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, ndepots AS LONG, cap AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG
' VRP_PICKUP_DELIVERY: pickup()/delivery() = nreq; route() = 2*nreq; total(0).
DECLARE FUNCTION VRP_PICKUP_DELIVERY LIB "gmbor" ALIAS "ofx_pdp" (dist() AS DOUBLE, demand() AS DOUBLE, nreq AS LONG, cap AS DOUBLE, pickup() AS LONG, delivery() AS LONG, route() AS LONG, total() AS DOUBLE) AS LONG
' Arc routing (Chinese postman): euler() = n*n+2; elen(0) = lunghezza.
DECLARE FUNCTION ARC_ROUTING LIB "gmbor" ALIAS "ofx_arc_routing" (w() AS DOUBLE, n AS LONG, euler() AS LONG, elen() AS LONG) AS DOUBLE

' ── SCHEDULING: macchina singola, flow shop, progetto, job shop (18) ─────────
DECLARE FUNCTION SCHED_SPT LIB "gmbor" ALIAS "ofx_sched_spt" (p() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
DECLARE FUNCTION SCHED_WSPT LIB "gmbor" ALIAS "ofx_sched_wspt" (p() AS DOUBLE, w() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
DECLARE FUNCTION SCHED_EDD LIB "gmbor" ALIAS "ofx_sched_edd" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
' Min #tardivi (Moore-Hodgson): ritorna il numero di job tardivi; order() = n.
DECLARE FUNCTION SCHED_MOORE LIB "gmbor" ALIAS "ofx_sched_moore" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS LONG
DECLARE FUNCTION SCHED_LAWLER LIB "gmbor" ALIAS "ofx_sched_lawler" (p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE
' Intervalli: selected()/start()/end() = n.
DECLARE FUNCTION INTERVAL_MAX LIB "gmbor" ALIAS "ofx_interval_max" (start() AS DOUBLE, finish() AS DOUBLE, n AS LONG, selected() AS LONG) AS LONG
DECLARE FUNCTION INTERVAL_PARTITION LIB "gmbor" ALIAS "ofx_interval_partition" (start() AS DOUBLE, finish() AS DOUBLE, n AS LONG) AS LONG
' Flow shop: seq() = n.
DECLARE FUNCTION FLOWSHOP_JOHNSON LIB "gmbor" ALIAS "ofx_johnson" (p1() AS DOUBLE, p2() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE
' FLOWSHOP_NEH: p() = n*m (job i, macchina k = p(i*m+k)).
DECLARE FUNCTION FLOWSHOP_NEH LIB "gmbor" ALIAS "ofx_neh" (p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE
DECLARE FUNCTION FLOWSHOP_JOHNSON3 LIB "gmbor" ALIAS "ofx_johnson3" (p1() AS DOUBLE, p2() AS DOUBLE, p3() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE
DECLARE FUNCTION FLOWSHOP_CDS LIB "gmbor" ALIAS "ofx_cds" (p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE
DECLARE FUNCTION PARALLEL_LPT LIB "gmbor" ALIAS "ofx_lpt" (p() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE
' PROJECT_CPM: preds() = n*npred (predecessori, -1 padding); est()/lst()/slack() = n.
DECLARE FUNCTION PROJECT_CPM LIB "gmbor" ALIAS "ofx_cpm" (dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, lst() AS DOUBLE, slack() AS DOUBLE) AS DOUBLE
' PROJECT_PERT: to()/tm()/tp() = n; est()/variance() = n.
DECLARE FUNCTION PROJECT_PERT LIB "gmbor" ALIAS "ofx_pert" (to() AS DOUBLE, tm() AS DOUBLE, tp() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, variance() AS DOUBLE) AS DOUBLE
' PROJECT_RCPSP: demand() = n*nr; capacity() = nr; start() = n.
DECLARE FUNCTION PROJECT_RCPSP LIB "gmbor" ALIAS "ofx_rcpsp" (dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, demand() AS DOUBLE, capacity() AS DOUBLE, nr AS LONG, start() AS LONG) AS DOUBLE
' Job shop: route()/dur()/start() = njobs*nmach.
DECLARE FUNCTION JOB_SHOP LIB "gmbor" ALIAS "ofx_job_shop" (route() AS LONG, dur() AS DOUBLE, njobs AS LONG, nmach AS LONG, start() AS LONG) AS DOUBLE
' Timetabling: conflicts() = n*n; slotOf() = n.
DECLARE FUNCTION TIMETABLING LIB "gmbor" ALIAS "ofx_timetabling" (conflicts() AS LONG, n AS LONG, slotOf() AS LONG) AS LONG
' Rostering: demand() = ndays; schedule() = nnurses*ndays; ritorna i turni non coperti.
DECLARE FUNCTION ROSTERING LIB "gmbor" ALIAS "ofx_rostering" (demand() AS LONG, nnurses AS LONG, ndays AS LONG, maxWork AS LONG, schedule() AS LONG) AS LONG
