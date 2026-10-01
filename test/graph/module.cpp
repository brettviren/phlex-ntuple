// Graph-roundtrip test plugin: the fold (one phlex_ntuple::accumulator per
// "run" partition) and an output node persisting TableGroup products to HDF5
// (mirrors phlex-arrow-hdf's ArrowHdfOutput, reusing phlex-arrow-common +
// arrow-hdf).
//
// Config: output_file (string, required).
#include "summary.hpp"

#include "phlex_ntuple/Accumulator.hpp"

#include "phlex_arrow_common/ArrowProducts.hpp"
#include "phlex_arrow_common/CellAddress.hpp"
#include "phlex_arrow_common/PhlexTypes.hpp"
#include "phlex_arrow_common/StoreAddress.hpp"
#include "phlex_arrow_common/TableGroup.hpp"

#include "arrow_hdf/Address.hpp"
#include "arrow_hdf/Hdf5File.hpp"

#include "phlex/module.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace phlex;

namespace {

struct Writer {
    std::string path;
    std::optional<arrow_hdf::Hdf5File> file;

    explicit Writer(std::string p) : path(std::move(p)) {}

    void write(phlex_arrow::product_store const& store)
    {
        if (store.empty()) return;
        auto specs = phlex_arrow::select_arrow_products(store);
        if (specs.empty()) return;
        if (!file) {
            auto f = arrow_hdf::Hdf5File::create(path);
            if (!f.ok()) throw std::runtime_error(f.status().ToString());
            file.emplace(std::move(*f));
            auto st = file->write_root_attr(phlex_arrow::kAddressSchemeAttr,
                                            phlex_arrow::kAddressScheme);
            if (!st.ok()) throw std::runtime_error(st.ToString());
        }
        for (auto const& spec : specs) {
            std::string product{static_cast<std::string_view>(spec.suffix())};
            arrow_hdf::Address addr(phlex_arrow::store_address(store, product));
            auto const& tg = store.get_product<phlex_arrow::TableGroup>(&spec);
            auto st = file->write_tables(addr, tg.members, tg.type);
            if (!st.ok()) throw std::runtime_error(st.ToString());
        }
    }
};

}  // namespace

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    // Fold: one accumulator per run partition; serial (accumulator not thread-safe).
    m.fold("collect", &phlex_ntuple::fold_step<Summary>, concurrency::serial, "run",
           std::string("hit_summary"))
      .input_family(product_selector{.creator = "make", .layer = "event", .suffix = "summary"})
      .output_product_suffixes("hit_summary");

    m.make<Writer>(config.get<std::string>("output_file"))
      .output("write", &Writer::write, concurrency::serial);
}
