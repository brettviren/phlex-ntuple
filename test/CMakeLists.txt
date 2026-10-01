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

# End-to-end running-graph test (public Phlex API only): two test plugins
# (graph/source.cpp: the producer; graph/module.cpp: the fold + an HDF5 output
# node) are run by the `phlex` program over graph/graph_roundtrip.jsonnet, then
# graph/verify.cpp checks the written file.  Needs the HDF5 output path
# (arrow_hdf + HDF5) and the phlex executable (whose lib dir holds the
# generate_layers driver).
find_program(PHLEX_EXECUTABLE phlex HINTS "${phlex_BIN_DIR}")
if(arrow_hdf_FOUND AND HDF5_FOUND AND PHLEX_EXECUTABLE)
  add_library(phlex_ntuple_test_source MODULE graph/source.cpp)
  target_link_libraries(phlex_ntuple_test_source PRIVATE phlex::source
    arrow_ntuple::arrow_ntuple ${PHLEX_NTUPLE_RUNTIME_LIBS})

  add_library(phlex_ntuple_test_module MODULE graph/module.cpp)
  target_link_libraries(phlex_ntuple_test_module PRIVATE phlex_ntuple phlex::module
    phlex_arrow_common::phlex_arrow_common arrow_hdf::arrow_hdf arrow_ntuple::arrow_ntuple
    Arrow::arrow_shared HDF5::HDF5 ${PHLEX_NTUPLE_RUNTIME_LIBS})

  add_executable(test_graph_verify graph/verify.cpp)
  target_link_libraries(test_graph_verify PRIVATE phlex_arrow_common::phlex_arrow_common
    arrow_hdf::arrow_hdf Arrow::arrow_shared HDF5::HDF5 ${PHLEX_NTUPLE_RUNTIME_LIBS})

  set(_graph_dir "${CMAKE_CURRENT_BINARY_DIR}/graph_roundtrip")
  file(MAKE_DIRECTORY "${_graph_dir}")
  add_test(NAME graph_roundtrip_run
           COMMAND "${PHLEX_EXECUTABLE}" -c "${CMAKE_CURRENT_SOURCE_DIR}/graph/graph_roundtrip.jsonnet"
           WORKING_DIRECTORY "${_graph_dir}")
  set_tests_properties(graph_roundtrip_run PROPERTIES
    ENVIRONMENT "PHLEX_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR}:${phlex_LIBRARY_DIR}"
    FIXTURES_SETUP graph_roundtrip)
  add_test(NAME graph_roundtrip COMMAND test_graph_verify graph_roundtrip.h5
           WORKING_DIRECTORY "${_graph_dir}")
  set_tests_properties(graph_roundtrip PROPERTIES FIXTURES_REQUIRED graph_roundtrip)
else()
  message(STATUS "phlex-ntuple: skipping graph_roundtrip test (needs arrow_hdf, HDF5 "
                 "and the phlex executable).")
endif()
