// Provenance stamping: cell path -> leading cell.<layer> + creator columns.

#include "phlex_ntuple/Provenance.hpp"

#include "arrow_ntuple/Ntuple.hpp"

#include "phlex/model/data_cell_index.hpp"

#include <arrow/api.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++fails; }
}

int main()
{
    using phlex::data_cell_index;

    // cell_path excludes the job root and is outermost-first.
    auto job = data_cell_index::job();
    auto run0 = job->make_child("run", 0);
    auto ev5 = run0->make_child("event", 5);
    auto ev7 = run0->make_child("event", 7);
    {
        auto p = phlex_ntuple::cell_path(*ev5);
        check(p.size() == 2, "two levels below job");
        check(p[0].first == "run" && p[0].second == 0, "outermost is run 0");
        check(p[1].first == "event" && p[1].second == 5, "innermost is event 5");
    }

    // A two-row user table.
    arrow_ntuple::ntuple nt{"x", arrow_ntuple::column<double>("q")};
    nt.append(1.25);
    nt.append(2.5);
    auto user = nt.finish();

    phlex_ntuple::provenance_buffer prov;
    prov.record(*ev5, "make:algo");
    prov.record(*ev7, "make:algo");
    check(prov.num_rows() == 2, "recorded two rows");

    auto t = prov.prepend_to(user);
    auto s = t->schema();
    check(s->num_fields() == 4, "provenance (3) + user (1) columns");
    check(s->field(0)->name() == "cell.run" && s->field(0)->type()->Equals(arrow::int64()),
          "col0 cell.run int64");
    check(s->field(1)->name() == "cell.event", "col1 cell.event");
    check(s->field(2)->name() == "creator" && s->field(2)->type()->Equals(arrow::utf8()),
          "col2 creator utf8");
    check(s->field(3)->name() == "q", "user column preserved after provenance");

    auto run_a = std::static_pointer_cast<arrow::Int64Array>(t->column(0)->chunk(0));
    auto evt_a = std::static_pointer_cast<arrow::Int64Array>(t->column(1)->chunk(0));
    auto cre_a = std::static_pointer_cast<arrow::StringArray>(t->column(2)->chunk(0));
    auto q_a = std::static_pointer_cast<arrow::DoubleArray>(t->column(3)->chunk(0));
    check(run_a->Value(0) == 0 && run_a->Value(1) == 0, "cell.run values");
    check(evt_a->Value(0) == 5 && evt_a->Value(1) == 7, "cell.event values");
    check(cre_a->GetString(0) == "make:algo", "creator value");
    check(q_a->Value(0) == 1.25 && q_a->Value(1) == 2.5, "user values intact");

    // Version metadata from the ntuple survives.
    check(t->schema()->metadata() && t->schema()->metadata()->Contains("arrow_ntuple.version"),
          "user schema metadata preserved");

    // Inconsistent cell depth across rows throws.
    {
        phlex_ntuple::provenance_buffer bad;
        bad.record(*ev5, "c");            // depth 2
        bool threw = false;
        try { bad.record(*run0, "c"); }   // depth 1
        catch (const std::exception&) { threw = true; }
        check(threw, "inconsistent cell depth throws");
    }

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "OK\n";
    return 0;
}
