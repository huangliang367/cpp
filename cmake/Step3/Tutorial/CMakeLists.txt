add_executable(Tutorial)
message("Tutorial target created")
target_sources(Tutorial
  PRIVATE
    Tutorial.cxx
)

target_link_libraries(Tutorial
  PRIVATE
    MathFunctions
)
