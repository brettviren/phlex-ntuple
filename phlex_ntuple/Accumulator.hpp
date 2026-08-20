#ifndef PHLEX_NTUPLE_ACCUMULATOR_H
#define PHLEX_NTUPLE_ACCUMULATOR_H

// phlex_ntuple::accumulator<S> — the strategy-B fold holder.
//
// A row-producing node emits one record S per fine cell; a Phlex fold owns one
// accumulator<S> per partition (coarse) cell, appends each incoming row, and
// on partition completion the framework turns the accumulator into a product
// via the ADL free function `send()` below.  The product is a
// phlex_arrow::TableGroup (the canonical Phlex Arrow product), so the existing
// phlex-arrow-hdf output node persists it unchanged.
//
// The fold learns each row's cell from a phlex::handle<S> input (the only route
// Phlex offers), so provenance columns (cell.<layer> + creator) are stamped
// from the handle — see fold_step() and Provenance.hpp.
//
// Register with, e.g.:
//   g.fold("collect", &phlex_ntuple::fold_step<Summary>, concurrency::unlimited, "run",
//          std::string{"hit_summary"})
//    .input_family(product_selector{.creator="make", .layer="event", .suffix="summary"})
//    .output_product_suffixes("hit_summary");

#include "phlex_ntuple/Provenance.hpp"

#include "arrow_ntuple/Record.hpp"
#include "phlex_arrow_common/TableGroup.hpp"

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace phlex_ntuple {

struct accumulator_options {
    std::string group_type;                        ///< TableGroup.type; empty = the ntuple name
    std::string member;                            ///< member key; empty = the ntuple name
    bool stamp_provenance = true;
    arrow_ntuple::ntuple_options ntuple_opts{};
};

template <typename S>
class accumulator {
  public:
    // Constructed once per partition cell from the fold's init args (brace-init).
    explicit accumulator(std::string name, accumulator_options opts = {})
      : m_name(std::move(name)), m_opts(std::move(opts)), m_rows(m_name, m_opts.ntuple_opts)
    {
    }

    /// Append one row, stamping the fine cell + creator into provenance.
    void append(const phlex::data_cell_index& cell, std::string_view creator, const S& row)
    {
        if (m_opts.stamp_provenance) m_prov.record(cell, creator);
        m_rows.append(row);
    }

    /// Materialise the accumulated rows (+ provenance) as a TableGroup product,
    /// and reset for reuse.
    phlex_arrow::TableGroup table_group()
    {
        auto user = m_rows.finish();
        auto table = m_opts.stamp_provenance ? m_prov.prepend_to(user) : user;
        phlex_arrow::TableGroup tg;
        tg.type = m_opts.group_type.empty() ? m_name : m_opts.group_type;
        tg.members[m_opts.member.empty() ? m_name : m_opts.member] = std::move(table);
        m_prov.reset();
        return tg;
    }

    const std::string& name() const noexcept { return m_name; }

  private:
    std::string m_name;
    accumulator_options m_opts;
    arrow_ntuple::row_ntuple<S> m_rows;
    provenance_buffer m_prov;
};

/// The fold function: first parameter is the accumulator; the row arrives as a
/// handle so its cell + creator are available for provenance.
template <typename S>
void fold_step(accumulator<S>& acc, phlex::handle<S> h)
{
    const auto cr = h.creator();  // {plugin, algorithm} string_views
    std::string creator = std::string(cr.plugin) + ":" + std::string(cr.algorithm);
    acc.append(h.data_cell_index(), creator, *h);
}

/// Fold-result conversion (found by ADL at the fold's commit): turn the
/// accumulator into its TableGroup product.
template <typename S>
phlex_arrow::TableGroup send(accumulator<S>& acc)
{
    return acc.table_group();
}

}  // namespace phlex_ntuple

#endif
