// accumulator<S>: append rows (+ provenance) and materialise a TableGroup.

#include "phlex_ntuple/Accumulator.hpp"

#include "phlex/model/data_cell_index.hpp"

#include <arrow/api.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

struct Summary {
    std::int32_t nhits;
    double q;
};
ARROW_NTUPLE_FIELDS(Summary, nhits, q);

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++fails; }
}

int main()
{
    using phlex::data_cell_index;
    auto job = data_cell_index::job();
    auto run2 = job->make_child("run", 2);

    // --- default: provenance stamped, TableGroup with one member ---
    {
        phlex_ntuple::accumulator<Summary> acc{"hit_summary"};
        acc.append(*run2->make_child("event", 0), "make:algo", Summary{.nhits = 3, .q = 1.5});
        acc.append(*run2->make_child("event", 1), "make:algo", Summary{.nhits = 5, .q = 2.5});

        auto tg = phlex_ntuple::send(acc);  // exercises the ADL fold-result hook
        check(tg.type == "hit_summary", "TableGroup type defaults to ntuple name");
        check(tg.members.size() == 1 && tg.members.count("hit_summary") == 1, "single member");

        auto t = tg.members.at("hit_summary");
        auto s = t->schema();
        check(t->num_rows() == 2, "two rows");
        check(s->field(0)->name() == "cell.run" && s->field(1)->name() == "cell.event" &&
                  s->field(2)->name() == "creator",
              "provenance columns lead");
        check(s->field(3)->name() == "nhits" && s->field(4)->name() == "q", "user columns follow");

        auto run_a = std::static_pointer_cast<arrow::Int64Array>(t->column(0)->chunk(0));
        auto evt_a = std::static_pointer_cast<arrow::Int64Array>(t->column(1)->chunk(0));
        auto nh_a = std::static_pointer_cast<arrow::Int32Array>(t->column(3)->chunk(0));
        check(run_a->Value(0) == 2 && run_a->Value(1) == 2, "cell.run constant within partition");
        check(evt_a->Value(0) == 0 && evt_a->Value(1) == 1, "cell.event per row");
        check(nh_a->Value(0) == 3 && nh_a->Value(1) == 5, "user payload intact");
    }

    // --- provenance disabled: only user columns; custom type/member ---
    {
        phlex_ntuple::accumulator_options opts;
        opts.stamp_provenance = false;
        opts.group_type = "wc.summary";
        opts.member = "table";
        phlex_ntuple::accumulator<Summary> acc{"hit_summary", opts};
        acc.append(*run2->make_child("event", 9), "make:algo", Summary{.nhits = 1, .q = 0.0});
        auto tg = acc.table_group();
        check(tg.type == "wc.summary", "custom group type");
        check(tg.members.count("table") == 1, "custom member name");
        auto t = tg.members.at("table");
        check(t->num_columns() == 2 && t->schema()->field(0)->name() == "nhits",
              "no provenance columns when disabled");
    }

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "OK\n";
    return 0;
}
