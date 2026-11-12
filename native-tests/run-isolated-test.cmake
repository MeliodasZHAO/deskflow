# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-License-Identifier: MIT
if(NOT DEFINED TEST_EXECUTABLE OR NOT EXISTS "${TEST_EXECUTABLE}")
  message(FATAL_ERROR "An existing isolated test executable is required")
endif()
if(WIN32)
  set(tempRoot "$ENV{TEMP}")
elseif(DEFINED ENV{TMPDIR} AND NOT "$ENV{TMPDIR}" STREQUAL "")
  set(tempRoot "$ENV{TMPDIR}")
else()
  set(tempRoot "/tmp")
endif()
if(NOT IS_DIRECTORY "${tempRoot}")
  message(FATAL_ERROR "Temporary directory is unavailable")
endif()
file(REAL_PATH "${tempRoot}" tempRoot)
string(RANDOM LENGTH 32 ALPHABET 0123456789abcdef id)
set(profile "${tempRoot}/df-offline-${id}")
file(MAKE_DIRECTORY "${profile}/config/DeskflowLab" "${profile}/state")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
  "QT_QPA_PLATFORM=offscreen" "DESKFLOW_TEST_ROOT=${profile}"
  "XDG_CONFIG_HOME=${profile}/config" "XDG_STATE_HOME=${profile}/state"
  "${TEST_EXECUTABLE}"
  RESULT_VARIABLE result TIMEOUT 90)
file(REMOVE_RECURSE "${profile}")
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Isolated test failed: ${result}")
endif()
