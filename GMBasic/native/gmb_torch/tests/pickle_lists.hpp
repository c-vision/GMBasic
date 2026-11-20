/* pickle_lists.hpp — lettore minimo di pickle (protocolli 2-5): liste di int e liste di str (dataset di HazyResearch/ncm-datasets). */
#pragma once
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace pkl {
struct PV { int kind = 0; int64_t i = 0; std::string s; std::vector<std::shared_ptr<PV>> items; };   /* 0=int 1=str 2=list */
using PP = std::shared_ptr<PV>;

inline PP read(const std::string& path)
{
    std::ifstream f(path, std::ios::binary); if (!f) throw std::runtime_error("pickle non leggibile: " + path);
    std::string buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const unsigned char* p = (const unsigned char*)buf.data(); const unsigned char* e = p + buf.size();
    std::vector<PP> st; std::vector<size_t> marks; std::map<int64_t, PP> memo; int64_t memo_n = 0;
    auto u = [&](int n) { uint64_t x = 0; for (int k = 0; k < n; k++) x |= (uint64_t)p[k] << (8 * k); p += n; return x; };
    auto mk = [](int kind) { auto v = std::make_shared<PV>(); v->kind = kind; return v; };
    while (p < e)
    {
        unsigned char op = *p++;
        switch (op)
        {
        case 0x80: p += 1; break;
        case 0x95: p += 8; break;
        case ']': st.push_back(mk(2)); break;
        case '(': marks.push_back(st.size()); break;
        case 'K': { auto v = mk(0); v->i = (int64_t)u(1); st.push_back(v); break; }
        case 'M': { auto v = mk(0); v->i = (int64_t)u(2); st.push_back(v); break; }
        case 'J': { auto v = mk(0); v->i = (int32_t)u(4); st.push_back(v); break; }
        case 0x8c: { auto v = mk(1); size_t n = (size_t)u(1); v->s.assign((const char*)p, n); p += n; st.push_back(v); break; }
        case 'X': { auto v = mk(1); size_t n = (size_t)u(4); v->s.assign((const char*)p, n); p += n; st.push_back(v); break; }
        case 0x8d: { auto v = mk(1); size_t n = (size_t)u(8); v->s.assign((const char*)p, n); p += n; st.push_back(v); break; }
        case 0x94: memo[memo_n++] = st.back(); break;
        case 'q': memo[(int64_t)u(1)] = st.back(); break;
        case 'r': memo[(int64_t)u(4)] = st.back(); break;
        case 'h': st.push_back(memo.at((int64_t)u(1))); break;
        case 'j': st.push_back(memo.at((int64_t)u(4))); break;
        case 'a': { PP v = st.back(); st.pop_back(); st.back()->items.push_back(v); break; }
        case 'e': { size_t m = marks.back(); marks.pop_back(); PP l = st[m - 1]; for (size_t i = m; i < st.size(); i++) l->items.push_back(st[i]); st.resize(m); break; }
        case '.': return st.back();
        default: throw std::runtime_error("pickle: opcode non supportato " + std::to_string((int)op));
        }
    }
    throw std::runtime_error("pickle: STOP mancante");
}
} /* namespace pkl */
