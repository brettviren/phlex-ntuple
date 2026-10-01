// The row type shared by the graph-roundtrip test plugins and verifier.
#ifndef PHLEX_NTUPLE_TEST_GRAPH_SUMMARY_HPP
#define PHLEX_NTUPLE_TEST_GRAPH_SUMMARY_HPP

#include "arrow_ntuple/Record.hpp"

#include <cstdint>

struct Summary {
    std::int32_t nhits;
    double q;
};
ARROW_NTUPLE_FIELDS(Summary, nhits, q);

#endif
