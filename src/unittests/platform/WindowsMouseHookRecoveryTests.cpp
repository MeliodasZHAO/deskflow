// SPDX-FileCopyrightText: (C) 2026 Deskflow Enhanced contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#include "platform/WindowsMouseHookRecovery.h"

#include <QTest>

using deskflow::platform::WindowsMouseHookRecovery;

class WindowsMouseHookRecoveryTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void recoversOnceAfterSwitchingToAnOrdinaryWindow()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 0, 0));
    QVERIFY(!recovery.observe(2, 9, 10, 10));
    QVERIFY(!recovery.observe(2, 9, 20, 10));
    QVERIFY(recovery.observe(2, 9, 20, 10));
    QVERIFY(!recovery.observe(2, 9, 25, 10));
    QVERIFY(!recovery.observe(2, 9, 30, 10));
    QVERIFY(!recovery.observe(3, 9, 30, 10));
    QVERIFY(!recovery.observe(3, 9, 40, 10));
    QVERIFY(recovery.observe(3, 9, 40, 10));
  }

  void neverRepairsAnIdleOrResponsiveHook()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    QVERIFY(!recovery.observe(2, 9, 10, 10));
    QVERIFY(!recovery.observe(2, 9, 20, 10));
    QVERIFY(!recovery.observe(2, 10, 20, 10));
    QVERIFY(!recovery.observe(2, 11, 30, 10));
    QVERIFY(!recovery.observe(2, 11, 30, 10));
  }

  void discardsEvidenceWhenCaptureIsOutOfScope()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    QVERIFY(!recovery.observe(1, 9, 20, 10));
    recovery.reset(); // Cursor confined, secure desktop, or input routed to the peer.
    QVERIFY(!recovery.observe(1, 9, 25, 10));
    QVERIFY(!recovery.observe(1, 9, 25, 10));
  }
};

QTEST_GUILESS_MAIN(WindowsMouseHookRecoveryTests)
#include "WindowsMouseHookRecoveryTests.moc"
