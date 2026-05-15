add_executable(opendss_runtime_metadata_loader_test
  runtime_metadata_loader_test.cpp
)

target_link_libraries(opendss_runtime_metadata_loader_test PRIVATE
  opendss_runtime
)

add_test(
  NAME opendss_runtime_metadata_loader_test
  COMMAND opendss_runtime_metadata_loader_test
)
