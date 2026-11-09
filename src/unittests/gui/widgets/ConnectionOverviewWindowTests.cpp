// SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "common/Settings.h"
#include "gui/MainWindow.h"
#include "gui/widgets/ConnectionHome.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>

using Home = deskflow::gui::ConnectionHome;

class ConnectionOverviewWindowTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void coldStartKeepsControlsInsideTheScrollablePage()
  {
    MainWindow window;
    window.show();
    QTest::qWait(10);
    auto *home = window.findChild<Home *>();
    QVERIFY(home);
    QCOMPARE(home->state().role, Home::Role::Share);
    auto *scroll = home->findChild<QScrollArea *>("homeScroll");
    auto *column = home->findChild<QWidget *>("homeColumn");
    QVERIFY(scroll);
    QVERIFY(column);
    QCOMPARE(column->width(), qMin(880, scroll->viewport()->width()));
    auto *primary = home->findChild<QPushButton *>("homePrimary");
    QVERIFY(primary);
    scroll->ensureWidgetVisible(primary);
    QTest::qWait(1);
    QVERIFY(scroll->viewport()->rect().contains(QRect(primary->mapTo(scroll->viewport(), QPoint()), primary->size())));
    QVERIFY(primary->height() >= primary->sizeHint().height());
    QCOMPARE(home->state().stage, Home::Stage::Idle);
  }

  void connectionSettingsCanReturnToOverview()
  {
    MainWindow window;
    window.show();
    auto *home = window.findChild<Home *>();
    QVERIFY(home);
    QTest::mouseClick(home->findChild<QPushButton *>("sidebarConnection"), Qt::LeftButton);
    QTest::qWait(1);
    auto *back = window.findChild<QPushButton *>("backToOverview");
    QVERIFY(back);
    QVERIFY(back->isVisible());
    QVERIFY(!home->isVisible());
    QTest::mouseClick(back, Qt::LeftButton);
    QVERIFY(home->isVisible());
    QVERIFY(!back->isVisible());
  }

  void roleSelectionUpdatesTheExistingControllerWithoutStartingSharing()
  {
    MainWindow window;
    window.show();
    auto *home = window.findChild<Home *>();
    QVERIFY(home);
    QTest::mouseClick(home->findChild<QPushButton *>("homeChangeDirection"), Qt::LeftButton);
    QTest::mouseClick(home->findChild<QPushButton *>("homeReceive"), Qt::LeftButton);
    QCOMPARE(home->state().role, Home::Role::Receive);
    QCOMPARE(home->state().stage, Home::Stage::Idle);
    QVERIFY(home->findChild<QPushButton *>("homePrimary")->isEnabled());
  }
};

int main(int argc, char **argv)
{
  QTemporaryDir profile;
  if (!profile.isValid())
    return 1;
  qputenv("XDG_CONFIG_HOME", profile.path().toUtf8());
  qputenv("XDG_STATE_HOME", profile.path().toUtf8());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
#ifdef Q_OS_WIN
  const auto configPath = Settings::portableSettingsFile();
  QFile previous(configPath);
  if (previous.exists()) {
    if (!previous.open(QIODevice::ReadOnly) || !previous.readLine().startsWith("; ConnectionOverviewWindowTests"))
      return 1;
    previous.close();
  }
#else
  const auto configPath = profile.path() + "/Deskflow/Deskflow.conf";
#endif
  QDir().mkpath(QFileInfo(configPath).absolutePath());
  QFile config(configPath);
  if (!config.open(QIODevice::WriteOnly))
    return 1;
  config.write(
      "; ConnectionOverviewWindowTests\n[core]\ncomputerName=overview-test\ncoreMode=2\nlanguage=en\n"
      "[security]\ntlsEnabled=false\n[gui]\nstartCoreWithGui=false\nenableUpdateCheck=false\n"
      "[client]\nremoteHost=example.invalid\n"
  );
  config.close();
  ConnectionOverviewWindowTests tests;
  const auto result = QTest::qExec(&tests, argc, argv);
#ifdef Q_OS_WIN
  QFile::remove(configPath);
#endif
  return result;
}

#include "ConnectionOverviewWindowTests.moc"
