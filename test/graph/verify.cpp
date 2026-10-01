// Phase-3 acceptance verifier: checks the HDF5 file written by running
// graph_roundtrip.jsonnet through the `phlex` program (producer -> fold ->
// output over run/event, with an out-of-order concurrent producer).  Each run
// partition's table must carry correct per-row cell provenance.
//
// Usage: test_graph_verify <file.h5>

#include "phlex_arrow_common/CellAddress.hpp"

#include "arrow_hdf/Address.hpp"
#include "arrow_hdf/Hdf5File.hpp"

#include <arrow/api.h>

#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <utility>

namespace {
int fails = 0;
void check(bool ok, const std::string& what)
{
    if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++fails; }
}
}  // namespace

int main(int argc, char* argv[])
{
    if (argc != 2) { std::cerr << "usage: test_graph_verify <file.h5>\n"; return 2; }
    constexpr int NRUN = 2, NEVT = 3;

    auto ro = arrow_hdf::Hdf5File::open_readonly(argv[1]);
    if (!ro.ok()) { std::cerr << "FAIL: cannot open " << argv[1] << "\n"; return 1; }

    auto attr = ro->read_root_attr(phlex_arrow::kAddressSchemeAttr);
    check(attr.ok() && attr.ValueOrDie() == phlex_arrow::kAddressScheme,
          "address-scheme root attribute stamped");

    auto hier = ro->scan();
    check(hier.ok(), "scan() succeeds on the written file");
    if (!hier.ok()) return 1;

    // The fold's TableGroup member tables: one ".../hit_summary" table per run.
    int ntables = 0, total_rows = 0;
    std::set<std::pair<std::int64_t, std::int64_t>> seen;
    for (auto const& path : hier->product_paths()) {
        if (!path.ends_with("/hit_summary")) continue;
        ++ntables;
        auto rt = ro->read(arrow_hdf::Address(path));
        check(rt.ok(), "read " + path);
        if (!rt.ok()) continue;
        auto t = (*rt)->CombineChunks().ValueOrDie();
        auto s = t->schema();
        const int ri = s->GetFieldIndex("cell.run");
        const int ei = s->GetFieldIndex("cell.event");
        const int ni = s->GetFieldIndex("nhits");
        check(ri >= 0 && ei >= 0 && s->GetFieldIndex("creator") >= 0 && ni >= 0 &&
                  s->GetFieldIndex("q") >= 0,
              "provenance + user columns present");
        if (ri < 0 || ei < 0 || ni < 0) continue;
        auto run_a = std::static_pointer_cast<arrow::Int64Array>(t->column(ri)->chunk(0));
        auto evt_a = std::static_pointer_cast<arrow::Int64Array>(t->column(ei)->chunk(0));
        auto nh_a = std::static_pointer_cast<arrow::Int32Array>(t->column(ni)->chunk(0));
        for (std::int64_t i = 0; i < t->num_rows(); ++i) {
            seen.insert({run_a->Value(i), evt_a->Value(i)});
            check(nh_a->Value(i) == evt_a->Value(i),
                  "per-row provenance matches payload (nhits == cell.event)");
            ++total_rows;
        }
    }
    check(ntables == NRUN, "one table per run partition (got " + std::to_string(ntables) + ")");
    check(total_rows == NRUN * NEVT, "total row count across partitions");
    bool all_pairs = static_cast<int>(seen.size()) == NRUN * NEVT;
    for (int r = 0; r < NRUN; ++r)
        for (int e = 0; e < NEVT; ++e) all_pairs = all_pairs && seen.count({r, e});
    check(all_pairs, "every (run,event) present exactly once despite concurrency");

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "OK: producer->fold->output graph wrote " << total_rows
              << " provenance-stamped rows across " << ntables << " run tables\n";
    return 0;
}
