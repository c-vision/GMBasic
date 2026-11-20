/* gmb_pt_save.hpp — scrittore `torch.save` senza Python per checkpoint NCM: {"model": {nome: tensore}, "inference": {...}}.
 * Stesso formato di tools/ncm_make_tiny.cpp (zip con data.pkl protocollo 2 + data/<n>), leggibile da pt_load / gmb_pt_load.hpp.
 * I tensori sono scritti float32 (o long) contigui su CPU. Richiede <caffe2/serialize/inline_container.h> (libtorch). */
#pragma once
#include <torch/torch.h>
#include <caffe2/serialize/inline_container.h>
#include <map>
#include <string>
#include <vector>

namespace gmb {
namespace ptsave {

struct Pk {
    std::string b;
    void op(char c) { b += c; }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) b += (char)((v >> (8 * i)) & 0xFF); }
    void str(const std::string& s) { op('X'); u32((uint32_t)s.size()); b += s; }
    void integer(int64_t v)
    {
        if (v >= 0 && v < 256) { op('K'); b += (char)v; }
        else if (v >= 0 && v < 65536) { op('M'); b += (char)(v & 0xFF); b += (char)(v >> 8); }
        else { op('J'); u32((uint32_t)(int32_t)v); }
    }
    void global(const std::string& m, const std::string& n) { op('c'); b += m + "\n" + n + "\n"; }
};

using Dict = std::map<std::string, at::Tensor>;

inline void write(const std::string& path, const std::vector<std::pair<std::string, const Dict*>>& top)
{
    Pk p; std::vector<at::Tensor> st;
    auto tensor = [&](const at::Tensor& t0) {
        at::Tensor t = t0.detach().to(at::kCPU).contiguous();
        if (t.is_floating_point()) t = t.to(at::kFloat);
        const int key = (int)st.size(); st.push_back(t);
        const bool isint = t.scalar_type() == at::kLong;
        p.global("torch._utils", "_rebuild_tensor_v2"); p.op('(');
        p.op('('); p.str("storage"); p.global("torch", isint ? "LongStorage" : "FloatStorage"); p.str(std::to_string(key)); p.str("cpu"); p.integer(t.numel()); p.op('t'); p.op('Q');
        p.integer(0);
        p.op('('); for (int64_t i = 0; i < t.dim(); i++) p.integer(t.size(i)); p.op('t');
        p.op('('); for (int64_t i = 0; i < t.dim(); i++) p.integer(t.stride(i)); p.op('t');
        p.op((char)0x89);
        p.global("collections", "OrderedDict"); p.op(')'); p.op('R');
        p.op('t'); p.op('R');
    };
    p.op((char)0x80); p.op((char)2);
    p.op('}'); p.op('(');
    for (auto& kv : top)
    {
        p.str(kv.first);
        p.global("collections", "OrderedDict"); p.op(')'); p.op('R'); p.op('(');
        for (auto& e : *kv.second) { p.str(e.first); tensor(e.second); }
        p.op('u');
    }
    p.op('u'); p.op('.');
    caffe2::serialize::PyTorchStreamWriter w(path);
    w.writeRecord("data.pkl", p.b.data(), p.b.size());
    for (size_t i = 0; i < st.size(); i++) w.writeRecord("data/" + std::to_string(i), st[i].data_ptr(), st[i].nbytes());
    w.writeEndOfFile();
}

} /* namespace ptsave */
} /* namespace gmb */
