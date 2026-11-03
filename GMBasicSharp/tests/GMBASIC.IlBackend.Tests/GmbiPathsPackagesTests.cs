// Dove stanno i pacchetti: UNA regola, due macchine.
//   checkout sorgente (sviluppo, compilatore presente) -> <root>/packages (+ <root>/../packages)
//   client (app installata, nessun compilatore)        -> ~/.gmbasic/packages
// La regola è la stessa del runtime C (gmb_paths_packages_dir): qui si verifica il lato C#.
using System;
using System.IO;
using CommonLib;
using Xunit;

namespace Basic.IlBackend.Tests;

public class GmbiPathsPackagesTests
{
    private static string TmpDir()
    {
        var d = Path.Combine(Path.GetTempPath(), "gmbpaths_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(d);
        return d;
    }

    // Un programma installato NON ha un checkout: i pacchetti vanno nella cache UTENTE,
    // non accanto all'eseguibile (era il comportamento precedente: sparivano a ogni update).
    [Fact]
    public void Client_Senza_Checkout_Usa_Cache_Utente()
    {
        var dir = TmpDir();
        try
        {
            var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            Assert.Equal(Path.Combine(home, ".gmbasic", "packages"), GmbiPaths.PackagesDirFor(dir, null));
        }
        finally { Directory.Delete(dir, true); }
    }

    // Checkout sorgente: i pacchetti del progetto stanno in <root>/packages.
    [Fact]
    public void Checkout_Usa_Packages_Di_Progetto()
    {
        var dir = TmpDir();
        try
        {
            File.WriteAllText(Path.Combine(dir, "GMBASIC.sln"), "");
            Directory.CreateDirectory(Path.Combine(dir, "packages"));
            Assert.Equal(Path.Combine(dir, "packages"), GmbiPaths.PackagesDirFor(dir, null));
        }
        finally { Directory.Delete(dir, true); }
    }

    // Monorepo: la cache è alla RADICE del monorepo (<root>/../packages), non dentro GMBasic/.
    [Fact]
    public void Monorepo_Usa_Packages_Della_Radice()
    {
        var mono = TmpDir();
        try
        {
            var gm = Path.Combine(mono, "GMBasic");
            Directory.CreateDirectory(Path.Combine(gm, "native", "gmb_runtime_c", "include"));
            File.WriteAllText(Path.Combine(gm, "native", "gmb_runtime_c", "include", "gmb_runtime.h"), "");
            Directory.CreateDirectory(Path.Combine(mono, "packages"));
            Assert.Equal(Path.Combine(mono, "packages"), GmbiPaths.PackagesDirFor(gm, null));
        }
        finally { Directory.Delete(mono, true); }
    }

    // Un packagesDir configurato esplicitamente NON viene mai sostituito.
    [Fact]
    public void Configurato_Vince_Sempre()
    {
        var dir = TmpDir();
        try
        {
            Assert.Equal("/opt/miei-pacchetti", GmbiPaths.PackagesDirFor(dir, "/opt/miei-pacchetti"));
        }
        finally { Directory.Delete(dir, true); }
    }
}
