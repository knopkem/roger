#
# Try to find OpenCV, and set link libraries for RelWithDebInfo builds if possible. 
#

# Use the "Config" find_package mode.
if (OpenCV_FOUND)
  return()
endif()

find_package(OpenCV REQUIRED NO_MODULE)

foreach( __lib ${OpenCV_LIBS} )
  get_property(__lib_import_configs TARGET "${__lib}" PROPERTY IMPORTED_CONFIGURATIONS)
  list(FIND __lib_import_configs RELWITHDEBINFO __configIdx)
  if(__configIdx LESS 0) 
    set_target_properties(${__lib} PROPERTIES
      MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RELEASE"
    )
  endif()
endforeach()

