namespace CommonLib.Trading;

/// <summary>
/// Tipi di dati immutabili per il motore di ottimizzazione e backtesting di portafoglio.
///
/// Record inclusi:
/// - <see cref="PortfolioResult"/>: risultato di un'ottimizzazione (pesi, rendimento, rischio, Sharpe).
/// - <see cref="PortfolioConstraints"/>: vincoli per l'ottimizzazione (rendimento target, bound inferiori/superiori).
/// - <see cref="GaParameters"/>: parametri per l'algoritmo genetico (popolazione, generazioni, tassi).
/// - <see cref="PortfolioMetrics"/>: metriche di performance sintetiche (CAGR, Sharpe, drawdown, volatilità).
/// - <see cref="BacktestResult"/>: risultato completo di un backtest (serie rendimenti, pesi finali, metriche).
/// - <see cref="RebalanceStrategy"/>: strategia di ribilanciamento (tipo, periodo, finestra lookback, asset).
/// </summary>

/// <summary>
/// Risultato di un'ottimizzazione di portafoglio. Contiene i pesi ottimali, il rendimento atteso,
/// la deviazione standard (rischio) e l'indice di Sharpe.
/// </summary>
/// <param name="Weights">Vettore dei pesi ottimali per ciascun asset.</param>
/// <param name="Return">Rendimento atteso del portafoglio ottimizzato.</param>
/// <param name="StDev">Deviazione standard (rischio) del portafoglio ottimizzato.</param>
/// <param name="SharpeRatio">Indice di Sharpe del portafoglio ottimizzato.</param>
public sealed record PortfolioResult(
    double[] Weights,
    double Return,
    double StDev,
    double SharpeRatio
);

/// <summary>
/// Vincoli opzionali per l'ottimizzazione mean-variance. Permette di specificare un rendimento
/// target e limiti inferiore/superiore per i pesi dei singoli asset.
/// </summary>
/// <param name="TargetReturn">Rendimento annualizzato target (opzionale, usato con ottimizzazione TARGETRETURN).</param>
/// <param name="LowerBounds">Limiti inferiori per i pesi (opzionale, default 0.0).</param>
/// <param name="UpperBounds">Limiti superiori per i pesi (opzionale, default 1.0).</param>
public sealed record PortfolioConstraints(
    double? TargetReturn,
    double[]? LowerBounds,
    double[]? UpperBounds
);

/// <summary>
/// Parametri di configurazione per l'algoritmo genetico (GA) usato nell'ottimizzazione di portafoglio.
/// Controlla dimensione della popolazione, numero di generazioni, tassi di mutazione/crossover
/// e il coefficiente di avversione al rischio.
/// </summary>
/// <param name="PopulationSize">Numero di individui per generazione.</param>
/// <param name="Generations">Numero massimo di generazioni.</param>
/// <param name="MutationRate">Probabilità di mutazione (0.0 – 1.0).</param>
/// <param name="CrossoverRate">Probabilità di crossover (0.0 – 1.0).</param>
/// <param name="RiskCoefficient">Coefficiente di avversione al rischio nella funzione fitness.</param>
public sealed record GaParameters(
    int PopulationSize,
    int Generations,
    double MutationRate,
    double CrossoverRate,
    double RiskCoefficient
);

/// <summary>
/// Metriche sintetiche di performance di un portafoglio o strategia di investimento.
/// Calcolate a partire da una serie storica di rendimenti.
/// </summary>
/// <param name="Cagr">CAGR (Compound Annual Growth Rate) — tasso di crescita annuale composto.</param>
/// <param name="SharpeRatio">Indice di Sharpe — rendimento in eccesso per unità di rischio.</param>
/// <param name="MaxDrawdown">Massimo drawdown — perdita massima dal picco al minimo.</param>
/// <param name="Volatility">Volatilità annualizzata — deviazione standard dei rendimenti.</param>
public sealed record PortfolioMetrics(
    double Cagr,
    double SharpeRatio,
    double MaxDrawdown,
    double Volatility
);

/// <summary>
/// Risultato completo di un backtest. Include la serie temporale dei rendimenti del portafoglio,
/// i pesi finali al termine del periodo e le metriche di performance calcolate.
/// </summary>
/// <param name="Returns">Array dei rendimenti del portafoglio per ciascun periodo.</param>
/// <param name="FinalWeights">Pesi finali degli asset al termine del backtest.</param>
/// <param name="Metrics">Metriche di performance (CAGR, Sharpe, drawdown, volatilità).</param>
public sealed record BacktestResult(
    double[] Returns,
    double[] FinalWeights,
    PortfolioMetrics Metrics
);

/// <summary>
/// Definisce una strategia di ribilanciamento del portafoglio.
/// Supporta ribilanciamento periodico (ogni N periodi) e basato su momentum
/// (seleziona i migliori N asset in base alla finestra di lookback).
/// </summary>
/// <param name="Type">Tipo di strategia: "PERIODIC" o "MOMENTUM".</param>
/// <param name="Period">Numero di periodi tra un ribilanciamento e l'altro.</param>
/// <param name="LookbackWindow">Finestra storica (in periodi) per calcolare il momentum.</param>
/// <param name="NumAssets">Numero di asset da selezionare nella strategia momentum.</param>
/// <param name="NumToReplace">Numero di asset da sostituire a ogni ribilanciamento (riservato).</param>
public sealed record RebalanceStrategy(
    string Type,
    int Period,
    int LookbackWindow,
    int NumAssets,
    int NumToReplace
);
