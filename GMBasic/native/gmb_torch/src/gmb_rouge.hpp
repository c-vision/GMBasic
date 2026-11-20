/* ============================================================================
 * gmb_rouge.hpp — ROUGE-1 / ROUGE-2 / ROUGE-L (F-measure) in C++ puro, senza Python.
 *
 * Riproduce il tokenizer di rouge_score con use_stemmer=True: minuscolo, ogni sequenza non [a-z0-9] diventa spazio,
 * stemming Porter per i token di piu' di 3 lettere. ROUGE-N = overlap di n-grammi con conteggi (min), ROUGE-L = LCS sui token
 * (variante "rougeL", non "rougeLsum"); F = 2PR/(P+R).
 * Lo stemmer e' il Porter classico (Martin Porter, con le varianti bli/logi del suo sito); lo stemmer di NLTK usato da rouge_score
 * (modalita' NLTK_EXTENSIONS) si discosta su poche parole irregolari: il punteggio puo' differire di pochi centesimi di punto.
 * ============================================================================ */
#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace gmb {
namespace rouge_detail {

struct Porter {
    std::string b; int k = 0, j = 0;
    bool cons(int i) const
    {
        switch (b[i]) { case 'a': case 'e': case 'i': case 'o': case 'u': return false; case 'y': return i == 0 ? true : !cons(i - 1); default: return true; }
    }
    int m() const
    {
        int n = 0, i = 0;
        for (;;) { if (i > j) return n; if (!cons(i)) break; i++; }
        i++;
        for (;;)
        {
            for (;;) { if (i > j) return n; if (cons(i)) break; i++; }
            i++; n++;
            for (;;) { if (i > j) return n; if (!cons(i)) break; i++; }
            i++;
        }
    }
    bool vowelinstem() const { for (int i = 0; i <= j; i++) if (!cons(i)) return true; return false; }
    bool doublec(int i) const { return i >= 1 && b[i] == b[i - 1] && cons(i); }
    bool cvc(int i) const
    {
        if (i < 2 || !cons(i) || cons(i - 1) || !cons(i - 2)) return false;
        char ch = b[i]; return !(ch == 'w' || ch == 'x' || ch == 'y');
    }
    bool ends(const char* s)
    {
        int l = (int)std::string(s).size();
        if (l > k + 1) return false;
        if (b.compare(k - l + 1, l, s) != 0) return false;
        j = k - l; return true;
    }
    void setto(const char* s) { int l = (int)std::string(s).size(); b.replace(j + 1, k - j, s); k = j + l; }
    void r(const char* s) { if (m() > 0) setto(s); }
    void step1ab()
    {
        if (b[k] == 's')
        {
            if (ends("sses")) k -= 2; else if (ends("ies")) setto("i"); else if (b[k - 1] != 's') k--;
        }
        if (ends("eed")) { if (m() > 0) k--; }
        else if ((ends("ed") || ends("ing")) && vowelinstem())
        {
            k = j;
            if (ends("at")) setto("ate"); else if (ends("bl")) setto("ble"); else if (ends("iz")) setto("ize");
            else if (doublec(k)) { k--; char ch = b[k]; if (ch == 'l' || ch == 's' || ch == 'z') k++; }
            else if (m() == 1 && cvc(k)) setto("e");
        }
    }
    void step1c() { if (ends("y") && vowelinstem()) b[k] = 'i'; }
    void step2()
    {
        if (k < 1) return;
        switch (b[k - 1])
        {
        case 'a': if (ends("ational")) { r("ate"); break; } if (ends("tional")) { r("tion"); break; } break;
        case 'c': if (ends("enci")) { r("ence"); break; } if (ends("anci")) { r("ance"); break; } break;
        case 'e': if (ends("izer")) { r("ize"); break; } break;
        case 'l': if (ends("bli")) { r("ble"); break; } if (ends("alli")) { r("al"); break; } if (ends("entli")) { r("ent"); break; }
                  if (ends("eli")) { r("e"); break; } if (ends("ousli")) { r("ous"); break; } break;
        case 'o': if (ends("ization")) { r("ize"); break; } if (ends("ation")) { r("ate"); break; } if (ends("ator")) { r("ate"); break; } break;
        case 's': if (ends("alism")) { r("al"); break; } if (ends("iveness")) { r("ive"); break; } if (ends("fulness")) { r("ful"); break; }
                  if (ends("ousness")) { r("ous"); break; } break;
        case 't': if (ends("aliti")) { r("al"); break; } if (ends("iviti")) { r("ive"); break; } if (ends("biliti")) { r("ble"); break; } break;
        case 'g': if (ends("logi")) { r("log"); break; } break;
        }
    }
    void step3()
    {
        switch (b[k])
        {
        case 'e': if (ends("icate")) { r("ic"); break; } if (ends("ative")) { r(""); break; } if (ends("alize")) { r("al"); break; } break;
        case 'i': if (ends("iciti")) { r("ic"); break; } break;
        case 'l': if (ends("ical")) { r("ic"); break; } if (ends("ful")) { r(""); break; } break;
        case 's': if (ends("ness")) { r(""); break; } break;
        }
    }
    void step4()
    {
        if (k < 1) return;
        switch (b[k - 1])
        {
        case 'a': if (ends("al")) break; return;
        case 'c': if (ends("ance")) break; if (ends("ence")) break; return;
        case 'e': if (ends("er")) break; return;
        case 'i': if (ends("ic")) break; return;
        case 'l': if (ends("able")) break; if (ends("ible")) break; return;
        case 'n': if (ends("ant")) break; if (ends("ement")) break; if (ends("ment")) break; if (ends("ent")) break; return;
        case 'o': if (ends("ion") && j >= 0 && (b[j] == 's' || b[j] == 't')) break; if (ends("ou")) break; return;
        case 's': if (ends("ism")) break; return;
        case 't': if (ends("ate")) break; if (ends("iti")) break; return;
        case 'u': if (ends("ous")) break; return;
        case 'v': if (ends("ive")) break; return;
        case 'z': if (ends("ize")) break; return;
        default: return;
        }
        if (m() > 1) k = j;
    }
    void step5()
    {
        j = k;
        if (b[k] == 'e') { int a = m(); if (a > 1 || (a == 1 && !cvc(k - 1))) k--; }
        if (b[k] == 'l' && doublec(k) && m() > 1) k--;
    }
    std::string stem(const std::string& w)
    {
        if (w.size() <= 2) return w;
        b = w; k = (int)w.size() - 1; j = 0;
        step1ab(); if (k > 0) { step1c(); step2(); step3(); step4(); step5(); }
        return b.substr(0, (size_t)k + 1);
    }
};

inline std::vector<std::string> tokenize(const std::string& text)
{
    std::string s; s.reserve(text.size());
    for (unsigned char c : text) s += (c >= 'A' && c <= 'Z') ? (char)(c + 32) : ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) ? (char)c : ' ';
    std::vector<std::string> out; Porter p;
    for (size_t i = 0; i < s.size();)
    {
        if (s[i] == ' ') { i++; continue; }
        size_t e = i; while (e < s.size() && s[e] != ' ') e++;
        std::string w = s.substr(i, e - i);
        out.push_back(w.size() > 3 ? p.stem(w) : w);
        i = e;
    }
    return out;
}

inline double fscore(double overlap, double nh, double nr)
{
    if (nh == 0 || nr == 0 || overlap == 0) return 0.0;
    double p = overlap / nh, r = overlap / nr; return 2 * p * r / (p + r);
}

inline double rouge_n(const std::vector<std::string>& ref, const std::vector<std::string>& hyp, int n)
{
    auto grams = [&](const std::vector<std::string>& t) {
        std::map<std::vector<std::string>, int> m; double tot = 0;
        for (size_t i = 0; i + n <= t.size(); i++) { m[std::vector<std::string>(t.begin() + i, t.begin() + i + n)]++; tot++; }
        return std::make_pair(m, tot); };
    auto [rm, rt] = grams(ref); auto [hm, ht] = grams(hyp);
    double ov = 0; for (auto& kv : hm) { auto it = rm.find(kv.first); if (it != rm.end()) ov += std::min(kv.second, it->second); }
    return fscore(ov, ht, rt);
}

inline double rouge_l(const std::vector<std::string>& ref, const std::vector<std::string>& hyp)
{
    if (ref.empty() || hyp.empty()) return 0.0;
    std::vector<std::vector<int>> d(ref.size() + 1, std::vector<int>(hyp.size() + 1, 0));
    for (size_t i = 1; i <= ref.size(); i++)
        for (size_t jx = 1; jx <= hyp.size(); jx++)
            d[i][jx] = ref[i - 1] == hyp[jx - 1] ? d[i - 1][jx - 1] + 1 : std::max(d[i - 1][jx], d[i][jx - 1]);
    return fscore(d[ref.size()][hyp.size()], (double)hyp.size(), (double)ref.size());
}

} /* namespace rouge_detail */

struct RougeScores { double r1 = 0, r2 = 0, rl = 0; };

/* ROUGE F-measure di una coppia (riferimento, ipotesi), in [0,1]. */
inline RougeScores rouge_score(const std::string& reference, const std::string& hypothesis)
{
    auto r = rouge_detail::tokenize(reference), h = rouge_detail::tokenize(hypothesis);
    return { rouge_detail::rouge_n(r, h, 1), rouge_detail::rouge_n(r, h, 2), rouge_detail::rouge_l(r, h) };
}

} /* namespace gmb */
