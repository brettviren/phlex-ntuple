// Phase-3 acceptance workflow: producer -> fold -> output over a two-layer
// (run/event) hierarchy; see verify.cpp for what is checked afterwards.
{
  stage: 'ntuple_test',
  driver: {
    cpp: 'generate_layers',
    layers: {
      run: { parent: 'job', total: 2 },
      event: { parent: 'run', total: 3 },
    },
  },
  sources: {
    make: { cpp: 'phlex_ntuple_test_source' },
  },
  modules: {
    ntuple: { cpp: 'phlex_ntuple_test_module', output_file: 'graph_roundtrip.h5' },
  },
  max_concurrency: 4,
}
