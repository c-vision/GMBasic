/* ============================================================================
 * gmb_pt_load.hpp — lettore di checkpoint PyTorch (`torch.save`, formato zip) SENZA Python.
 *
 * Legge `<archivio>/data.pkl` con un interprete pickle minimale (solo i codici che servono a uno
 * state dict: OrderedDict, tuple, liste, interi, stringhe, torch._utils._rebuild_tensor_v2, storage
 * persistenti) e le storage `<archivio>/data/<n>` con caffe2::serialize::PyTorchStreamReader.
 * NON esegue codice del pickle: ogni GLOBAL sconosciuto e' un errore (sicuro su file non fidati).
 *
 * Perche' non `torch::jit::readArchiveAndTensors`: rifiuta i `torch.save` di state dict con l'attributo
 * `_metadata` sull'OrderedDict (BUILD), che e' il caso dei checkpoint dei tokenizer NCM.
 *
 * Uso:
 *   std::map<std::string, at::Tensor> sd = gmb::pt_load("generator.pt");        // chiave "model" se presente
 *   std::map<std::string, at::Tensor> sd = gmb::pt_load("tok.pt", "model");     // sotto-dizionario scelto
 * I tensori sono su CPU, con dtype e forma del file (le view condividono la storage).
 * ============================================================================ */
#pragma once

#include <torch/torch.h>
#include <caffe2/serialize/inline_container.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gmb {

namespace ptl {

struct Val;
using VP = std::shared_ptr<Val>;

struct StorageRef { std::string key; std::string dtype; };
struct TensorDesc { StorageRef st; int64_t offset = 0; std::vector<int64_t> size, stride; };

struct Val {
    enum K { NONE, BOOL, INT, FLOAT, STR, TUPLE, LIST, DICT, GLOBAL, STORAGE, TENSOR, OBJ } k = NONE;
    int64_t i = 0;
    double f = 0;
    std::string s;                                   /* STR / GLOBAL ("modulo.nome") */
    std::vector<VP> items;                           /* TUPLE / LIST */
    std::vector<std::pair<VP, VP>> dict;             /* DICT (ordine di inserimento) */
    StorageRef st;                                   /* STORAGE */
    TensorDesc t;                                    /* TENSOR */
};

inline VP mk(Val::K k) { auto v = std::make_shared<Val>(); v->k = k; return v; }

inline at::ScalarType dtype_of(const std::string& storage_class)
{
    if (storage_class == "FloatStorage")    return at::kFloat;
    if (storage_class == "DoubleStorage")   return at::kDouble;
    if (storage_class == "HalfStorage")     return at::kHalf;
    if (storage_class == "BFloat16Storage") return at::kBFloat16;
    if (storage_class == "LongStorage")     return at::kLong;
    if (storage_class == "IntStorage")      return at::kInt;
    if (storage_class == "ShortStorage")    return at::kShort;
    if (storage_class == "CharStorage")     return at::kChar;
    if (storage_class == "ByteStorage")     return at::kByte;
    if (storage_class == "BoolStorage")     return at::kBool;
    throw std::runtime_error("pt_load: storage non supportata: " + storage_class);
}

class Reader {
public:
    explicit Reader(const std::string& path) : zip_(path)
    {
        /* il prefisso dell'archivio e' la cartella radice dello zip: PyTorchStreamReader lo deduce */
    }

    VP parse()
    {
        auto [data, size] = zip_.getRecord("data.pkl");
        const uint8_t* p = static_cast<const uint8_t*>(data.get());
        const uint8_t* end = p + size;
        std::vector<VP> st;                    /* stack */
        std::vector<size_t> marks;
        std::map<int64_t, VP> memo;
        auto need = [&](size_t n) { if ((size_t)(end - p) < n) throw std::runtime_error("pt_load: pickle troncato"); };
        auto u8 = [&]() { need(1); return *p++; };
        auto u16 = [&]() { need(2); uint16_t v = p[0] | (p[1] << 8); p += 2; return (uint32_t)v; };
        auto u32 = [&]() { need(4); uint32_t v = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); p += 4; return v; };
        auto str = [&](size_t n) { need(n); std::string s((const char*)p, n); p += n; return s; };
        auto line = [&]() { std::string s; while (true) { need(1); char c = (char)*p++; if (c == '\n') break; s += c; } return s; };
        auto pop = [&]() { if (st.empty()) throw std::runtime_error("pt_load: stack vuoto"); VP v = st.back(); st.pop_back(); return v; };
        auto popmark = [&]() { if (marks.empty()) throw std::runtime_error("pt_load: MARK mancante"); size_t m = marks.back(); marks.pop_back(); return m; };

        while (true)
        {
            uint8_t op = u8();
            switch (op)
            {
            case 0x80: u8(); break;                                                /* PROTO */
            case 0x95: need(8); p += 8; break;                                     /* FRAME */
            case '.': return st.empty() ? mk(Val::NONE) : st.back();               /* STOP */
            case 'N': st.push_back(mk(Val::NONE)); break;
            case 0x88: { auto v = mk(Val::BOOL); v->i = 1; st.push_back(v); break; }
            case 0x89: { auto v = mk(Val::BOOL); v->i = 0; st.push_back(v); break; }
            case 'K': { auto v = mk(Val::INT); v->i = u8(); st.push_back(v); break; }
            case 'M': { auto v = mk(Val::INT); v->i = u16(); st.push_back(v); break; }
            case 'J': { auto v = mk(Val::INT); v->i = (int32_t)u32(); st.push_back(v); break; }
            case 0x8a: { auto v = mk(Val::INT); uint8_t n = u8(); int64_t x = 0; need(n);
                         for (int b = 0; b < n && b < 8; b++) x |= (int64_t)p[b] << (8 * b);
                         if (n > 0 && n < 8 && (p[n - 1] & 0x80)) x |= -((int64_t)1 << (8 * n));
                         p += n; v->i = x; st.push_back(v); break; }               /* LONG1 */
            case 'G': { need(8); uint64_t b = 0; for (int k = 0; k < 8; k++) b = (b << 8) | p[k]; p += 8;
                        auto v = mk(Val::FLOAT); double d; std::memcpy(&d, &b, 8); v->f = d; st.push_back(v); break; }
            case 'X': { auto v = mk(Val::STR); v->s = str(u32()); st.push_back(v); break; }            /* BINUNICODE */
            case 0x8c: { auto v = mk(Val::STR); v->s = str(u8()); st.push_back(v); break; }            /* SHORT_BINUNICODE */
            case 'U': { auto v = mk(Val::STR); v->s = str(u8()); st.push_back(v); break; }             /* SHORT_BINSTRING */
            case 'c': { auto v = mk(Val::GLOBAL); std::string m = line(); std::string n = line(); v->s = m + "." + n; st.push_back(v); break; }
            case 0x93: { VP n = pop(), m = pop(); auto v = mk(Val::GLOBAL); v->s = m->s + "." + n->s; st.push_back(v); break; }  /* STACK_GLOBAL */
            case ')': st.push_back(mk(Val::TUPLE)); break;
            case ']': st.push_back(mk(Val::LIST)); break;
            case '}': st.push_back(mk(Val::DICT)); break;
            case '(': marks.push_back(st.size()); break;
            case 't': { size_t m = popmark(); auto v = mk(Val::TUPLE); v->items.assign(st.begin() + m, st.end()); st.resize(m); st.push_back(v); break; }
            case 0x85: { auto v = mk(Val::TUPLE); VP a = pop(); v->items = { a }; st.push_back(v); break; }
            case 0x86: { auto v = mk(Val::TUPLE); VP b = pop(), a = pop(); v->items = { a, b }; st.push_back(v); break; }
            case 0x87: { auto v = mk(Val::TUPLE); VP c = pop(), b = pop(), a = pop(); v->items = { a, b, c }; st.push_back(v); break; }
            case 'q': { int64_t k = u8(); if (st.empty()) throw std::runtime_error("pt_load: BINPUT su stack vuoto"); memo[k] = st.back(); break; }
            case 'r': { int64_t k = u32(); memo[k] = st.back(); break; }
            case 0x94: { int64_t k = (int64_t)memo.size(); memo[k] = st.back(); break; }                /* MEMOIZE */
            case 'h': { int64_t k = u8(); auto it = memo.find(k); if (it == memo.end()) throw std::runtime_error("pt_load: BINGET sconosciuto"); st.push_back(it->second); break; }
            case 'j': { int64_t k = u32(); auto it = memo.find(k); if (it == memo.end()) throw std::runtime_error("pt_load: LONG_BINGET sconosciuto"); st.push_back(it->second); break; }
            case 's': { VP v = pop(), k = pop(); st.back()->dict.emplace_back(k, v); break; }           /* SETITEM */
            case 'u': { size_t m = popmark(); VP d = st[m - 1]; for (size_t j = m; j + 1 < st.size(); j += 2) d->dict.emplace_back(st[j], st[j + 1]); st.resize(m); break; }
            case 'a': { VP v = pop(); st.back()->items.push_back(v); break; }                            /* APPEND */
            case 'e': { size_t m = popmark(); VP l = st[m - 1]; for (size_t j = m; j < st.size(); j++) l->items.push_back(st[j]); st.resize(m); break; }
            case 'b': { pop(); break; }                                                                  /* BUILD: attributi (_metadata) ignorati */
            case 'Q': { VP pid = pop();                                                                  /* BINPERSID: ('storage', cls, key, loc, numel) */
                        if (pid->k != Val::TUPLE || pid->items.size() < 3 || pid->items[0]->s != "storage")
                            throw std::runtime_error("pt_load: persistent id non supportato");
                        auto v = mk(Val::STORAGE);
                        std::string cls = pid->items[1]->s; size_t dot = cls.rfind('.'); v->st.dtype = dot == std::string::npos ? cls : cls.substr(dot + 1);
                        v->st.key = pid->items[2]->s; st.push_back(v); break; }
            case 'R': { VP args = pop(), fn = pop();                                                     /* REDUCE */
                        if (fn->k != Val::GLOBAL) throw std::runtime_error("pt_load: REDUCE su oggetto non globale");
                        if (fn->s == "collections.OrderedDict") { st.push_back(mk(Val::DICT)); break; }
                        if (fn->s == "torch._utils._rebuild_tensor_v2" || fn->s == "torch._utils._rebuild_tensor")
                        {
                            if (args->items.size() < 4 || args->items[0]->k != Val::STORAGE) throw std::runtime_error("pt_load: _rebuild_tensor_v2 argomenti inattesi");
                            auto t = mk(Val::TENSOR); t->t.st = args->items[0]->st; t->t.offset = args->items[1]->i;
                            for (auto& e : args->items[2]->items) t->t.size.push_back(e->i);
                            for (auto& e : args->items[3]->items) t->t.stride.push_back(e->i);
                            st.push_back(t); break;
                        }
                        throw std::runtime_error("pt_load: callable non ammesso nel pickle: " + fn->s); }
            default: throw std::runtime_error("pt_load: opcode pickle non supportato: " + std::to_string((int)op));
            }
        }
    }

    at::Tensor tensor(const TensorDesc& d)
    {
        auto it = storages_.find(d.st.key);
        if (it == storages_.end())
        {
            auto [data, size] = zip_.getRecord("data/" + d.st.key);
            auto sp = std::make_shared<std::pair<at::DataPtr, size_t>>(std::move(data), size);
            it = storages_.emplace(d.st.key, sp).first;
        }
        const at::ScalarType dt = dtype_of(d.st.dtype);
        auto holder = it->second;
        at::Tensor base = at::from_blob(holder->first.get(), { (int64_t)(holder->second / c10::elementSize(dt)) },
                                        [holder](void*) {}, at::TensorOptions().dtype(dt));
        return base.as_strided(d.size, d.stride, d.offset).clone();      /* clone: indipendente dalla storage dello zip */
    }

private:
    caffe2::serialize::PyTorchStreamReader zip_;
    std::map<std::string, std::shared_ptr<std::pair<at::DataPtr, size_t>>> storages_;
};

inline void collect(Reader& r, const VP& v, const std::string& prefix, std::map<std::string, at::Tensor>& out)
{
    if (v->k == Val::TENSOR) { out[prefix] = r.tensor(v->t); return; }
    if (v->k == Val::DICT)
        for (auto& kv : v->dict)
            if (kv.first->k == Val::STR) collect(r, kv.second, prefix.empty() ? kv.first->s : prefix + "." + kv.first->s, out);
}

} /* namespace ptl */

/* Carica uno state dict. `key` (es. "model") seleziona il sotto-dizionario; vuoto = radice,
 * e se la radice ha una sola chiave "model" la usa. Ritorna nome -> tensore (CPU). */
inline std::map<std::string, at::Tensor> pt_load(const std::string& path, const std::string& key = "")
{
    ptl::Reader r(path);
    ptl::VP root = r.parse();
    ptl::VP sel = root;
    if (root->k == ptl::Val::DICT)
    {
        for (auto& kv : root->dict)
            if (kv.first->k == ptl::Val::STR && (key.empty() ? kv.first->s == "model" : kv.first->s == key)) { sel = kv.second; break; }
    }
    std::map<std::string, at::Tensor> out;
    ptl::collect(r, sel, "", out);
    return out;
}

} /* namespace gmb */
