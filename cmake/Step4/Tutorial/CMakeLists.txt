add_executable(Tutorial)

target_sources(Tutorial
  PRIVATE
    Tutorial.cxx
)

# TODO14: Add VendorLib to Tutorial

target_link_libraries(Tutorial
  PRIVATE
    MathFunctions
	VendorLib
)

# TODO4: Add a compile feature for C++20 support to Tutorial
target_compile_features(Tutorial PRIVATE cxx_std_20)

if(
  (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") OR
  (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
)

  # TODO9: Add the /W3 compile flag to Tutorial
 target_compile_options(Tutorial PRIVATE /W3)
elseif(
  (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR
  (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
)

  # TODO10: Add the -Wall compile flag to Tutorial
  target_compile_options(Tutorial PRIVATE -Wall)

endif()
