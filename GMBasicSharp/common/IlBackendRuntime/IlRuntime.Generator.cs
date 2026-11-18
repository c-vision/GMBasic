// FILE: Basic.IlBackend.IlRuntime.Generator.cs
// GENERATOR ... YIELD / NEXT(generatore) per il backend IL.
//
// Il corpo del generatore è compilato come metodo statico `Proc_<id>(params)` (void). Il call site
// non lo esegue: crea un IlGeneratorInstance (lazy). Al primo NEXT il corpo parte su un thread
// dedicato; ogni YIELD consegna il valore al chiamante e sospende il thread finché il NEXT
// successivo non lo riprende (handoff con due semafori: un solo thread esegue alla volta, quindi
// nessun dato condiviso richiede lock). Fine corsa (corpo terminato) → BasicException(StopIteration),
// la stessa condizione di PCode/VM-C/C99 (catturabile con TRY/CATCH).

using System;
using System.Reflection;
using System.Threading;

namespace Basic.IlBackend;

/// <summary>Stato di un'istanza di generatore IL: il corpo gira su un thread, sincronizzato con il consumatore.</summary>
public sealed class IlGeneratorInstance
{
    // Il generatore che sta eseguendo SUL thread corrente (impostato all'avvio del suo thread).
    [ThreadStatic] private static IlGeneratorInstance? _current;
    internal static IlGeneratorInstance? Current => _current;

    private const int StackBytes = 16 * 1024 * 1024;

    private readonly MethodBase _method;
    private readonly object?[] _args;
    private readonly SemaphoreSlim _toGenerator = new(0);
    private readonly SemaphoreSlim _toConsumer = new(0);
    private Thread? _thread;
    private object? _value;
    private Exception? _error;
    private string? _traceText;
    private bool _done;
    private bool _abandoned;

    internal IlGeneratorInstance(MethodBase method, object?[] args)
    {
        _method = method;
        _args = args;
    }

    /// <summary>Prossimo valore prodotto dal generatore; lancia StopIteration a fine corsa.</summary>
    internal object? Next()
    {
        if (_done) throw StopIteration();
        if (_thread is null)
        {
            _thread = new Thread(Run, StackBytes) { IsBackground = true, Name = "gmb-generator" };
            _thread.Start();
        }
        else
        {
            _toGenerator.Release();
        }
        _toConsumer.Wait();
        if (_error is not null)
        {
            var e = _error;
            _error = null;
            _done = true;
            /* STACK TRACE: il corpo ha girato su un ALTRO thread, con la sua catena. I suoi
             * frame vanno anteposti a quelli del chiamante, come sulle gambe native dove la
             * catena è condivisa tra corpo e chiamante (l'emitter IL li emette nel prologo del
             * generatore, quindi qui sono già in _traceText). */
            IlRuntime.SetTracePrefix(_traceText);
            throw e;
        }
        if (_done) throw StopIteration();
        return _value;
    }

    /// <summary>Chiamato dal corpo (thread del generatore): consegna <paramref name="value"/> e attende il prossimo NEXT.</summary>
    internal void Yield(object? value)
    {
        _value = value;
        _toConsumer.Release();
        _toGenerator.Wait();
        if (_abandoned) throw new ThreadInterruptedException();
    }

    private void Run()
    {
        _current = this;
        try
        {
            var parameters = _method.GetParameters();
            var callArgs = new object?[parameters.Length];
            for (var i = 0; i < parameters.Length; i++)
            {
                var target = parameters[i].ParameterType;
                if (target.IsByRef) target = target.GetElementType()!;
                var arg = i < _args.Length ? _args[i] : null;
                callArgs[i] = arg is null || target.IsInstanceOfType(arg)
                    ? arg
                    : Convert.ChangeType(arg, target, System.Globalization.CultureInfo.InvariantCulture);
            }
            _method.Invoke(null, callArgs);
        }
        catch (ThreadInterruptedException) { /* consumatore sparito */ }
        catch (TargetInvocationException tie) when (tie.InnerException is ThreadInterruptedException) { }
        catch (TargetInvocationException tie)
        {
            _error = tie.InnerException ?? tie;
            _traceText = IlRuntime.TraceText();   /* catena del corpo, PRIMA di lasciare il thread */
        }
        catch (Exception ex)
        {
            _error = ex;
            _traceText = IlRuntime.TraceText();
        }
        finally
        {
            _done = true;
            _toConsumer.Release();
        }
    }

    private static Exception StopIteration() =>
        new BasicException(ErrorCode.StopIteration, SourcePosition.Unknown, "Stop iteration");
}

public static partial class IlRuntime
{
    /// <summary>Crea (senza eseguire) l'iteratore di un GENERATOR: <paramref name="body"/> è il token del metodo `Proc_&lt;id&gt;`.</summary>
    public static object GeneratorCreate(RuntimeMethodHandle body, object?[] args)
        => new IlGeneratorInstance(MethodBase.GetMethodFromHandle(body)!, args);

    /// <summary>YIELD: consegna il valore al consumatore e sospende il corpo del generatore corrente.</summary>
    public static void GeneratorYield(object? value)
    {
        var gen = IlGeneratorInstance.Current
            ?? throw new InvalidOperationException("YIELD fuori da un GENERATOR in esecuzione.");
        gen.Yield(value);
    }

    /// <summary>NEXT(generatore): prossimo valore (object) o BasicException(StopIteration) a fine corsa.</summary>
    public static object? GeneratorNext(object? generator)
    {
        if (generator is IlGeneratorInstance gen) return gen.Next();
        throw new BasicException(ErrorCode.InvalidParameter, SourcePosition.Unknown,
            "NEXT() richiede un generatore.");
    }
}
