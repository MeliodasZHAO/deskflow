# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME NetworkMonitorTests
  DEPENDS gui
  SOURCE NetworkMonitorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME IpcLifecycleTests
  DEPENDS gui
  LIBS app client server platform
  SOURCE IpcLifecycleTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ClientAppRecoveryTests
  DEPENDS app
  LIBS client server platform Qt6::Widgets
  SOURCE ClientAppRecoveryTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/deskflow"
)

find_package(OpenSSL 3.0 REQUIRED COMPONENTS SSL Crypto)
create_test(
  NAME ConfiguredTlsProbeTests
  DEPENDS gui
  LIBS OpenSSL::SSL OpenSSL::Crypto
  SOURCE ConfiguredTlsProbeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
set_tests_properties(ConfiguredTlsProbeTests PROPERTIES TIMEOUT 30)
