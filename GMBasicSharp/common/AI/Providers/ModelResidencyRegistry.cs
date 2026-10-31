// FILE: Basic.Providers.ModelResidencyRegistry
// M21 — Registry di RESIDENZA unificato multi-provider: un'unica vista dei modelli caricati da
// TUTTI i provider embedded (mlx, llamacpp, ...) con lo stesso stato di residenza
// (DISK/WARM/ACTIVE) e una POLITICA DI MEMORIA condivisa: quando serve spazio, demota i modelli
// ACTIVE meno usati e poi li evicta (LRU), su qualunque provider.
//
// questo livello C#); qui il registry lo registra solo per la VISTA. MLX/llama.cpp sono provider
// C#-hosted: la loro policy di memoria è qui.
using System;
using System.Collections.Generic;
using System.Linq;

namespace Basic.Providers;

internal enum ModelResidencyState { Disk = 0, Warm = 1, Active = 2 }

internal static class ModelResidencyRegistry
{
    internal sealed class Entry
    {
        public required string Name { get; init; }
        public required string Provider { get; init; }
        public long SizeBytes { get; set; }
        public ModelResidencyState Residency { get; set; }
        public long LastUsed { get; set; }
        public int Priority { get; set; }      // M21 orchestrazione: più alto = tenuto di più
        public string Device { get; set; } = "auto";   // M21: AUTO | CPU | METAL (stato GPU dichiarato)
        public long OffloadBytes { get; set; }         // M21: budget di offload dichiarato (0 = nessuno)
        public Action? Evict { get; init; }    // libera il modello (unload/dispose)
        public Action? Demote { get; init; }   // rinuncia ai pesi residenti (se supportato)
    }

    private static readonly object _lock = new();
    private static readonly Dictionary<string, Entry> _entries = new(StringComparer.OrdinalIgnoreCase);
    private static long _clock;

    /// <summary>Memoria disponibile+reclamabile (byte). Iniettabile nei test.</summary>
    internal static Func<ulong> AvailableBytes { get; set; } = static () => 0;

    internal static void Register(string name, string provider, long sizeBytes, ModelResidencyState residency,
                                  Action? evict = null, Action? demote = null, int priority = 0,
                                  string device = "auto", long offloadBytes = 0)
    {
        if (string.IsNullOrWhiteSpace(name)) return;
        lock (_lock)
            _entries[name] = new Entry { Name = name, Provider = provider, SizeBytes = sizeBytes,
                                         Residency = residency, LastUsed = ++_clock, Priority = priority,
                                         Device = device, OffloadBytes = offloadBytes,
                                         Evict = evict, Demote = demote };
    }

    /// <summary>M21: stato GPU dichiarato (AUTO/CPU/METAL) + budget di offload (non misurato: M5 Max
    /// non espone contatori VRAM; Metal è usato solo dagli slot esperti).</summary>
    internal static void SetDevice(string name, string device, long offloadBytes = 0)
    {
        lock (_lock) { if (_entries.TryGetValue(name, out var e)) { e.Device = device; e.OffloadBytes = offloadBytes; } }
    }

    /// <summary>M21 orchestrazione: priorità di eviction (guidata da AI SET PRIORITY).</summary>
    internal static void SetPriority(string name, int priority)
    {
        lock (_lock) { if (_entries.TryGetValue(name, out var e)) e.Priority = priority; }
    }

    private static Entry? MainVictim(IEnumerable<Entry> cand, Func<Entry, bool> filter) =>
        cand.Where(filter).OrderBy(e => e.Priority).ThenBy(e => e.LastUsed).FirstOrDefault();

    internal static void Remove(string name)
    {
        lock (_lock) { _entries.Remove(name); }
    }

    internal static void Touch(string name)
    {
        lock (_lock) { if (_entries.TryGetValue(name, out var e)) e.LastUsed = ++_clock; }
    }

    internal static void SetResidency(string name, ModelResidencyState r)
    {
        lock (_lock) { if (_entries.TryGetValue(name, out var e)) { e.Residency = r; e.LastUsed = ++_clock; } }
    }

    internal static IReadOnlyList<Entry> Snapshot()
    {
        lock (_lock) { return _entries.Values.Select(Clone).ToList(); }
    }

    /// <summary>Libera memoria su TUTTI i provider finché `AvailableBytes() >= required`: demota gli
    /// ACTIVE LRU che lo supportano, poi evicta i LRU. `exempt` = modello da non toccare. Non lancia.</summary>
    internal static bool EnsureCapacity(ulong required, string? exempt = null)
    {
        if (required == 0) return true;
        if (AvailableBytes() >= required) return true;
        while (true)
        {
            Entry? victim; bool demote;
            lock (_lock)
            {
                var cand = _entries.Values
                    .Where(e => !string.Equals(e.Name, exempt, StringComparison.OrdinalIgnoreCase)).ToList();
                victim = MainVictim(cand, e => e.Residency == ModelResidencyState.Active && e.Demote != null);
                demote = victim != null;
                victim ??= MainVictim(cand, _ => true);
                if (victim == null) return AvailableBytes() >= required;
            }
            try
            {
                if (demote) { victim.Demote!(); SetResidency(victim.Name, ModelResidencyState.Warm); }
                else { victim.Evict!(); Remove(victim.Name); }
            }
            catch { Remove(victim.Name); return AvailableBytes() >= required; }
            if (AvailableBytes() >= required) return true;
        }
    }

    /// <summary>Vista unificata leggibile: "provider/nome:STATE(sizeMB)".</summary>
    internal static string Describe()
    {
        var snap = Snapshot();
        if (snap.Count == 0) return "(nessun modello in residenza)";
        return string.Join("; ", snap.OrderBy(e => e.Provider).ThenByDescending(e => e.LastUsed)
            .Select(e => $"{e.Provider}/{e.Name}:{e.Residency}@{e.Device}({e.SizeBytes / (1 << 20)}MB)"));
    }

    private static Entry Clone(Entry e) =>
        new() { Name = e.Name, Provider = e.Provider, SizeBytes = e.SizeBytes, Residency = e.Residency,
                LastUsed = e.LastUsed, Priority = e.Priority, Device = e.Device, OffloadBytes = e.OffloadBytes,
                Evict = e.Evict, Demote = e.Demote };
}
