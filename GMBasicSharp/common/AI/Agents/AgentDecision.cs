// FILE: Basic.Agents.AgentDecision — N3 Needle3 (confidence source/status + regola di accettazione).
using System.Collections.Generic;

namespace Basic.Agents;

/// <summary>Fonte della confidence di una decisione (N3, Needle3).
/// MAI assumere confidence = probabilità di correttezza: il significato dipende dalla fonte.
/// Dopo un fine-tuning la confidence può essere assente (UNKNOWN): il backend deve dichiararlo.</summary>
public enum ConfidenceSource
{
    Unknown = 0,
    ModelCalibrated = 1,
    TokenProbability = 2,
    Heuristic = 3,
    Ensemble = 4,
}

/// <summary>Stato di calibrazione della confidence (N3, Needle3).</summary>
public enum CalibrationStatus
{
    Unknown = 0,
    Uncalibrated = 1,
    Calibrated = 2,
}

/// <summary>Motivo di escalation (N3/N4, Needle3). None = nessuna escalation, eseguire.</summary>
public enum EscalationReason
{
    None = 0,
    LowConfidence = 1,
    UncalibratedConfidence = 2,
    UnsupportedCapability = 3,
    NoTool = 4,
    Error = 5,
}

/// <summary>Risultato di una decisione con confidence tracciata (N3, Needle3).
/// La soglia di accettazione si applica SOLO se <see cref="Calibration"/> è Calibrated;
/// altrimenti si escala di default (mai fidarsi di un numero non calibrato).</summary>
public sealed class DecisionResult
{
    public string? Value { get; init; }
    public double Probability { get; init; }
    public double Confidence { get; init; }
    public ConfidenceSource Source { get; init; } = ConfidenceSource.Unknown;
    public CalibrationStatus Calibration { get; init; } = CalibrationStatus.Unknown;

    /// <summary>True solo se calibrata E sopra soglia. Tutto il resto escala.</summary>
    public bool ShouldAccept(double threshold) =>
        Calibration == CalibrationStatus.Calibrated
        && Confidence >= threshold
        && threshold >= 0.0 && threshold <= 1.0;

    /// <summary>None se accettabile, altrimenti il motivo dell'escalation.</summary>
    public EscalationReason EscalationOrNone(double threshold)
    {
        if (ShouldAccept(threshold)) return EscalationReason.None;
        if (Calibration != CalibrationStatus.Calibrated) return EscalationReason.UncalibratedConfidence;
        return EscalationReason.LowConfidence;
    }
}
