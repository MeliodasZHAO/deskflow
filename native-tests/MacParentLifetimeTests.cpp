// SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "common/MacParentLifetime.h"
#include <QTest>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
class MacParentLifetimeTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void rejectsNonParents()
  {
    QCOMPARE(deskflow::mac::parentExitDescriptor(1), -1);
    QCOMPARE(deskflow::mac::parentExitDescriptor(getpid()), -1);
  }
  void parentExitReleasesIndependentMonitor_data()
  {
    QTest::addColumn<bool>("crash");
    QTest::newRow("normal-exit") << false;
    QTest::newRow("force-kill") << true;
  }
  void parentExitReleasesIndependentMonitor()
  {
    QFETCH(bool, crash);
    int report[2], gate[2];
    QVERIFY(pipe(report) == 0);
    QVERIFY(pipe(gate) == 0);
    const auto parent = fork();
    QVERIFY(parent >= 0);
    if (parent == 0) {
      close(report[0]);
      close(gate[1]);
      const auto worker = fork();
      if (worker == 0) {
        close(gate[0]);
        const int fd = deskflow::mac::parentExitDescriptor(getppid());
        const timespec immediate{0, 0}, deadline{5, 0};
        const char ready = fd >= 0 && !deskflow::mac::waitForParentExit(fd, &immediate) ? 'R' : 'F';
        (void)write(report[1], &ready, 1);
        const char result = ready == 'R' && deskflow::mac::waitForParentExit(fd, &deadline) ? 'E' : 'F';
        (void)write(report[1], &result, 1);
        if (fd >= 0)
          close(fd);
        _exit(0);
      }
      close(report[1]);
      char command;
      (void)read(gate[0], &command, 1);
      _exit(worker < 0 ? 1 : 0);
    }
    close(report[1]);
    close(gate[0]);
    auto readByte = [&] {
      pollfd descriptor{report[0], POLLIN, 0};
      char value = 'F';
      if (poll(&descriptor, 1, 6000) > 0)
        (void)read(report[0], &value, 1);
      return value;
    };
    const char ready = readByte();
    if (crash || ready != 'R')
      kill(parent, SIGKILL);
    else {
      const char command = 'X';
      (void)write(gate[1], &command, 1);
    }
    const char result = readByte();
    int status;
    (void)waitpid(parent, &status, 0);
    close(gate[1]);
    close(report[0]);
    QCOMPARE(ready, 'R');
    QCOMPARE(result, 'E');
  }
};
QTEST_GUILESS_MAIN(MacParentLifetimeTests)
#include "MacParentLifetimeTests.moc"
