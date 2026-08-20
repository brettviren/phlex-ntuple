#include "phlex_ntuple/Provenance.hpp"

#include "arrow_ntuple/Error.hpp"

#include <arrow/api.h>

#include <algorithm>
#include <utility>

namespace phlex_ntuple {

std::vector<std::pair<std::string, std::int64_t>> cell_path(const phlex::data_cell_index& cell)
{
    std::vector<std::pair<std::string, std::int64_t>> out;
    const phlex::data_cell_index* p = &cell;
    // Walk parents until the job root (which has no parent), collecting each
    // (layer, number); then reverse so the result is outermost-first.
    while (p->has_parent()) {
        out.emplace_back(std::string(static_cast<std::string_view>(p->layer_name())),
                         static_cast<std::int64_t>(p->number()));
        p = p->parent().get();
    }
    std::reverse(out.begin(), out.end());
    return out;
}

void provenance_buffer::record(const phlex::data_cell_index& cell, std::string_view creator)
{
    const auto path = cell_path(cell);
    if (!m_frozen) {
        m_layers.clear();
        m_numbers.clear();
        for (const auto& [layer, _] : path) {
            m_layers.push_back(layer);
            m_numbers.emplace_back();
        }
        m_frozen = true;
    } else {
        if (path.size() != m_layers.size()) {
            throw arrow_ntuple::error(
                "phlex_ntuple: inconsistent cell depth across rows (provenance "
                "expects one layer per row)");
        }
        for (std::size_t i = 0; i < path.size(); ++i) {
            if (path[i].first != m_layers[i]) {
                throw arrow_ntuple::error("phlex_ntuple: inconsistent layer name '" +
                                          path[i].first + "' (expected '" + m_layers[i] + "')");
            }
        }
    }
    for (std::size_t i = 0; i < path.size(); ++i) m_numbers[i].push_back(path[i].second);
    m_creators.emplace_back(creator);
}

std::shared_ptr<arrow::Table>
provenance_buffer::prepend_to(const std::shared_ptr<arrow::Table>& user) const
{
    if (m_creators.empty()) return user;
    if (static_cast<std::size_t>(user->num_rows()) != m_creators.size()) {
        throw arrow_ntuple::error("phlex_ntuple: provenance row count (" +
                                  std::to_string(m_creators.size()) +
                                  ") does not match table row count (" +
                                  std::to_string(user->num_rows()) + ")");
    }

    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::ChunkedArray>> columns;

    for (std::size_t i = 0; i < m_layers.size(); ++i) {
        arrow::Int64Builder b;
        if (auto st = b.AppendValues(m_numbers[i]); !st.ok()) throw arrow_ntuple::error(st);
        std::shared_ptr<arrow::Array> a;
        if (auto st = b.Finish(&a); !st.ok()) throw arrow_ntuple::error(st);
        fields.push_back(arrow::field("cell." + m_layers[i], arrow::int64(), /*nullable=*/false));
        columns.push_back(std::make_shared<arrow::ChunkedArray>(a));
    }
    {
        arrow::StringBuilder b;
        if (auto st = b.AppendValues(m_creators); !st.ok()) throw arrow_ntuple::error(st);
        std::shared_ptr<arrow::Array> a;
        if (auto st = b.Finish(&a); !st.ok()) throw arrow_ntuple::error(st);
        fields.push_back(arrow::field("creator", arrow::utf8(), /*nullable=*/false));
        columns.push_back(std::make_shared<arrow::ChunkedArray>(a));
    }

    // Append the user's columns after the provenance columns.
    const auto& us = user->schema();
    for (int i = 0; i < us->num_fields(); ++i) {
        fields.push_back(us->field(i));
        columns.push_back(user->column(i));
    }

    auto schema = arrow::schema(fields, us->metadata());
    return arrow::Table::Make(schema, columns);
}

void provenance_buffer::reset()
{
    m_frozen = false;
    m_layers.clear();
    m_numbers.clear();
    m_creators.clear();
}

}  // namespace phlex_ntuple
