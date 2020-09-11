

include(ExternalProject)


set(MESA_ROOT ${CMAKE_SOURCE_DIR}/ports/src/mesa)

chariot_external(libgl ${MESA_ROOT}/lib/libOSMesa.a)
chariot_external(libglu ${MESA_ROOT}/lib/libGLU.a)


