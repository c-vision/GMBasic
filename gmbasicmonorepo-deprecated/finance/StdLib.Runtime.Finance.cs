// FILE: StdLib.Runtime.Finance.cs
// Dominio Finance della StdLib.Runtime: funzioni finanziarie che mancano ancora nel main switch
// della VM (CUMIPMT/CUMPRINC/DB/PRICE/DURATION/XIRR — le altre del namespace FINANCE, es. PMT/FV/
// IRR/NPV/SLN/SYD/DDB/YEARFRAC/DAYS360, sono già dispatchate inline in PCodeProcessor.Builtins.cs
// con implementazioni proprie, non toccate qui).
// PURO per costruzione (vedi Ast.NativeExecutionKind): nessun metodo qui accede a stato che
// sopravvive oltre la singola chiamata. Rispecchia esattamente le firme di CommonLib.Numerics
// (nessun riordino argomenti). Arità replicata dal vecchio dead file
// PCodeVm/PCodeProcessor.StdLib.Finance.cs (mai dispatchato): Price non espone i parametri
// opzionali firstCoupon/issue/calcMethod, Duration fissa modified=false — stesso comportamento
// consolidato di quel dead file.

namespace Basic.StdLibRuntime;

public static class FinanceFunctions
{
    public static decimal Cumipmt(decimal rate, decimal nper, decimal pv, int startPeriod, int endPeriod, int type) => CommonLib.Numerics.Cumipmt(rate, nper, pv, startPeriod, endPeriod, type);
    public static decimal Cumprinc(decimal rate, decimal nper, decimal pv, int startPeriod, int endPeriod, int type) => CommonLib.Numerics.Cumprinc(rate, nper, pv, startPeriod, endPeriod, type);
    public static decimal Db(decimal cost, decimal salvage, decimal life, decimal period, int month) => CommonLib.Numerics.Db(cost, salvage, life, period, month);
    public static double Xirr(double[] cashFlows, DateTime[] dates, double guess) => CommonLib.Numerics.Xirr(cashFlows, dates, guess);
    public static double Price(DateTime settlement, DateTime maturity, double rate, double yld, double redemption, int frequency, int basis) => CommonLib.Numerics.Price(settlement, maturity, rate, yld, redemption, frequency, basis);
    public static double Duration(DateTime settlement, DateTime maturity, double coupon, double yld, int frequency, int basis) => CommonLib.Numerics.Duration(settlement, maturity, coupon, yld, frequency, basis, false);
}
