// FILE: CommonLib.SystemMemory
// Memoria di sistema DISPONIBILE + RECLAMABILE, con API native del SO (attendibile):
//   - macOS:  host_statistics64(HOST_VM_INFO64) → (free + inactive + purgeable + speculative) × pageSize
//   - Linux:  /proc/meminfo → MemAvailable (stima del kernel di memoria ottenibile senza swap)
//   - Windows: GlobalMemoryStatusEx → ullAvailPhys
// È la base del rifiuto di caricamento modelli (loader AI) e della funzione BASIC SYSTEM.SYS("MEMAVAILABLE").
using System;
using System.IO;
using System.Runtime.InteropServices;

namespace CommonLib;

public static class SystemMemory
{
    /// <summary>Byte di RAM disponibili O RECLAMABILI (senza ricorrere allo swap). 0 se non determinabile.</summary>
    public static ulong AvailableReclaimableBytes()
    {
        try
        {
            if (OperatingSystem.IsMacOS()) return MacAvailable();
            if (OperatingSystem.IsLinux()) return LinuxMemInfo("MemAvailable");
            if (OperatingSystem.IsWindows()) return WindowsAvail();
        }
        catch { /* fallback sotto */ }
        // Fallback .NET: memoria gestita disponibile (proxy prudente, non include il file cache).
        try
        {
            var info = GC.GetGCMemoryInfo();
            var avail = (ulong)info.TotalAvailableMemoryBytes;
            return avail;
        }
        catch { return 0; }
    }

    /// <summary>Byte di RAM fisica totale. 0 se non determinabile.</summary>
    public static ulong TotalBytes()
    {
        try
        {
            if (OperatingSystem.IsMacOS())
            {
                var v = SysctlU64("hw.memsize");
                if (v != 0) return v;
            }
            if (OperatingSystem.IsLinux()) return LinuxMemInfo("MemTotal");
            if (OperatingSystem.IsWindows())
            {
                var m = new MEMORYSTATUSEX { dwLength = (uint)Marshal.SizeOf<MEMORYSTATUSEX>() };
                if (GlobalMemoryStatusEx(ref m)) return m.ullTotalPhys;
            }
        }
        catch { }
        return 0;
    }

    // ── macOS ──────────────────────────────────────────────────────────────────
    // Layout COMPLETO di `struct vm_statistics64` (mach/vm_statistics.h): i primi 4 campi e
    // purgeable/speculative ecc. sono natural_t (uint32), il resto uint64. Un layout sbagliato
    // legge garbage (bug reale: purgeable/speculative dichiarati uint64 → MEMAVAILABLE ~4 TB).
    [StructLayout(LayoutKind.Sequential)]
    private struct VmStatistics64
    {
        public uint free_count, active_count, inactive_count, wire_count;
        public ulong zero_fill_count, reactivations, pageins, pageouts, faults, cow_faults;
        public ulong lookups, hits, purges;
        public uint purgeable_count, speculative_count;
        public ulong decompressions, compressions, swapins, swapouts;
        public uint compressor_page_count, throttled_count, external_page_count, internal_page_count;
        public ulong total_uncompressed_pages_in_compressor;
    }

    [DllImport("/usr/lib/libSystem.dylib")] private static extern uint mach_host_self();
    [DllImport("/usr/lib/libSystem.dylib")] private static extern int host_statistics64(IntPtr host, int flavor, ref VmStatistics64 info, ref uint count);
    [DllImport("/usr/lib/libSystem.dylib")] private static extern ulong mach_absolute_time();

    private const int HOST_VM_INFO64 = 4;

    private static ulong PageSize()
    {
        var p = SysctlU64("hw.pagesize");
        return p != 0 ? p : 16384;
    }

    private static ulong MacAvailable()
    {
        var host = (IntPtr)mach_host_self();
        var info = new VmStatistics64();
        uint count = (uint)(Marshal.SizeOf<VmStatistics64>() / sizeof(uint));
        if (host_statistics64(host, HOST_VM_INFO64, ref info, ref count) != 0) return 0;
        ulong pages = (ulong)info.free_count + info.inactive_count + info.purgeable_count + info.speculative_count;
        return pages * PageSize();
    }

    private static ulong SysctlU64(string name)
    {
        ulong value = 0;
        nuint size = (nuint)sizeof(ulong);
        if (sysctlbyname(name, out value, ref size, IntPtr.Zero, 0) != 0) return 0;
        return value;
    }

    [DllImport("/usr/lib/libSystem.dylib")] private static extern int sysctlbyname(string name, out ulong oldp, ref nuint oldlenp, IntPtr newp, nuint newlen);

    // ── Linux ─────────────────────────────────────────────────────────────────
    private static ulong LinuxMemInfo(string key)
    {
        try
        {
            foreach (var line in File.ReadLines("/proc/meminfo"))
            {
                if (!line.StartsWith(key + ":", StringComparison.Ordinal)) continue;
                var rest = line[(key.Length + 1)..].Trim();
                var num = rest.Split(' ', StringSplitOptions.RemoveEmptyEntries)[0];
                if (ulong.TryParse(num, out var kb)) return kb * 1024UL;
            }
        }
        catch { }
        return 0;
    }

    // ── Windows ───────────────────────────────────────────────────────────────
    [StructLayout(LayoutKind.Sequential)]
    private struct MEMORYSTATUSEX
    {
        public uint dwLength, dwMemoryLoad;
        public ulong ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile;
        public ulong ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

    private static ulong WindowsAvail()
    {
        var m = new MEMORYSTATUSEX { dwLength = (uint)Marshal.SizeOf<MEMORYSTATUSEX>() };
        return GlobalMemoryStatusEx(ref m) ? m.ullAvailPhys : 0;
    }
}
