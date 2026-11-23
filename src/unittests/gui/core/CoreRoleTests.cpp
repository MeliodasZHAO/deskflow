// SPDX-FileCopyrightText: (C) 2026 Deskflow Enhanced contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "common/IsolatedSession.h"
#include "gui/core/CoreProcess.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

class CoreRoleTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void changingToServerClearsTheClientPin()
  {
    QTemporaryDir profile;
    QVERIFY(profile.isValid());
    const auto parent = QFileInfo(profile.path()).canonicalFilePath();
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
    ServerConfig config;
    deskflow::gui::CoreProcess core(config);
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
};

QTEST_GUILESS_MAIN(CoreRoleTests)
#include "CoreRoleTests.moc"
