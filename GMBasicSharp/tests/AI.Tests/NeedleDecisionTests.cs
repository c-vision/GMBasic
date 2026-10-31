// FILE: AI.Tests.NeedleDecisionTests — N3+N4 Needle3 (decisioni + escalation, senza modello).
using System;
using System.Collections.Generic;
using Basic.Agents;
using Xunit;

namespace AI.Tests;

public sealed class NeedleDecisionTests
{
    // ---- N3: la soglia vale solo se CALIBRATED ----

    [Fact]
    public void CalibratedAboveThreshold_Accepts()
    {
        var d = new DecisionResult
        {
            Value = "send", Confidence = 0.9,
            Source = ConfidenceSource.ModelCalibrated, Calibration = CalibrationStatus.Calibrated,
        };
        Assert.True(d.ShouldAccept(0.6));
        Assert.Equal(EscalationReason.None, d.EscalationOrNone(0.6));
    }

    [Fact]
    public void CalibratedBelowThreshold_EscalatesLowConfidence()
    {
        var d = new DecisionResult
        {
            Value = "send", Confidence = 0.3,
            Source = ConfidenceSource.TokenProbability, Calibration = CalibrationStatus.Calibrated,
        };
        Assert.False(d.ShouldAccept(0.6));
        Assert.Equal(EscalationReason.LowConfidence, d.EscalationOrNone(0.6));
    }

    [Fact]
    public void UncalibratedHighConfidence_StillEscalates()
    {
        // Il caso Needle: dopo fine-tuning la confidence puo' essere alta ma senza significato.
        // Mai fidarsi di un numero non calibrato, per quanto alto.
        var d = new DecisionResult
        {
            Value = "send", Confidence = 0.99,
            Source = ConfidenceSource.Heuristic, Calibration = CalibrationStatus.Uncalibrated,
        };
        Assert.False(d.ShouldAccept(0.6));
        Assert.Equal(EscalationReason.UncalibratedConfidence, d.EscalationOrNone(0.6));
    }

    [Fact]
    public void UnknownCalibration_StillEscalates()
    {
        var d = new DecisionResult { Value = "send", Confidence = 0.99 };
        Assert.False(d.ShouldAccept(0.6));
        Assert.Equal(EscalationReason.UncalibratedConfidence, d.EscalationOrNone(0.6));
    }

    // ---- N4: scala cheap-first forzata con stub ----

    private static DecisionResult Stub(double confidence, CalibrationStatus cal) =>
        new() { Value = "x", Confidence = confidence, Source = ConfidenceSource.Heuristic, Calibration = cal };

    [Fact]
    public void Policy_WalksWholeScale_WithStubs()
    {
        var policy = new EscalationPolicy();
        var (l0, r0) = policy.Decide(EscalationLevel.ToolModel,
            new DecisionResult { Value = "x", Confidence = 0.9, Source = ConfidenceSource.ModelCalibrated, Calibration = CalibrationStatus.Calibrated });
        Assert.Equal((EscalationLevel.ToolModel, EscalationReason.None), (l0, r0));

        var (l1, r1) = policy.Decide(EscalationLevel.ToolModel, Stub(0.2, CalibrationStatus.Calibrated));
        Assert.Equal((EscalationLevel.Jev, EscalationReason.LowConfidence), (l1, r1));

        var (l2, r2) = policy.Decide(EscalationLevel.Jev, Stub(0.99, CalibrationStatus.Uncalibrated));
        Assert.Equal((EscalationLevel.LargeLlm, EscalationReason.UncalibratedConfidence), (l2, r2));

        var (l3, r3) = policy.Decide(EscalationLevel.LargeLlm, Stub(0.1, CalibrationStatus.Calibrated));
        Assert.Equal((EscalationLevel.Human, EscalationReason.LowConfidence), (l3, r3));
    }

    [Fact]
    public void Policy_ForcedReason_SkipsCurrentLevel()
    {
        var policy = new EscalationPolicy();
        var (l, r) = policy.Decide(EscalationLevel.ToolModel, Stub(0.9, CalibrationStatus.Calibrated), EscalationReason.NoTool);
        Assert.Equal(EscalationLevel.Jev, l);
        Assert.Equal(EscalationReason.NoTool, r);
    }

    [Fact]
    public void Policy_HumanIsCap_NeverBeyond()
    {
        var policy = new EscalationPolicy();
        var (l, _) = policy.Decide(EscalationLevel.Human, Stub(0.0, CalibrationStatus.Calibrated));
        Assert.Equal(EscalationLevel.Human, l);
    }
}
