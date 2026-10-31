using System;
using Xunit;
using Basic.Runtime;

namespace Basic.IlBackend.Tests;

/// <summary>Bridge eventi nativo→managed (Fase 6): un evento fired dal runtime C deve
/// rientrare nel managed tramite il thunk di CGfx (function pointer C → delegato gestito).</summary>
public class GfxEventThunkTests
{
    [Fact]
    public void ControlEvent_NativeToManaged_Thunk()
    {
        var gfx = new CGfxGraphicsService();
        gfx.FormReset();

        gfx.FormAddControl("CHECK", new object[] { 10, 10, "A" }, "k1");
        int h = (int)gfx.FormLastHandle();
        Assert.True(h >= 0, $"handle controllo = {h}");

        int ev = gfx.FormEventIndex("CHANGE");
        Assert.True(ev > 0, $"event index CHANGE = {ev}");

        int calls = 0, ctl = -1, seen = -1;
        CGfxGraphicsService.OnControlEvent = (c, e) => { calls++; ctl = c; seen = e; };
        try
        {
            gfx.FormRegisterEventThunk(h, ev);
            gfx.FormFireEvent(h, ev);
            Assert.Equal(1, calls);
            Assert.Equal(h, ctl);
            Assert.Equal(ev, seen);
            // Il contesto "evento in corso" e' azzerato dopo la callback.
            Assert.Equal(-1, gfx.FormFiringControl());
            Assert.Equal(0, gfx.FormFiringEvent());
        }
        finally { CGfxGraphicsService.OnControlEvent = null; }
    }
}
