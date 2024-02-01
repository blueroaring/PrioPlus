check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

if(${Boost_FOUND})
  set(LibsToLink
    ${libcore}
    ${libnetwork}
    ${libpoint-to-point}
    ${libapplications}
    ${libdc-env}
    ${libdcb}
    ${Boost_LIBRARIES})
  if(${ENABLE_MTP})
    list(APPEND LibsToLink ${libmtp})
  endif()
  build_lib(
    LIBNAME json-util
    SOURCE_FILES
        model/json-util.cc
        helper/json-topology-helper.cc
    HEADER_FILES
        model/json-util.h
        helper/json-topology-helper.h
    LIBRARIES_TO_LINK
        ${LibsToLink}
)
endif()
