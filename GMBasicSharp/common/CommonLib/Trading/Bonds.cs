// FILE: CommonLib/Trading/Bonds.cs
// FACADE P/Invoke verso libgmbquant (gmbsuite/gmbquant + shared/libs/quant).
//
// La versione C# delle formule (Bonds.cs originale, 281 righe) e' archiviata in
// gmbasicmonorepo-deprecated/... . Stessa API pubblica; le date sono convertite in
// OADate (lo stesso seriale delle DateTime di GMBasic) e passate alla lib C.
// model 0 = Trading.Bonds (questa facade); la selezione model 0/1 del builtin sta in
// TradingFunctions e usa QuantNative direttamente.
using System;

namespace CommonLib.Trading;

public static class Bonds
{
    private static double Oa(DateTime d) => d.ToOADate();

    public static double BondPrice(DateTime settlement, DateTime maturity, double rate, double yld,
        double redemption, int frequency)
        => QuantNative.BondPrice(Oa(settlement), Oa(maturity), rate, yld, redemption, frequency, 0);

    public static double BondYield(DateTime settlement, DateTime maturity, double rate, double pr,
        double redemption, int frequency, double guess = 0.1)
        => QuantNative.BondYield(Oa(settlement), Oa(maturity), rate, pr, redemption, frequency, 0);

    /// <summary>Variante con modello 0/1 (usata dal builtin: 0 = Trading.Bonds, 1 = Numerics.Price).</summary>
    public static double BondPriceModel(DateTime settlement, DateTime maturity, double rate, double yld,
        double redemption, int frequency, int model)
        => QuantNative.BondPrice(Oa(settlement), Oa(maturity), rate, yld, redemption, frequency, model);

    public static double BondYieldModel(DateTime settlement, DateTime maturity, double rate, double pr,
        double redemption, int frequency, int model)
        => QuantNative.BondYield(Oa(settlement), Oa(maturity), rate, pr, redemption, frequency, model);

    public static double BondEquivalentYield(double faceValue, double bondPrice, double daysToMaturity)
        => QuantNative.BondEquivalentYield(faceValue, bondPrice, daysToMaturity);

    public static double CurrentYield(double annualCoupons, double bondPrice)
        => QuantNative.CurrentYield(annualCoupons, bondPrice);

    public static double YieldToMaturity(double couponPayment, double faceValue, double price, double years)
        => QuantNative.YieldToMaturity(couponPayment, faceValue, price, years);

    public static double ZeroCouponBondValue(double faceValue, double rate, double time)
        => QuantNative.ZeroCouponBondValue(faceValue, rate, time);

    public static double ZeroCouponBondYield(double faceValue, double price, double time)
        => QuantNative.ZeroCouponBondYield(faceValue, price, time);
}
