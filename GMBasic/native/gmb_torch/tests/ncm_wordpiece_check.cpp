/* ncm_wordpiece_check.cpp — F0.3 del piano NCM: tokenizer di testo in ingresso (BERT WordPiece uncased) tramite LLMToken.
 * Verifica id noti di bert-base-uncased, rimozione degli accenti, troncamento che preserva [CLS]/[SEP] e padding con 0.
 * Build: clang++ -std=c++17 -O2 -I ../src -I <torch>/include -I <torch>/include/torch/csrc/api/include ncm_wordpiece_check.cpp \
 *        -L <torch>/lib -ltorch -ltorch_cpu -lc10 -Wl,-rpath,<torch>/lib -o ncm_wordpiece_check
 * Uso:   ncm_wordpiece_check <vocab.txt di bert-base-uncased>     (libllmtoken deve essere in ~/.gmbasic/libs o accanto alla libreria) */
#include "../src/gmb_ncm.hpp"
#include <cstdio>
static int g_fail = 0;
static void check(const char* what, const std::vector<int>& got, const std::vector<int>& want)
{
    bool ok = got == want;
    std::printf("%s %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) { g_fail++; std::printf("  atteso:"); for (int i : want) std::printf(" %d", i); std::printf("\n  ottenuto:"); for (int i : got) std::printf(" %d", i); std::printf("\n"); }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc < 2) { std::printf("uso: ncm_wordpiece_check <vocab.txt>\n"); return 2; }
        gmb::WordPiece wp; wp.load(argv[1]);
        check("Hello, World! It's a test.", wp.encode("Hello, World! It's a test.", 14), {101, 7592, 1010, 2088, 999, 2009, 1005, 1055, 1037, 3231, 1012, 102, 0, 0});
        check("accenti e maiuscole rimossi (Cafe = café)", wp.encode("Cafe", 4), wp.encode("café", 4));
        check("troncamento: [CLS] e [SEP] restano", wp.encode("a b c d e f g h i j", 6), {101, 1037, 1038, 1039, 1040, 102});
        check("testo vuoto", wp.encode("", 4), {101, 102, 0, 0});
    }
    catch (const std::exception& e) { std::printf("ERRORE: %s\n", e.what()); return 1; }
    std::printf(g_fail ? "FALLITO (%d)\n" : "TUTTO OK\n", g_fail);
    return g_fail ? 1 : 0;
}
