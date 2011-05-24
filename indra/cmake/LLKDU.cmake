# -*- cmake -*-

# If You Want to Enable KDU set this in develop.py    -DUSE_KDU:BOOL=ON
# KDU Will Be Downloaded And Linked. No DLL Will Be Available
if (INSTALL_PROPRIETARY)
  set(USE_KDU ON CACHE BOOL "Use Kakadu library.")
endif (INSTALL_PROPRIETARY)

if (USE_KDU)
  include(Prebuilt)
  use_prebuilt_binary(kdu)
  if (WINDOWS)
    set(KDU_LIBRARY kdu.lib)
  else (WINDOWS)
    set(KDU_LIBRARY libkdu.a)
  endif (WINDOWS)
  set(KDU_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/kdu)
  set(LLKDU_INCLUDE_DIRS ${LIBS_OPEN_DIR}/llkdu)
  set(LLKDU_LIBRARIES llkdu)
endif (USE_KDU)
