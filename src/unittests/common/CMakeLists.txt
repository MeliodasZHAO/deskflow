# SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

create_test(
  NAME SettingsTests
  DEPENDS common
  SOURCE SettingsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME LogLevelTests
  DEPENDS common
  SOURCE LogLevelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME I18NTests
  DEPENDS common
  SOURCE I18NTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

create_test(
  NAME KeySequenceTests
  DEPENDS common
  SOURCE KeySequenceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common"
)

foreach(displayTest DisplayRoutingTests DesktopWorkspaceTests)
  create_test(NAME ${displayTest} DEPENDS common LIBS Qt6::Gui
    SOURCE ${displayTest}.cpp WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common")
  set_tests_properties(${displayTest} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
endforeach()

create_test(NAME RuntimeProfileTests DEPENDS common SOURCE RuntimeProfileTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/common")
