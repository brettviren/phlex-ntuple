// Phase-3 acceptance: a producer -> fold -> output graph over a two-layer
// (run/event) hierarchy, run with an out-of-order concurrent producer, writes
// one HDF5 file whose per-run tables carry correct per-row cell provenance.
//
// The producer runs at concurrency::unlimited (rows arrive out of order); the
// fold is serial (Phlex does not serialize per-accumulator — the accumulator is
// not thread-safe). Provenance is stamped per row from the handle's cell, so it
// is correct regardless of arrival order — that is what this test proves.

#include "phlex_ntuple/Accumulator.hpp"

#include "phlex_arrow_common/ArrowProducts.hpp"
#include "phlex_arrow_common/CellAddress.hpp"
#include "phlex_arrow_common/StoreAddress.hpp"
#include "phlex_arrow_common/TableGroup.hpp"

#include "arrow_hdf/Address.hpp"
#include "arrow_hdf/Hdf5File.hpp"

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"

#include "plugins/layer_generator.hpp"

#include <arrow/api.h>

#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace phlex;

struct Summary {
    std::int32_t nhits;
    double q;
};
ARROW_NTUPLE_FIELDS(Summary, nhits, q);

namespace {

int fails = 0;
void check(bool ok, const std::string& what)
{
    if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++fails; }
}

// Persist TableGroup products to HDF5 (mirrors phlex-arrow-hdf's ArrowHdfOutput,
// reusing phlex-arrow-common + arrow-hdf) and capture them for verification.
struct Writer {
    std::string path;
    std::optional<arrow_hdf::Hdf5File>* file;
    std::vector<phlex_arrow::TableGroup>* captured;

    Writer(std::string p, std::optional<arrow_hdf::Hdf5File>& f,
           std::vector<phlex_arrow::TableGroup>& c)
      : path(std::move(p)), file(&f), captured(&c)
    {
    }

    void write(phlex::experimental::product_store const& store)
    {
        if (store.empty()) return;
        auto specs = phlex_arrow::select_arrow_products(store);
        if (specs.empty()) return;
        if (!file->has_value()) {
            auto f = arrow_hdf::Hdf5File::create(path);
            if (!f.ok()) throw std::runtime_error(f.status().ToString());
            file->emplace(std::move(*f));
            auto st = (*file)->write_root_attr(phlex_arrow::kAddressSchemeAttr,
                                               phlex_arrow::kAddressScheme);
            if (!st.ok()) throw std::runtime_error(st.ToString());
        }
        for (auto const& spec : specs) {
            std::string product{static_cast<std::string_view>(spec.suffix())};
            arrow_hdf::Address addr(phlex_arrow::store_address(store, product));
            auto const& tg = store.get_product<phlex_arrow::TableGroup>(spec);
            captured->push_back(tg);
            auto st = (*file)->write_tables(addr, tg.members, tg.type);
            if (!st.ok()) throw std::runtime_error(st.ToString());
        }
    }
};

}  // namespace

int main()
{
    const std::string path = "graph_roundtrip.h5";
    constexpr int NRUN = 2, NEVT = 3;

    auto gen = experimental::layer_generator::make();
    gen->add_layer("run", {"job", static_cast<std::size_t>(NRUN)});
    gen->add_layer("event", {"run", static_cast<std::size_t>(NEVT)});

    auto g = phlex::detail::framework_graph::without_driver(4);
    g.add_driver(gen);

    // Producer: one Summary per event cell.  nhits == event number ties the
    // payload to the provenance so a mix-up would be caught.
    g.provide(
       "make",
       [](data_cell_index const& idx) -> Summary {
           return Summary{.nhits = static_cast<std::int32_t>(idx.number()),
                          .q = 0.5 * static_cast<double>(idx.number())};
       },
       concurrency::unlimited)
      .output_product("make", "summary", "event");

    // Fold: one accumulator per run partition; serial (accumulator not thread-safe).
    g.fold("collect", &phlex_ntuple::fold_step<Summary>, concurrency::serial, "run",
           std::string("hit_summary"))
      .input_family(product_selector{.creator = "make", .layer = "event", .suffix = "summary"})
      .output_product_suffixes("hit_summary");

    std::optional<arrow_hdf::Hdf5File> file;
    std::vector<phlex_arrow::TableGroup> captured;
    g.make<Writer>(path, file, captured).output("write", &Writer::write, concurrency::serial);

    g.execute();
    file.reset();  // flush/close before re-reading

    // --- verify the captured products (exactly what was written) ---
    check(static_cast<int>(captured.size()) == NRUN, "one TableGroup per run partition");
    std::set<std::pair<std::int64_t, std::int64_t>> seen;
    int total_rows = 0;
    for (auto const& tg : captured) {
        check(tg.type == "hit_summary", "TableGroup type");
        check(tg.members.count("hit_summary") == 1, "member present");
        auto t = tg.members.at("hit_summary")->CombineChunks().ValueOrDie();
        auto s = t->schema();
        const int ri = s->GetFieldIndex("cell.run");
        const int ei = s->GetFieldIndex("cell.event");
        const int ni = s->GetFieldIndex("nhits");
        check(ri >= 0 && ei >= 0 && s->GetFieldIndex("creator") >= 0 && ni >= 0 &&
                  s->GetFieldIndex("q") >= 0,
              "provenance + user columns present");
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
    check(total_rows == NRUN * NEVT, "total row count across partitions");
    bool all_pairs = static_cast<int>(seen.size()) == NRUN * NEVT;
    for (int r = 0; r < NRUN; ++r)
        for (int e = 0; e < NEVT; ++e) all_pairs = all_pairs && seen.count({r, e});
    check(all_pairs, "every (run,event) present exactly once despite concurrency");

    // --- verify the HDF5 file persisted and carries the address-scheme stamp ---
    {
        auto ro = arrow_hdf::Hdf5File::open_readonly(path);
        check(ro.ok(), "HDF5 file re-opens read-only");
        if (ro.ok()) {
            auto attr = ro->read_root_attr(phlex_arrow::kAddressSchemeAttr);
            check(attr.ok() && attr.ValueOrDie() == phlex_arrow::kAddressScheme,
                  "address-scheme root attribute stamped");
            check(ro->scan().ok(), "scan() succeeds on the written file");
        }
    }

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "OK: producer->fold->output graph wrote " << total_rows
              << " provenance-stamped rows across " << captured.size() << " run tables\n";
    return 0;
}
