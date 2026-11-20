/* ncm_speed_profile.cpp — dove va il tempo di NCM::generate_ids (F5): generatore a livelli / codes_to_zhat / decoder VQ-VAE / pick_tokens.
 * Uso: ncm_speed_profile <dir NCM> <vocab.txt> [cpu|mps=mps] [batch=64] [rip=3] */
#include "../src/gmb_ncm.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void sy(const at::Tensor& t) { (void)t.flatten().slice(0, 0, 1).cpu(); }   /* sincronizza il dispositivo */
int main(int argc, char** argv)
{
    try
    {
        if (argc < 3) { std::printf("uso: ncm_speed_profile <dirNCM> <vocab.txt> [cpu|mps] [batch] [rip]\n"); return 2; }
        const bool mps = !(argc > 3 && std::string(argv[3]) == "cpu");
        const int B = argc > 4 ? atoi(argv[4]) : 64, rip = argc > 5 ? atoi(argv[5]) : 3;
        c10::Device dev = mps ? c10::Device(c10::kMPS) : c10::Device(c10::kCPU);
        gmb::Ncm m; m.load(std::string(argv[1]) + "/lm1b/small/generator.pt", std::string(argv[1]) + "/lm1b/tokenizer.pt", argv[2], dev);
        gmb::NcmOptions o;
        double a = 0, b = 0, c = 0, d = 0;
        for (int r = -1; r < rip; r++)
        {
            torch::manual_seed(42 + r);
            double t0 = now(); at::Tensor codes = m.generate_codes(B, o); sy(codes);
            double t1 = now(); at::Tensor z = m.codes_to_zhat(codes); sy(z);
            double t2 = now(); at::Tensor lg = m.decode_logits(z); sy(lg);
            double t3 = now(); at::Tensor ids = m.pick_tokens(lg, o); sy(ids);
            double t4 = now();
            if (r >= 0) { a += t1 - t0; b += t2 - t1; c += t3 - t2; d += t4 - t3; }
            (void)ids;
        }
        a /= rip; b /= rip; c /= rip; d /= rip; const double tot = a + b + c + d;
        std::printf("%s batch=%d  (media di %d)\n generatore a livelli : %7.3f s  %4.1f%%\n codes_to_zhat        : %7.3f s  %4.1f%%\n decoder (logits)     : %7.3f s  %4.1f%%\n pick_tokens          : %7.3f s  %4.1f%%\n totale               : %7.3f s\n",
                    mps ? "mps" : "cpu", B, rip, a, 100 * a / tot, b, 100 * b / tot, c, 100 * c / tot, d, 100 * d / tot, tot);
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
}
