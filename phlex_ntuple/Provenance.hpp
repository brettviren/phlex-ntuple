#ifndef PHLEX_NTUPLE_PROVENANCE_H
#define PHLEX_NTUPLE_PROVENANCE_H

// Provenance stamping: turn the per-row Phlex cell identity into leading table
// columns, so that a fold (which collapses many fine cells into one coarse
// table) does not lose the fine-cell identity.
//
// For each recorded row we prepend, in order:
//   cell.<layer0> : int64   ...   cell.<layerN> : int64   creator : utf8
// where the layers are the (layer, number) chain of the row's data_cell_index
// (outermost first, excluding the job root), and creator is "plugin:algorithm"
// of the product's creator.
//
// The layer set is frozen by the FIRST recorded row; a later row with a
// different cell depth or layer names throws (the fold's input family should
// select a single layer, so all rows share one path shape).
//
// Depends only on phlex::model (data_cell_index) and Apache Arrow — not on the
// graph/module machinery.

#include "phlex/model/data_cell_index.hpp"

#include <arrow/type_fwd.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phlex_ntuple {

/// The (layer, number) chain of `cell`, outermost first, excluding the job root.
std::vector<std::pair<std::string, std::int64_t>> cell_path(const phlex::data_cell_index& cell);

class provenance_buffer {
  public:
    /// Record one row's cell identity and creator ("plugin:algorithm").
    void record(const phlex::data_cell_index& cell, std::string_view creator);

    std::size_t num_rows() const noexcept { return m_creators.size(); }
    const std::vector<std::string>& layers() const noexcept { return m_layers; }

    /// Return a new table with the provenance columns prepended to `user`.
    /// If nothing was recorded, returns `user` unchanged.  Throws (arrow_ntuple::
    /// error) if the recorded row count does not match `user`'s row count.
    std::shared_ptr<arrow::Table> prepend_to(const std::shared_ptr<arrow::Table>& user) const;

    /// Discard recorded rows (keeps nothing; the next record() re-freezes layers).
    void reset();

  private:
    bool m_frozen = false;
    std::vector<std::string> m_layers;                 // e.g. {"run", "event"}
    std::vector<std::vector<std::int64_t>> m_numbers;  // [layer][row]
    std::vector<std::string> m_creators;               // [row]
};

}  // namespace phlex_ntuple

#endif
