// SPDX-FileCopyrightText: (C) 2026 Deskflow Enhanced contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "common/ExitCodes.h"
#include "common/IsolatedSession.h"
#include "gui/core/CoreProcess.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

namespace deskflow::gui {
class CoreRoleTests : public QObject
{
  Q_OBJECT
  QTemporaryDir m_profile;

private Q_SLOTS:
  void initTestCase()
  {
    QVERIFY(m_profile.isValid());
    const auto parent = QFileInfo(m_profile.path()).canonicalFilePath();
    const auto id = QUuid::createUuid().toString(QUuid::Id128);
    const auto root = parent + '/' + id;
    for (const auto *directory : {"config/DeskflowLab/tls", "runtime", "state", "logs"})
      QVERIFY(QDir().mkpath(root + '/' + directory));
    QFile session(root + "/session.json");
    QVERIFY(session.open(QIODevice::WriteOnly));
    const QJsonObject manifest{
        {"schema", 1}, {"purpose", "deskflow-lab-network"}, {"session", id}, {"mode", "client"},
        {"bindAddress", "127.0.0.1"}, {"peerAddress", "127.0.0.1"}, {"port", 34800},
        {"tls", true}, {"peerChecks", true}
    };
    const auto contents = QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    QCOMPARE(session.write(contents), contents.size());
    session.close();
    QVERIFY(deskflow::lab::IsolatedSession::activate(root, parent));
  }

  void changingToServerClearsTheClientPin()
  {
    ServerConfig config;
    CoreProcess core(config);
    const QString savedFingerprint(64, 'a');

    core.setMode(Settings::CoreMode::Client);
    QVERIFY(core.setExpectedPeerFingerprint(savedFingerprint));
    core.setMode(Settings::CoreMode::Client);
    QCOMPARE(core.expectedPeerFingerprint(), savedFingerprint);

    core.setMode(Settings::CoreMode::Server);
    QVERIFY(core.expectedPeerFingerprint().isEmpty());

    core.setMode(Settings::CoreMode::Client);
    QVERIFY(core.setExpectedPeerFingerprint(savedFingerprint));
    QCOMPARE(core.expectedPeerFingerprint(), savedFingerprint);
  }

  void retryAfterLayoutRefusal_data()
  {
    QTest::addColumn<bool>("ipcReady");
    QTest::newRow("reported over IPC") << true;
    QTest::newRow("exited before IPC was ready") << false;
  }

  void retryAfterLayoutRefusal()
  {
    QFETCH(bool, ipcReady);
    ServerConfig config;
    CoreProcess core(config);
    const QString savedFingerprint(64, 'a');
    core.setMode(Settings::CoreMode::Client);
    QVERIFY(core.setExpectedPeerFingerprint(savedFingerprint));
    core.m_startRequested = true;
    core.setProcessState(deskflow::core::ProcessState::Started);
    QVERIFY(!core.setExpectedPeerFingerprint(savedFingerprint));

    QSignalSpy refusal(&core, &CoreProcess::connectionRefused);
    QSignalSpy genericError(&core, &CoreProcess::error);
    if (ipcReady)
      core.onCoreIpcMessageReceived("connectionRefused", "UnknownClient");
    core.onProcessFinished(s_exitUnknownClient, QProcess::NormalExit);

    QCOMPARE(refusal.count(), 1);
    QCOMPARE(
        refusal.at(0).at(0).value<deskflow::core::ConnectionRefusal>(), deskflow::core::ConnectionRefusal::UnknownClient
    );
    QCOMPARE(genericError.count(), 0);
    QCOMPARE(core.processState(), deskflow::core::ProcessState::Stopped);
    QVERIFY(!core.m_retryTimer.isActive());
    QVERIFY(core.m_recoveryBlocked);
    QVERIFY(!core.recoveryPermitted());
    QCOMPARE(core.expectedPeerFingerprint(), savedFingerprint);
    QVERIFY(core.setExpectedPeerFingerprint(savedFingerprint));
  }
};
} // namespace deskflow::gui

QTEST_GUILESS_MAIN(deskflow::gui::CoreRoleTests)
#include "CoreRoleTests.moc"
