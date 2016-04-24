#
# Find and setup asciidoc[tor] bits
#


# First see if we can find the programs
find_program(ASCIIDOCTOR asciidoctor)
find_program(ASCIIDOC asciidoc)
find_program(A2X a2x)


# If we have asciidoctor, we need to figure out the version, as manpage
# output is relatively new (unreleased, at the time of this writing).
if(ASCIIDOCTOR)
	execute_process(
		COMMAND ${ASCIIDOCTOR} --version
		RESULT_VARIABLE _adoctor_result
		OUTPUT_VARIABLE _adoctor_verout
		ERROR_QUIET
	)
	if(NOT ${_adoctor_result} EQUAL "0")
		# Err...
		message(WARNING "Unexpected result trying asciidoctor --version.")
		set(_adoctor_verout "Asciidoctor 0.0.0 FAKE")
	endif()
	unset(_adoctor_result)

	# Break out the version.
	set(_adoctor_veregex "Asciidoctor ([0-9]+\\.[0-9]+\\.[0-9]+).*")
	string(REGEX REPLACE ${_adoctor_veregex} "\\1"
		ASCIIDOCTOR_VERSION ${_adoctor_verout})
	unset(_adoctor_verout)
	unset(_adoctor_veregex)
	message(STATUS "Found asciidoctor (${ASCIIDOCTOR}) version ${ASCIIDOCTOR_VERSION}")

	# 1.5.3 is the first release that can write manpages natively.  This
	# means 1.5.3 dev versions after a certain point can as well; assume
	# anybody running a 1.5.3 dev is keeping up well enough that it can
	# DTRT too.  We assume any version can do HTML.
	set(ASCIIDOCTOR_CAN_MAN  0)
	set(ASCIIDOCTOR_CAN_HTML 1)
	if(${ASCIIDOCTOR_VERSION} VERSION_GREATER "1.5.2")
		set(ASCIIDOCTOR_CAN_MAN 1)
	elseif(${ASCIIDOCTOR_VERSION} VERSION_LESS "0.0.1")
		set(ASCIIDOCTOR_CAN_HTML 0)
	endif()
endif(ASCIIDOCTOR)


# For asciidoc, it doesn't really matter, but look up the version for
# cosmetics anyway
if(ASCIIDOC)
	execute_process(
		COMMAND ${ASCIIDOC} --version
		RESULT_VARIABLE _adoc_result
		OUTPUT_VARIABLE _adoc_verout
		ERROR_QUIET
	)
	if(NOT ${_adoc_result} EQUAL "0")
		# Err...
		message(WARNING "Unexpected result trying asciidoc --version.")
		set(_adoc_verout "asciidoc 0.0.0")
	endif()
	unset(_adoc_result)

	# Break out the version.
	set(_adoc_veregex "asciidoc ([0-9]+\\.[0-9]+\\.[0-9]+).*")
	string(REGEX REPLACE ${_adoc_veregex} "\\1"
		ASCIIDOC_VERSION ${_adoc_verout})
	unset(_adoc_verout)
	unset(_adoc_veregex)
	message(STATUS "Found asciidoc (${ASCIIDOC}) version ${ASCIIDOC_VERSION}")

	# Can always do both, unless horked
	if(${ASCIIDOC_VERSION} VERSION_GREATER "0.0.0")
		set(ASCIIDOC_CAN_MAN  1)
		set(ASCIIDOC_CAN_HTML 1)
	endif()

	# This is an example of 'horked'...
	if(NOT A2X)
		set(ASCIIDOC_CAN_MAN 0)
	endif()
endif(ASCIIDOC)




#
# Generator functions for creating targets for the various
# transformations.
#


# Build a manpage via asciidoctor
function(asciidoctor_mk_manpage MANFILE ADFILE)
	# Guard
	if(NOT ASCIIDOCTOR_CAN_MAN)
		message(FATAL_ERROR "asciidoctor can't do man")
	endif()

	# Minimal seatbelt
	set(my_usage "asciidoctor_mk_manpage(<output> <input> [DEPENDS <deps>] [COMMENT <comment>])")
	cmake_parse_arguments(
		_ARGS
		""
		"COMMENT"
		"DEPENDS"
		${ARGN}
	)
	if(_ARGS_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR ${my_usage})
	endif()

	# Always depend on the input file, maybe on more
	set(dependancies ${ADFILE})
	if(_ARGS_DEPENDS)
		list(APPEND dependancies ${_ARGS_DEPENDS})
	endif()

	# Come up with some comment or other
	if(NOT _ARGS_COMMENT)
		get_filename_component(basename ${MANFILE} NAME)
		set(_ARGS_COMMENT "Generating ${basename} with asciidoctor")
	endif()

	# Setup the rule
	add_custom_command(OUTPUT ${MANFILE}
		DEPENDS ${dependancies}
		COMMAND ${ASCIIDOCTOR} -b manpage -o ${MANFILE} ${ADFILE}
		COMMENT ${_ARGS_COMMENT}
	)
endfunction(asciidoctor_mk_manpage)


# Build a manpage via asciidoc
function(asciidoc_mk_manpage MANFILE ADFILE)
	# Guard
	if(NOT A2X OR NOT ASCIIDOC_CAN_MAN)
		message(FATAL_ERROR "asciidoc can't do man")
	endif()

	# Minimal seatbelt
	set(my_usage "asciidoc_mk_manpage(<output> <input> [DEPENDS <deps>] [COMMENT <comment>])")
	cmake_parse_arguments(
		_ARGS
		""
		"COMMENT"
		"DEPENDS"
		${ARGN}
	)
	if(_ARGS_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR ${my_usage})
	endif()

	# Always depend on the input file, maybe on more
	set(dependancies ${ADFILE})
	if(_ARGS_DEPENDS)
		list(APPEND dependancies ${_ARGS_DEPENDS})
	endif()

	# Come up with some comment or other
	if(NOT _ARGS_COMMENT)
		get_filename_component(basename ${MANFILE} NAME)
		set(_ARGS_COMMENT "Generating ${basename} with a2x")
	endif()

	# a2x gives us very little control over input/output files, so we
	# have to do some vaguely stupid dances.  In theory, -D works for the
	# manpage output, but it's doc'd not to and will warn, so don't even
	# try.  The result is that it always puts the outfile file next to
	# the input.  So we make a temporary dir (with a hopefully unique
	# name) and do all our stuff in there.
	get_filename_component(basedir ${ADFILE} DIRECTORY)
	while(1)
		string(RANDOM rndstr)
		set(a2x_tmpdir "${basedir}/a2x.${rndstr}")
		if(NOT IS_DIRECTORY ${a2x_tmpdir})
			break()
		endif()
	endwhile()
	file(MAKE_DIRECTORY ${a2x_tmpdir})

	# This had better already be named "someprog.somesection.adoc",
	# because a2x is going to magically figure the program and section
	# name from the contents and make that output file.
	get_filename_component(inbasename ${ADFILE} NAME)
	string(REGEX REPLACE "(.*)\\.adoc$" "\\1" outbasename ${inbasename})
	if(NOT outbasename)
		message(FATAL_ERROR "Can't figure output for ${inbasename}")
	endif()

	# In/out tmpfile names
	set(a2x_intmp  "${a2x_tmpdir}/${inbasename}")
	set(a2x_outtmp "${a2x_tmpdir}/${outbasename}")

	add_custom_command(OUTPUT ${MANFILE}
		DEPENDS ${dependancies}
		COMMAND cp ${ADFILE} ${a2x_intmp}
		COMMAND ${A2X} --doctype manpage --format manpage ${a2x_intmp}
		COMMAND mv ${a2x_outtmp} ${MANFILE}
		COMMAND rm ${a2x_intmp}
		COMMENT ${_ARGS_COMMENT}
	)
endfunction(asciidoc_mk_manpage)



# Build HTML output with asciidoctor
function(asciidoctor_mk_html HTMLFILE ADFILE)
	# Guard
	if(NOT ASCIIDOCTOR_CAN_HTML)
		message(FATAL_ERROR "asciidoctor can't do html")
	endif()

	# Minimal seatbelt
	set(my_usage "asciidoctor_mk_html(<output> <input> [DEPENDS <deps>] [COMMENT <comment>])")
	cmake_parse_arguments(
		_ARGS
		""
		"COMMENT"
		"DEPENDS"
		${ARGN}
	)
	if(_ARGS_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR ${my_usage})
	endif()

	# Always depend on the input file, maybe on more
	set(dependancies ${ADFILE})
	if(_ARGS_DEPENDS)
		list(APPEND dependancies ${_ARGS_DEPENDS})
	endif()

	# Come up with some comment or other
	if(NOT _ARGS_COMMENT)
		get_filename_component(basename ${HTMLFILE} NAME)
		set(_ARGS_COMMENT "Generating ${basename} with asciidoctor")
	endif()

	# Setup the rule
	add_custom_command(OUTPUT ${HTMLFILE}
		DEPENDS ${dependancies}
		COMMAND ${ASCIIDOCTOR} -atoc -anumbered -o ${HTMLFILE} ${ADFILE}
		COMMENT ${_ARGS_COMMENT}
	)
endfunction(asciidoctor_mk_html)


# And the asciidoc HTML
function(asciidoc_mk_html HTMLFILE ADFILE)
	# Guard
	if(NOT ASCIIDOC_CAN_HTML)
		message(FATAL_ERROR "asciidoc can't do html")
	endif()

	# Minimal seatbelt
	set(my_usage "asciidoc_mk_html(<output> <input> [DEPENDS <deps>] [COMMENT <comment>])")
	cmake_parse_arguments(
		_ARGS
		""
		"COMMENT"
		"DEPENDS"
		${ARGN}
	)
	if(_ARGS_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR ${my_usage})
	endif()

	# Always depend on the input file, maybe on more
	set(dependancies ${ADFILE})
	if(_ARGS_DEPENDS)
		list(APPEND dependancies ${_ARGS_DEPENDS})
	endif()

	# Come up with some comment or other
	if(NOT _ARGS_COMMENT)
		get_filename_component(basename ${HTMLFILE} NAME)
		set(_ARGS_COMMENT "Generating ${basename} with asciidoc")
	endif()

	# Setup the rule
	add_custom_command(OUTPUT ${HTMLFILE}
		DEPENDS ${dependancies}
		COMMAND ${ASCIIDOC} -atoc -anumbered -o ${HTMLFILE} ${ADFILE}
		COMMENT ${_ARGS_COMMENT}
	)
endfunction(asciidoc_mk_html)
