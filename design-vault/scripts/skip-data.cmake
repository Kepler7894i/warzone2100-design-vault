# Passed as CMAKE_PROJECT_TOP_LEVEL_INCLUDES. Builds only the game executable:
#  - the game data (base.wz, mp.wz, music, videos) is taken from an installed copy of the game, so the data packaging step,
#    which needs several hundred MB of art, is replaced by an empty target;
#  - installer packaging (pkg) is not needed.
macro(add_subdirectory _wz_dir)
	if("${_wz_dir}" STREQUAL "data")
		message(STATUS "design-vault build: skipping data packaging")
		add_custom_target(data)
	elseif("${_wz_dir}" STREQUAL "pkg")
		message(STATUS "design-vault build: skipping installer packaging")
	else()
		_add_subdirectory(${ARGV})
	endif()
endmacro()
