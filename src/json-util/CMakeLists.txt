check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

if(${Boost_FOUND})
build_lib(
    LIBNAME json-util
    SOURCE_FILES
        helper/json-topology-helper.cc
        helper/json-application-helper.cc
        model/json-configs.cc
        model/json-outputs.cc
        utils/json-utils.cc
    HEADER_FILES
        helper/json-topology-helper.h
        helper/json-application-helper.h
        model/json-configs.h
        model/json-outputs.h
        utils/json-utils.h
    LIBRARIES_TO_LINK
        ${libcore}
        ${libnetwork}
        ${libpoint-to-point}
        ${libapplications}
        ${libdc-env}
        ${libdcb}
        ${Boost_LIBRARIES}
)
endif()
