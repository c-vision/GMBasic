/* bleu_check.cpp — verifiche di gmb_bleu.hpp su casi con valore noto (sacreBLEU 13a). Uso: bleu_check (nessun argomento, exit 0 = ok) */
#include "../src/gmb_bleu.hpp"
#include <cstdio>
#include <cmath>
static int fails = 0;
static void ck(bool ok, const char* what) { std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what); if (!ok) fails++; }
int main()
{
    using namespace gmb;
    { auto w = bleu_detail::words(bleu_detail::tokenize_13a("Hello, world! It's 3.14, ok.")); ck(w.size() == 9 && w[0] == "Hello" && w[1] == "," && w[4] == "It's" && w[5] == "3.14" && w[8] == ".", "13a: Hello , world ! It's 3.14 , ok . (l'apostrofo non si separa, 3.14 intatto)"); }
    { Bleu b; b.add("the cat sat on the mat today", "the cat sat on the mat today"); ck(std::fabs(b.score().bleu - 100.0) < 1e-9, "identico -> 100"); }
    { Bleu b; b.add("a b c d e f g h", "x y z w"); ck(b.score().bleu == 0.0, "nessun n-gramma in comune -> 0"); }
    /* esempio del paper originale / documentazione sacreBLEU: p1=8/9? calcolo a mano: hyp "the the the the the the the" vs ref "the cat is on the mat": clip -> 2/7 unigrammi, 0 bigrammi => BLEU 0 senza smoothing */
    { Bleu b; b.add("the cat is on the mat", "the the the the the the the"); auto s = b.score(); ck(std::fabs(s.p[0] - 2.0 / 7.0) < 1e-12 && s.bleu == 0.0, "clipping: 2/7 unigrammi"); }
    /* caso con valore calcolato a mano: ref "the quick brown fox jumps over the lazy dog", hyp "the quick brown fox jumped over the lazy dog": 8/9, 6/8, 4/7, 2/6; BP=1 */
    { Bleu b; b.add("the quick brown fox jumps over the lazy dog", "the quick brown fox jumped over the lazy dog"); auto s = b.score();
      const double e = 100.0 * std::exp((std::log(8.0 / 9) + std::log(6.0 / 8) + std::log(4.0 / 7) + std::log(2.0 / 6)) / 4.0);
      ck(std::fabs(s.bleu - e) < 1e-9, "valore calcolato a mano (8/9,6/8,4/7,2/6: 4 n-grammi su 8, 7, 6 ecc.)"); std::printf("     BLEU = %.4f (atteso %.4f)\n", s.bleu, e); }
    /* brevity penalty: ipotesi piu corta */
    { Bleu b; b.add("a b c d e f g h i j", "a b c d e"); auto s = b.score(); ck(std::fabs(s.bp - std::exp(1.0 - 10.0 / 5.0)) < 1e-12, "brevity penalty exp(1 - r/c)"); }
    std::printf(fails ? "FALLITO: %d\n" : "tutto ok\n", fails);
    return fails ? 1 : 0;
}
