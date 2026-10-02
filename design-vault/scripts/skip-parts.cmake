# Passed as CMAKE_PROJECT_TOP_LEVEL_INCLUDES, leaves out parts of the build that a development build does not need:
#  - installer packaging (pkg): it downloads files while configuring and needs signing/installer tools;
#  - with -DDESIGNVAULT_SKIP_DATA=ON also the game data (base.wz, mp.wz, music, videos), which is then not built; it
#    has to come from elsewhere (--datadir=...). The default is to build the data, so the result is a complete game.
macro(add_subdirectory _wz_dir)
	if("${_wz_dir}" STREQUAL "pkg")
		message(STATUS "design-vault build: skipping installer packaging")
	elseif("${_wz_dir}" STREQUAL "data" AND DESIGNVAULT_SKIP_DATA)
		message(STATUS "design-vault build: skipping data packaging")
		add_custom_target(data)
	else()
		_add_subdirectory(${ARGV})
	endif()
endmacro()
