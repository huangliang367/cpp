# TODO1: Add a MakeTable executable
add_executable(MakeTable)
# TODO2: Add MakeTable.cxx to the MakeTable executable
target_sources(MakeTable PRIVATE MakeTable.cxx)
# TODO3: Add a custom command which invokes MakeTable to generate SqrtTable.h
add_custom_command(
	OUTPUT SqrtTable.h
	COMMAND MakeTable SqrtTable.h
	DEPENDS MakeTable
	VERBATIM
)
# TODO4: Add a custom target which depends on SqrtTable.h
add_custom_target(RunMakeTable DEPENDS SqrtTable.h)
# TODO5: Add an INTERFACE library to describe the SqrtTable header
add_library(SqrtTable INTERFACE)
target_sources(SqrtTable
	INTERFACE 
	FILE_SET HEADERS
	BASE_DIRS
		${CMAKE_CURRENT_BINARY_DIR}
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/SqrtTables.h
)
# TODO6: Add the current binary directory (and optionally the SqrtTable.h FILE)
#        to a header file set of the interface library
# TODO7: Use add_dependencies to ensure the custom target always runs before
#        targets that depend on the interface library
add_dependencies(SqrtTable RunMakeTable)
