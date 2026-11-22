// SPDX-FileCopyrightText: (C) 2026 Deskflow Enhanced contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#include "platform/WindowsMouseHookRecovery.h"

#include <QTest>

using deskflow::platform::WindowsMouseHookRecovery;

class WindowsMouseHookRecoveryTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void recoversFromCaptureAfterTheDesktopSettles()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    recovery.cursorCaptured();
    QVERIFY(!recovery.observe(2, 20, 10, 10));
    QVERIFY(recovery.observe(2, 20, 10, 10));
    for (int i = 0; i < 10; ++i)
      QVERIFY(!recovery.observe(2, 20, 10, 10));
  }

  void skipsRecoveryWhenCallbacksResumeAfterCapture()
  {
    WindowsMouseHookRecovery recovery;
    recovery.cursorCaptured();
    QVERIFY(!recovery.observe(1, 20, 10, 10));
    QVERIFY(!recovery.observe(1, 21, 20, 10));
    QVERIFY(!recovery.observe(1, 21, 20, 10));
  }

  void retainsCaptureWhenTheForegroundWindowDisappears()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    recovery.cursorCaptured();
    recovery.observationUnavailable();
    recovery.observationUnavailable();
    QVERIFY(!recovery.observe(2, 9, 10, 10));
    QVERIFY(recovery.observe(2, 9, 10, 10));
    QVERIFY(!recovery.observe(2, 9, 10, 10));
  }

  void discardsMotionWithoutAMatchingObservation()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    QVERIFY(!recovery.observe(1, 9, 20, 10));
    recovery.observationUnavailable();
    QVERIFY(!recovery.observe(2, 9, 20, 10));
    QVERIFY(!recovery.observe(2, 9, 20, 10));
  }

  void recoversWhenTheFirstMoveAfterSwitchingWindowsReachesTheEdge()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 0, 0));
    QVERIFY(!recovery.observe(2, 9, 10, 10));
    QVERIFY(recovery.observe(2, 9, 10, 10));
  }

  void retriesWithFreshEvidenceInTheSameWindow()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 0, 0));
    QVERIFY(!recovery.observe(1, 9, 10, 0));
    QVERIFY(recovery.observe(1, 9, 10, 0));
    for (long x = 11; x <= 14; ++x)
      QVERIFY(!recovery.observe(1, 9, x, 0));
    QVERIFY(recovery.observe(1, 9, 14, 0));

    // Idle polls cannot cause another installation. After callbacks resume,
    // a later interruption in the same window can still be repaired.
    QVERIFY(!recovery.observe(1, 10, 15, 0));
    for (int i = 0; i < 10; ++i)
      QVERIFY(!recovery.observe(1, 10, 15, 0));
    QVERIFY(!recovery.observe(1, 10, 20, 0));
    QVERIFY(recovery.observe(1, 10, 20, 0));
  }

  void neverRepairsAnIdleOrResponsiveHook()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    QVERIFY(!recovery.observe(2, 9, 10, 10));
    QVERIFY(!recovery.observe(2, 10, 20, 10));
    QVERIFY(!recovery.observe(2, 10, 20, 10));
    QVERIFY(!recovery.observe(2, 11, 30, 10));
    QVERIFY(!recovery.observe(2, 11, 30, 10));
  }

  void discardsEvidenceWhenCaptureIsOutOfScope()
  {
    WindowsMouseHookRecovery recovery;
    QVERIFY(!recovery.observe(1, 9, 10, 10));
    QVERIFY(!recovery.observe(1, 9, 20, 10));
    recovery.cursorCaptured();
    recovery.reset(); // Secure desktop, screen saver, or input routed to the peer.
    QVERIFY(!recovery.observe(1, 9, 25, 10));
    QVERIFY(!recovery.observe(1, 9, 25, 10));
  }
};

QTEST_GUILESS_MAIN(WindowsMouseHookRecoveryTests)
#include "WindowsMouseHookRecoveryTests.moc"
