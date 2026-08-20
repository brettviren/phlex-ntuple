# phlex-ntuple tests.

add_executable(test_provenance test_provenance.cpp)
target_link_libraries(test_provenance PRIVATE phlex_ntuple phlex::model
  arrow_ntuple::arrow_ntuple Arrow::arrow_shared ${PHLEX_NTUPLE_RUNTIME_LIBS})
add_test(NAME provenance COMMAND test_provenance)

add_executable(test_accumulator test_accumulator.cpp)
target_link_libraries(test_accumulator PRIVATE phlex_ntuple phlex::model
  phlex_arrow_common::phlex_arrow_common arrow_ntuple::arrow_ntuple
  Arrow::arrow_shared ${PHLEX_NTUPLE_RUNTIME_LIBS})
add_test(NAME accumulator COMMAND test_accumulator)

# End-to-end running-graph test.  Needs the HDF5 output path (arrow_hdf + HDF5)
# and phlex's layer_generator driver (whose header lives in the phlex SOURCE
# tree, not the install; its shared lib is in the Spack view).
set(PHLEX_NTUPLE_REFERENCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../reference/phlex"
    CACHE PATH "phlex source tree (for the layer_generator test-driver header)")
find_library(PHLEX_LAYER_GENERATOR_LIB layer_generator)
if(arrow_hdf_FOUND AND HDF5_FOUND AND PHLEX_LAYER_GENERATOR_LIB AND
   EXISTS "${PHLEX_NTUPLE_REFERENCE_DIR}/plugins/layer_generator.hpp")
  add_executable(test_graph_roundtrip test_graph_roundtrip.cpp)
  target_include_directories(test_graph_roundtrip PRIVATE "${PHLEX_NTUPLE_REFERENCE_DIR}")
  target_link_libraries(test_graph_roundtrip PRIVATE phlex_ntuple phlex::core
    phlex_arrow_common::phlex_arrow_common arrow_hdf::arrow_hdf arrow_ntuple::arrow_ntuple
    Arrow::arrow_shared HDF5::HDF5 "${PHLEX_LAYER_GENERATOR_LIB}" ${PHLEX_NTUPLE_RUNTIME_LIBS})
  add_test(NAME graph_roundtrip COMMAND test_graph_roundtrip)
else()
  message(STATUS "phlex-ntuple: skipping graph_roundtrip test (needs arrow_hdf, HDF5, "
                 "the layer_generator lib, and the phlex source tree).")
endif()
