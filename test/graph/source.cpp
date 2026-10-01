// Graph-roundtrip test plugin: the producer.  One Summary per event cell;
// nhits == event number ties the payload to the provenance so a mix-up would
// be caught by the verifier.  Runs at concurrency::unlimited so rows reach the
// fold out of order.
#include "summary.hpp"

#include "phlex/model/data_cell_index.hpp"
#include "phlex/source.hpp"

#include <cstdint>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(m)
{
    m.provide(
       "make",
       [](data_cell_index const& idx) -> Summary {
           return Summary{.nhits = static_cast<std::int32_t>(idx.number()),
                          .q = 0.5 * static_cast<double>(idx.number())};
       },
       concurrency::unlimited)
      .output_product("make", "summary", "event");
}
