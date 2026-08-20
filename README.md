# phlex-ntuple

Phlex glue for [`arrow-ntuple`](../arrow-ntuple): stamp per-row cell provenance
and accumulate rows across a fold into a `phlex_arrow::TableGroup` product that
the existing `phlex-arrow-hdf` output node persists unchanged.

This package deliberately does **not** reinvent the addressing or output path —
[`phlex-arrow-common`](../phlex-arrow-common) already owns the address scheme
(`stage-plugin-algorithm-suffix/v1`) and cell extraction, and
[`phlex-arrow-hdf`](../phlex-arrow-hdf) already owns the HDF5 output node. The
only genuinely new concerns here are:

1. **Provenance stamping** (`phlex_ntuple/Provenance.hpp`) — because a fold
   collapses many fine cells into one coarse table, the fine-cell identity would
   be lost unless recorded per row. `provenance_buffer` prepends, in order,
   `cell.<layer> : int64` for each layer of the row's `data_cell_index` (job
   root excluded) plus `creator : utf8`.
2. **The fold accumulator** (`phlex_ntuple/Accumulator.hpp`) — `accumulator<S>`
   wraps an `arrow_ntuple::row_ntuple<S>`, appends each incoming row (stamping
   provenance from the row's `phlex::handle<S>`), and produces a `TableGroup`
   via the ADL `send()` the Phlex fold applies at commit.

## Usage

```cpp
struct Summary { int nhits; double q; };
ARROW_NTUPLE_FIELDS(Summary, nhits, q);

// producer: one Summary per fine (event) cell
g.provide("make", [](phlex::data_cell_index const& c) -> Summary { /* ... */ },
          concurrency::unlimited)
 .output_product("make", "summary", "event");

// fold: one accumulator per coarse (run) partition; serial (the accumulator is
// not thread-safe — Phlex does not serialize per-accumulator).
g.fold("collect", &phlex_ntuple::fold_step<Summary>, concurrency::serial, "run",
       std::string{"hit_summary"})
 .input_family(product_selector{.creator="make", .layer="event", .suffix="summary"})
 .output_product_suffixes("hit_summary");

// output: the existing phlex-arrow-hdf module persists the TableGroup.
```

Resulting HDF5 (per run), readable in plain h5py:

```
/run/<r>/CURRENT/_/collect/hit_summary/hit_summary
    cell.run    (N,)   int64      <- stamped
    cell.event  (N,)   int64      <- stamped
    creator     (N,)   utf8       <- stamped ("plugin:algorithm")
    nhits       (N,)   int32
    q           (N,)   float64
```

The row travelling the graph is a plain `Summary` value, not a one-row Arrow
table — the table materialises once, at the fold (see the arrow-ntuple design
notes on the measured cost of one-row tables).

## Dependencies

`arrow-ntuple`, `phlex-arrow-common` (TableGroup, cell/address helpers),
`phlex` (`handle`, `data_cell_index`). The library links `phlex::model` only;
it does **not** depend on arrow-hdf or phlex-arrow-hdf. C++23 (phlex's floor).

## Build & test

```bash
./umbrella config phlex-ntuple
./umbrella build  phlex-ntuple
ctest --test-dir builds/envs/gcc15/phlex-ntuple --output-on-failure
```

Tests: `provenance` and `accumulator` (Arrow + phlex::model only), plus
`graph_roundtrip` — a real producer→fold→output `framework_graph` run
concurrently, writing an HDF5 file whose per-run tables carry correct per-row
provenance (the last is skipped if arrow-hdf / HDF5 / the phlex source tree are
unavailable).
