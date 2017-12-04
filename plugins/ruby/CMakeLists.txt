OPTION(DL_RUBY "download libruby from the internet" ON)
IF (DL_RUBY AND NOT APPLE)
    IF (UNIX)
        SET(RUBYLIB ${CMAKE_CURRENT_SOURCE_DIR}/linux${DFHACK_BUILD_ARCH}/libruby.so)
        SET(RUBYLIB_INSTALL_NAME "libruby.so")
        DOWNLOAD_FILE_UNZIP("https://github.com/DFHack/dfhack-bin/releases/download/0.43.05/linux32-libruby187.so.gz"
            "gz"
            ${RUBYLIB}.gz
            "2d06f5069ff07ea934ecd40db55a4ac5"
            ${RUBYLIB}
            "b00d8d7086cb39f6fde793f9d89cb2d7")
    ELSE (UNIX)
        SET(RUBYLIB ${CMAKE_CURRENT_SOURCE_DIR}/win${DFHACK_BUILD_ARCH}/libruby.dll)
        SET(RUBYLIB_INSTALL_NAME "libruby.dll")
        DOWNLOAD_FILE_UNZIP("https://github.com/DFHack/dfhack-bin/releases/download/0.43.05/win32-libruby187.dll.gz"
            "gz"
            ${RUBYLIB}.gz
            "ffc0f1b5b33748e2a36128e90c97f6b2"
            ${RUBYLIB}
            "482c1c418f4ee1a5f04203eee1cda0ef")
    ENDIF(UNIX)
ENDIF(DL_RUBY AND NOT APPLE)

ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl ${dfhack_SOURCE_DIR}/library/include/df/codegen.out.xml ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb
    # cmake quirk: depending on codegen.out.xml or generate_headers only is not enough, needs both
    # test by manually patching any library/xml/moo.xml, run make ruby-autogen-rb -j2, and check build/plugins/ruby/ruby-autogen.rb for patched xml data
    DEPENDS generate_headers ${dfhack_SOURCE_DIR}/library/include/df/codegen.out.xml ${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl
    COMMENT ruby-autogen.rb
)
ADD_CUSTOM_TARGET(ruby-autogen-rb DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb)

INCLUDE_DIRECTORIES("${dfhack_SOURCE_DIR}/depends/tthread")

DFHACK_PLUGIN(ruby ruby.cpp LINK_LIBRARIES dfhack-tinythread)
ADD_DEPENDENCIES(ruby ruby-autogen-rb)

INSTALL(FILES ${RUBYLIB} DESTINATION ${DFHACK_LIBRARY_DESTINATION})

INSTALL(DIRECTORY .
        DESTINATION hack/ruby
        FILES_MATCHING PATTERN "*.rb")

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb DESTINATION hack/ruby)
