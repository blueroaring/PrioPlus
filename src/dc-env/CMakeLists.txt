build_lib(
  LIBNAME dc-env
  SOURCE_FILES
	  model/dc-topology.cc
  HEADER_FILES
	  model/dc-topology.h
  LIBRARIES_TO_LINK
      ${libcore}
      ${libnetwork}
	  ${libinternet}
)
