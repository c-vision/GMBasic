using System;
using Xunit;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.Tests
{
    // G1 (piano ILBackend-GPM-Stackless-Coroutines): driver v2 — Call/Return/CompleteTask
    // per SUB void non grafiche. Gli stepper sono simulati con transizioni scritte a mano:
    // l'emitter StepV2_* è G2. Il driver deve fare push/pop della call chain, riprendere
    // ReturnPc del caller e completare SOLO alla root.
    public class IlGpmV2DriverTests
    {
        private static PCodeTask NewTask() =>
            new PCodeTask { Frame = new PCodeCallFrame { OwnerProcedure = 0 } };

        private static IlStepTransition Call(int target, int resumePc) =>
            new(IlStepKind.Call, resumePc, target, null,
                IlReturnDestination.Discard, null, null, null);

        private static IlStepTransition Ret(int resumePc) =>
            new(IlStepKind.Return, resumePc, 0, null,
                IlReturnDestination.Discard, null, null, null);

        [Fact]
        public void GoExit_CancelsPendingAwait()
        {
            // G5 §9: GO EXIT su un frame con await pending (timer/channel) deve CANCELLARE
            // l'op — i wake successivi troveranno l'op Cancelled (generation-safe, niente
            // resume fantasma dopo la terminazione del task).
            var task = NewTask();
            var driver = new IlGpmV2Driver();
            driver.Apply(task, Call(target: 10, resumePc: 5));
            var op = new IlTimerAwait(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() + 60_000);
            op.TryStart(task, driver.CurrentFrame!);
            driver.CurrentFrame!.Await = op;
            Assert.Equal(IlAwaitStatus.Pending, op.Status);

            Assert.Equal(IlGpmStepResult.Finished,
                driver.Apply(task, new IlStepTransition(IlStepKind.GoExit, 0, 0, null,
                    IlReturnDestination.Discard, null, null, null)));
            Assert.Equal(IlAwaitStatus.Cancelled, op.Status);
        }

        [Fact]
        public void SingleLevel_SubCall_Returns_And_CompletesAtRoot()
        {
            var task = NewTask();
            var driver = new IlGpmV2Driver();

            Assert.Equal(IlGpmStepResult.Continue,
                driver.Apply(task, Call(target: 10, resumePc: 5)));
            Assert.NotNull(driver.CurrentFrame);
            Assert.Equal(10, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(0, driver.CurrentFrame.Pc);
            Assert.NotNull(driver.CurrentFrame.Caller);
            Assert.Equal(5, driver.CurrentFrame.ReturnPc);

            Assert.Equal(IlGpmStepResult.Continue, driver.Apply(task, Ret(resumePc: 5)));
            Assert.Equal(0, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(5, driver.CurrentFrame.Pc);

            Assert.Equal(IlGpmStepResult.Finished,
                driver.Apply(task, new(IlStepKind.CompleteTask, 0, 0, null,
                    IlReturnDestination.Discard, null, null, null)));
            Assert.Null(driver.CurrentFrame);
        }

        [Fact]
        public void TwoLevelChain_PushesAndPops_InLifoOrder()
        {
            var task = NewTask();
            var driver = new IlGpmV2Driver();

            // root → A
            driver.Apply(task, Call(target: 20, resumePc: 3));
            // A → B
            driver.Apply(task, Call(target: 30, resumePc: 7));
            Assert.Equal(30, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(20, driver.CurrentFrame.Caller!.ProcedureId);
            Assert.Equal(0, driver.CurrentFrame.Caller!.Caller!.ProcedureId);

            // B rientra
            driver.Apply(task, Ret(resumePc: 7));
            Assert.Equal(20, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(7, driver.CurrentFrame.Pc);
            // A rientra
            driver.Apply(task, Ret(resumePc: 3));
            Assert.Equal(0, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(3, driver.CurrentFrame.Pc);
        }

        [Fact]
        public void ThreeLevelChain_AllFramesUnwindInOrder()
        {
            var task = NewTask();
            var driver = new IlGpmV2Driver();

            driver.Apply(task, Call(target: 20, resumePc: 1));
            driver.Apply(task, Call(target: 30, resumePc: 2));
            driver.Apply(task, Call(target: 40, resumePc: 3));
            Assert.Equal(40, driver.CurrentFrame!.ProcedureId);

            var expected = new[] { 40, 30, 20, 0 };
            foreach (var e in expected)
            {
                Assert.Equal(e, driver.CurrentFrame!.ProcedureId);
                driver.Apply(task, Ret(resumePc: 4));
            }

            Assert.Null(driver.CurrentFrame);
            Assert.Equal(IlGpmStepResult.Finished,
                driver.Apply(task, new(IlStepKind.CompleteTask, 0, 0, null,
                    IlReturnDestination.Discard, null, null, null)));
        }

        [Fact]
        public void Recursion_Depth3_UnwindsBackToRoot()
        {
            var task = NewTask();
            var driver = new IlGpmV2Driver();

            // root → R → R → R : tre frame annidati della stessa procedura (un'attivazione
            // per livello, ognuna con il proprio Caller).
            driver.Apply(task, Call(target: 50, resumePc: 9));
            driver.Apply(task, Call(target: 50, resumePc: 9));
            driver.Apply(task, Call(target: 50, resumePc: 9));

            var f = driver.CurrentFrame;
            Assert.NotNull(f);
            var depth = 0;
            for (var n = f; n is not null; n = n.Caller) depth++;
            Assert.Equal(4, depth); // R(3) + root(1)
            Assert.All(new[] { 50, 50, 50 }, _ => { });

            for (var i = 0; i < 3; i++)
                driver.Apply(task, Ret(resumePc: 9));
            Assert.Equal(0, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(9, driver.CurrentFrame.Pc);
        }

        [Fact]
        public void Return_AtRoot_CompletesImplicitly()
        {
            var task = NewTask();
            var driver = new IlGpmV2Driver();

            driver.Apply(task, Call(target: 60, resumePc: 2));
            driver.Apply(task, Ret(resumePc: 2));
            // Return della root (senza CompleteTask esplicito) = Finished per il piano §5.
            Assert.Equal(IlGpmStepResult.Finished, driver.Apply(task, Ret(resumePc: 2)));
            Assert.Null(driver.CurrentFrame);
        }

        [Fact]
        public void Destinations_Local_WritesReturnValueToCallerSlot()
        {
            var task = NewTask();
            var driver = new IlGpmV2Driver();
            driver.CurrentFrame!.Locals = new object?[8];

            // G4: la destinazione (Local + ReturnSlot) è decisa alla CALL e salvata sul frame
            // del callee; il Return del callee scrive il valore nel local del caller.
            var callWithDest = new IlStepTransition(IlStepKind.Call, 2, 60, null,
                IlReturnDestination.Local, null, null, null) { ReturnSlot = 3 };
            driver.Apply(task, callWithDest);
            // il caller (root) ha Locals pronti; il callee ha la destinazione.
            Assert.Equal(60, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(IlReturnDestination.Local, driver.CurrentFrame.ReturnDestination);
            Assert.Equal(3, driver.CurrentFrame.ReturnSlot);

            var withValue = new IlStepTransition(IlStepKind.Return, 2, 0, null,
                IlReturnDestination.Discard, 42, null, null);
            Assert.Equal(IlGpmStepResult.Continue, driver.Apply(task, withValue));
            // il caller è di nuovo la root; il valore 42 è nel suo Locals[3].
            Assert.Equal(0, driver.CurrentFrame!.ProcedureId);
            Assert.Equal(42, driver.CurrentFrame.Locals![3]);
        }
    }
}
