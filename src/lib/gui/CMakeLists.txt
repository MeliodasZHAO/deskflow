# SPDX-FileCopyrightText: (C) 2024 - 2026 Deskflow Developers
# SPDX-FileCopyrightText: (C) 2024 Symless Ltd
# SPDX-License-Identifier: MIT

set(target gui)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(APPLE)
  set (platform_sources
    OSXHelpers.h
    OSXHelpers.mm
  )
endif()

add_library(${target} STATIC
  ${platform_sources}
  ComputerSetupModel.cpp
  ComputerSetupModel.h
  Diagnostic.cpp
  Diagnostic.h
  FileTail.cpp
  FileTail.h
  Logger.cpp
  Logger.h
  MainWindow.cpp
  MainWindow.h
  MainWindow.ui
  Messages.cpp
  Messages.h
  StyleUtils.h
  TlsUtility.cpp
  TlsUtility.h
  VersionChecker.cpp
  VersionChecker.h
  config/ComputerConfig.h
  config/Computer.cpp
  config/Computer.h
  config/ComputerList.cpp
  config/ComputerList.h
  config/ServerConfig.cpp
  config/ServerConfig.h
  core/CoreProcess.cpp
  core/CoreProcess.h
  core/NetworkMonitor.cpp
  core/NetworkMonitor.h
  dialogs/AboutDialog.cpp
  dialogs/AboutDialog.h
  dialogs/AboutDialog.ui
  dialogs/ActionDialog.cpp
  dialogs/ActionDialog.h
  dialogs/ActionDialog.ui
  dialogs/ClientConfigDialog.h
  dialogs/ClientConfigDialog.cpp
  dialogs/ClientConfigDialog.ui
  dialogs/ComputerSettingsDialog.cpp
  dialogs/ComputerSettingsDialog.h
  dialogs/ComputerSettingsDialog.ui
  dialogs/FingerprintDialog.cpp
  dialogs/FingerprintDialog.h
  dialogs/HelpDialog.cpp
  dialogs/HelpDialog.h
  dialogs/HotkeyDialog.cpp
  dialogs/HotkeyDialog.h
  dialogs/HotkeyDialog.ui
  dialogs/ServerConfigDialog.cpp
  dialogs/ServerConfigDialog.h
  dialogs/ServerConfigDialog.ui
  dialogs/SettingsDialog.cpp
  dialogs/SettingsDialog.h
  dialogs/SettingsDialog.ui
  ipc/IpcClient.cpp
  ipc/IpcClient.h
  ipc/DaemonIpcClient.cpp
  ipc/DaemonIpcClient.h
  ipc/CoreIpcClient.cpp
  ipc/CoreIpcClient.h
  validators/AliasValidator.cpp
  validators/AliasValidator.h
  validators/ComputerDuplicationsValidator.cpp
  validators/ComputerDuplicationsValidator.h
  validators/ComputerLineNameValidator.cpp
  validators/ComputerLineNameValidator.h
  validators/ComputerNameValidator.cpp
  validators/ComputerNameValidator.h
  validators/EmptyStringValidator.cpp
  validators/EmptyStringValidator.h
  validators/IStringValidator.cpp
  validators/IStringValidator.h
  validators/LineEditValidator.cpp
  validators/LineEditValidator.h
  validators/SpacesValidator.cpp
  validators/SpacesValidator.h
  validators/ValidationError.cpp
  validators/ValidationError.h
  validators/IpAddressValidator.cpp
  validators/IpAddressValidator.h
  widgets/ComputerSetupView.cpp
  widgets/ComputerSetupView.h
  widgets/FingerprintPreview.cpp
  widgets/FingerprintPreview.h
  widgets/KeySequenceWidget.cpp
  widgets/KeySequenceWidget.h
  widgets/LogDock.cpp
  widgets/LogDock.h
  widgets/LogWidget.h
  widgets/LogWidget.cpp
  widgets/NewComputerWidget.cpp
  widgets/NewComputerWidget.h
  widgets/RemoveComputerWidget.cpp
  widgets/RemoveComputerWidget.h
  widgets/SearchWidget.h
  widgets/SearchWidget.cpp
  widgets/StatusBar.cpp
  widgets/StatusBar.h
  widgets/SettingsDialogButtonBox.cpp
  widgets/SettingsDialogButtonBox.h
)

target_link_libraries(${target}
  PUBLIC
    common
    platform
    net
    Qt6::Widgets
)

target_sources(gui PRIVATE
  ../experience/LayoutModel.cpp
  ../experience/LayoutModel.h
  DialogPresentation.h
  ExperienceStyle.h
  config/DesktopLayout.cpp
  config/DesktopLayout.h
  config/MonitorLayoutDraft.cpp
  config/MonitorLayoutDraft.h
  config/MonitorTopology.cpp
  config/MonitorTopology.h
  dialogs/DesktopLayoutEditor.cpp
  dialogs/DesktopLayoutEditor.h
  dialogs/DisplayTopologyDialog.cpp
  dialogs/DisplayTopologyDialog.h
  widgets/ActionButton.h
  widgets/DesktopWorkspaceView.cpp
  widgets/DesktopWorkspaceView.h
  widgets/FlowLayout.h
  widgets/OptionControl.h
)

target_sources(gui PRIVATE
  IdentityBindings.cpp
  IdentityBindings.h
  RecentServerHistory.cpp
  RecentServerHistory.h
  core/ConfiguredTlsProbe.cpp
  core/ConfiguredTlsProbe.h
  core/DeviceRecovery.cpp
  core/DeviceRecovery.h
  core/DiscoveryCatalog.cpp
  core/DiscoveryCatalog.h
  core/DiscoveryScope.cpp
  core/DiscoveryScope.h
  core/DiscoveryTransport.h
  core/DiscoveryTransportMac.cpp
  core/DiscoveryTransportWin.cpp
  core/NearbyDiscovery.cpp
  core/NearbyDiscovery.h
  core/NearbyPublisher.cpp
  core/NearbyPublisher.h
  core/RecoveryProbe.cpp
  core/RecoveryProbe.h
  core/ScopedAddressResolver.cpp
  dialogs/NearbyDevicesDialog.cpp
  dialogs/NearbyDevicesDialog.h
  dialogs/PairingReviewDialog.cpp
  dialogs/PairingReviewDialog.h
  dialogs/TrustedDevicesDialog.cpp
  dialogs/TrustedDevicesDialog.h
  widgets/RecentServersButton.cpp
  widgets/RecentServersButton.h
)
if(WIN32)
 target_link_libraries(gui PRIVATE dnsapi iphlpapi)
 target_compile_definitions(gui PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0601 NTDDI_VERSION=0x06010000)
endif()

if(APPLE)
 target_link_libraries(gui PRIVATE "-framework SystemConfiguration" "-framework CoreFoundation")
endif()

target_sources(gui PRIVATE
  core/AutomaticRecovery.cpp
  core/AutomaticRecovery.h
  core/PowerMonitor.cpp
  core/PowerMonitor.h
  dialogs/AutomaticRecoveryDialog.cpp
  dialogs/AutomaticRecoveryDialog.h
  dialogs/DiagnosticExportDialog.cpp
  dialogs/DiagnosticExportDialog.h
  widgets/ConnectionDiagnostics.cpp
  widgets/ConnectionDiagnostics.h
)
if(APPLE)
 target_link_libraries(gui PRIVATE "-framework SystemConfiguration" "-framework CoreFoundation")
endif()
