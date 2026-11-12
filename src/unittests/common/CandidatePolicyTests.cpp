// SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "common/CandidatePolicy.h"
#include "common/CredentialStore.h"
#include "common/IsolatedSession.h"
#include "common/ServiceSession.h"
#include "common/Settings.h"
#include "common/WindowsCredentialFiles.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <QXmlStreamReader>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
using namespace deskflow::candidate;
namespace {
QString portableProfile()
{
  return qEnvironmentVariable("DESKFLOW_TEST_ROOT") + "/DeskflowCandidate/DeskflowCandidate.conf";
}
#ifdef Q_OS_WIN
bool everyoneRead(const QString &path, bool allow)
{
  // Only called on synthetic files inside a QTemporaryDir. QFile permissions
  // represent the DOS read-only flag on Windows, not the credential DACL.
  QProcess process;
  process.start(
      qEnvironmentVariable("WINDIR") + "/System32/icacls.exe",
      {QDir::toNativeSeparators(path), allow ? "/grant" : "/remove:g", allow ? "*S-1-1-0:R" : "*S-1-1-0"}
  );
  return process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
bool credentialAlias(const QString &target, const QString &link)
{
#ifdef Q_OS_WIN
  // Hard links need no developer-mode privilege and are rejected by the same
  // native credential-handle check used to reject reparse points.
  return CreateHardLinkW(reinterpret_cast<LPCWSTR>(link.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()), nullptr);
#else
  return QFile::link(target, link);
#endif
}
#endif
} // namespace
class CandidatePolicyTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
  void clearedProbeMetadataDoesNotExhaustRecoveryLimit()
  {
    QTemporaryDir dir;
    const auto root = QFileInfo(dir.path()).canonicalFilePath() + "/DeskflowCandidate";
    QVERIFY(QDir().mkpath(root));
    const auto settings = root + "/DeskflowCandidate.conf", parent = root + "/recovery-sessions";
    QVERIFY(deskflow::lab::CredentialStore::ensurePrivateCandidateDirectory(parent));
    for (int i = 0; i < 130; ++i)
      QVERIFY(QDir().mkdir(parent + '/' + QString::number(i, 16).rightJustified(32, '0')));
    QVERIFY(deskflow::lab::CredentialStore::recoverCandidateSessions(settings));
    const auto bad = parent + '/' + QString(32, 'a');
#ifdef Q_OS_WIN
    // QFile::link creates a .lnk shortcut on Windows, not a directory symlink.
    // An actual damaged pending bundle must still be inspected and refused.
    QVERIFY(QDir().mkdir(bad));
    QFile pending(bad + "/credentials.bundle");
    QVERIFY(pending.open(QIODevice::WriteOnly));
    pending.write("SYNTHETIC-INVALID-STALE-BUNDLE");
    pending.close();
#else
    QVERIFY(QFile::link(root, bad));
#endif
    QVERIFY(!deskflow::lab::CredentialStore::recoverCandidateSessions(settings));
  }
  void candidateProfileReacquiredAfterDestructionAndRevocationRefused()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto root = QFileInfo(dir.path()).canonicalFilePath() + "/DeskflowCandidate";
    QVERIFY(QDir().mkpath(root));
    const auto settings = root + "/DeskflowCandidate.conf", identity = root + "/tls/deskflow-candidate.pem";
    QVERIFY(deskflow::lab::CredentialStore::ensurePrivateCandidateDirectory(root + "/tls"));
    QFile file(identity);
    QVERIFY(deskflow::lab::CredentialStore::openPrivateCandidateFile(file));
    file.write("SYNTHETIC-NOT-A-KEY");
    file.close();
    const auto fp = QString(64, 'a');
    QVERIFY(
        deskflow::lab::CredentialStore::writeCandidateTrust(
            root + "/tls/trusted-servers", ("v2:sha256:" + fp + "\n").toUtf8()
        )
    );
    QString error;
    auto lease = deskflow::lab::CredentialStore::acquireCandidateProfile(settings, identity, fp, &error);
    QVERIFY2(lease, qPrintable(error));
    QCOMPARE(lease->material().identity, QByteArray("SYNTHETIC-NOT-A-KEY"));
    lease.reset();
    auto second = deskflow::lab::CredentialStore::acquireCandidateProfile(settings, identity, fp, &error);
    QVERIFY(second);
    second.reset();
    QVERIFY(
        deskflow::lab::CredentialStore::writeCandidateTrust(
            root + "/tls/trusted-servers", ("v2:sha256:" + QString(64, 'b') + "\n").toUtf8()
        )
    );
    QVERIFY(!deskflow::lab::CredentialStore::acquireCandidateProfile(settings, identity, fp, &error));
    QCOMPARE(error, QString("candidate-identity-not-trusted"));
    QVERIFY(!deskflow::lab::CredentialStore::acquireCandidateProfile(settings, root + "/Deskflow/tls/key", fp, &error));
    QVERIFY(QFile::remove(identity));
    QVERIFY(!deskflow::lab::CredentialStore::acquireCandidateProfile(settings, identity, QString(64, 'b'), &error));
  }
  void profileRejectsBroadAccessAndCredentialAliases()
  {
    QTemporaryDir dir;
    const auto root = QFileInfo(dir.path()).canonicalFilePath() + "/DeskflowCandidate";
    QVERIFY(QDir().mkpath(root));
    QVERIFY(deskflow::lab::CredentialStore::ensurePrivateCandidateDirectory(root + "/tls"));
    const auto settings = root + "/DeskflowCandidate.conf", identity = root + "/tls/deskflow-candidate.pem",
               trust = root + "/tls/trusted-servers", fp = QString(64, 'a');
    QFile f(identity);
    QVERIFY(deskflow::lab::CredentialStore::openPrivateCandidateFile(f));
    f.write("SYNTHETIC-NOT-A-KEY");
    f.close();
    QVERIFY(deskflow::lab::CredentialStore::writeCandidateTrust(trust, ("v2:sha256:" + fp + "\n").toUtf8()));
#ifdef Q_OS_WIN
    QVERIFY(everyoneRead(identity, true));
#else
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadOther));
#endif
    QString error;
    QVERIFY(!deskflow::lab::CredentialStore::acquireCandidateProfile(settings, identity, fp, &error));
#ifdef Q_OS_WIN
    QVERIFY(everyoneRead(identity, false));
#else
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
#endif
    QVERIFY(QFile::remove(trust));
    QVERIFY(credentialAlias(identity, trust));
    QVERIFY(!deskflow::lab::CredentialStore::acquireCandidateProfile(settings, identity, fp, &error));
  }
#else
  void unsupportedCandidateCredentialDirectoryIsNotCreated()
  {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto root = QFileInfo(dir.path()).canonicalFilePath() + "/DeskflowCandidate";
    QVERIFY(QDir().mkpath(root));
    const auto tls = root + "/tls";
    QVERIFY(!deskflow::lab::CredentialStore::ensurePrivateCandidateDirectory(tls));
    QVERIFY(!QFileInfo::exists(tls));
    QVERIFY(!deskflow::lab::CredentialStore::writeCandidateTrust(tls + "/trusted-servers", "synthetic"));
    QVERIFY(!QFileInfo::exists(tls));
  }
#endif
  void serviceSessionBindsContextAndRejectsUnsafePath()
  {
    deskflow::candidate::ServiceSession s{portableProfile(), {QString(32, 'a'), QString(64, 'b'), 9}};
    QVERIFY(s.valid());
    auto copy = deskflow::candidate::ServiceSession::decode(s.encode());
    QCOMPARE(copy.context, s.context);
    QCOMPARE(copy.settings, s.settings);
    QVERIFY(copy.arguments().contains("--peer-auth-context " + s.context.encode()));
    s.settings = "/private/tmp/Deskflow/Deskflow.conf";
    QVERIFY(!s.valid());
    s.settings = "/private/tmp/bad\"path/DeskflowCandidate/DeskflowCandidate.conf";
    QVERIFY(!s.valid());
    QVERIFY(!deskflow::candidate::ServiceSession::decode("garbage").valid());
  }

  void pauseDominatesAutoStart_data()
  {
    QTest::addColumn<bool>("autoStart");
    QTest::addColumn<bool>("paused");
    QTest::addColumn<bool>("expected");
    QTest::newRow("off") << false << false << false;
    QTest::newRow("paused-off") << false << true << false;
    QTest::newRow("resume") << true << false << true;
    QTest::newRow("paused-on") << true << true << false;
  }
  void pauseDominatesAutoStart()
  {
    QFETCH(bool, autoStart);
    QFETCH(bool, paused);
    QFETCH(bool, expected);
    QCOMPARE(mayResume(autoStart, paused), expected);
  }
  void paths_data()
  {
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("ok");
    QTest::newRow("own") << portableProfile() << true;
    for (const auto &v : QStringList{
             "/private/tmp/Deskflow/Deskflow.conf", "/private/tmp/DeskflowCandidate/Deskflow.conf",
             "/private/tmp/Deskflow/DeskflowCandidate.conf", "relative/DeskflowCandidate/DeskflowCandidate.conf",
             "/private/tmp/DeskflowCandidate/../DeskflowCandidate/DeskflowCandidate.conf",
             "//remote/DeskflowCandidate/DeskflowCandidate.conf"
         })
      QTest::newRow(qPrintable(v)) << v << false;
  }
  void paths()
  {
    QFETCH(QString, path);
    QFETCH(bool, ok);
    QCOMPARE(settingsPath(path), ok);
  }
  void refusesLinkedConfigAndCredentials()
  {
    QTemporaryDir d;
    QVERIFY(d.isValid());
    const auto root = QFileInfo(d.path()).canonicalFilePath() + "/DeskflowCandidate";
    QVERIFY(QDir().mkpath(root + "/tls"));
    QFile target(root + "/unrelated");
    QVERIFY(target.open(QIODevice::WriteOnly));
    target.write("not a credential");
    target.close();
    const auto settings = root + "/DeskflowCandidate.conf", cert = root + "/tls/deskflow-candidate.pem";
    QVERIFY(securityProfile(settings, cert, true, true));
    QVERIFY(!securityProfile(settings, cert, false, true));
    QVERIFY(!securityProfile(settings, cert, true, false));
    QVERIFY(!securityProfile(settings, "/private/tmp/Deskflow/tls/deskflow.pem", true, true));
#ifdef Q_OS_WIN
    // Qt creates shortcuts on Windows. Shortcut filenames are not accepted as
    // configuration/credential paths; native hard-link refusal is tested above.
    QVERIFY(QFile::link(target.fileName(), settings + ".lnk"));
    QVERIFY(!settingsPath(settings + ".lnk"));
    QVERIFY(!securityProfile(settings, cert + ".lnk", true, true));
#else
    QVERIFY(QFile::link(target.fileName(), settings));
    QVERIFY(!settingsPath(settings));
    QVERIFY(QFile::remove(settings));
    QVERIFY(QFile::link(target.fileName(), root + "/tls/trusted-servers"));
    QVERIFY(!securityProfile(settings, cert, true, true));
#endif
  }
  void publicPreferencesSurviveFreshIsolatedSettingsProcess()
  {
    const auto parent = qEnvironmentVariable("DESKFLOW_TEST_ROOT");
    QVERIFY(!parent.isEmpty());
    const auto id = QUuid::createUuid().toString(QUuid::Id128);
    const auto root = parent + '/' + id;
    for (const auto *directory : {"config/DeskflowLab/tls", "state", "runtime", "logs"})
      QVERIFY(QDir().mkpath(root + '/' + directory));
    QFile manifest(root + "/session.json");
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(QJsonDocument(
                       QJsonObject{
                           {"schema", 1},
                           {"purpose", "deskflow-lab-network"},
                           {"session", id},
                           {"mode", "client"},
                           {"bindAddress", "127.0.0.1"},
                           {"peerAddress", "127.0.0.1"},
                           {"port", 34800},
                           {"tls", true},
                           {"peerChecks", true}
                       }
    ).toJson());
    manifest.close();
    QSettings settings(root + "/fixture-public.conf", QSettings::IniFormat);
    settings.setValue("gui/userPaused", true);
    settings.setValue("gui/identityBindings", "synthetic-bindings");
    settings.setValue("gui/automaticRecovery", "synthetic-recovery");
    settings.setValue("gui/runtimeProfile", "synthetic-runtime");
    settings.setValue("gui/unknown-obsolete-key", "remove me");
    settings.sync();
    QCOMPARE(settings.status(), QSettings::NoError);
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {"--settings-fixture", root});
    QVERIFY(child.waitForFinished(5000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        child.exitCode() == 0,
        qPrintable(QString::fromUtf8(child.readAllStandardError()) + QString::fromUtf8(child.readAllStandardOutput()))
    );
    settings.sync();
    QVERIFY(!settings.contains("gui/unknown-obsolete-key"));
    QVERIFY(settings.value("gui/userPaused").toBool());
  }
  void privateCandidateFileIsExclusiveAndContainsOnlySyntheticFixture()
  {
    QTemporaryDir d;
    QVERIFY(d.isValid());
    const auto root = QFileInfo(d.path()).canonicalFilePath() + "/DeskflowCandidate";
    QVERIFY(QDir().mkpath(root + "/tls"));
    const auto path = root + "/tls/deskflow-candidate.pem";
    QFile f(path);
    QVERIFY(deskflow::lab::CredentialStore::openPrivateCandidateFile(f));
#ifndef Q_OS_WIN
    const auto permissions = f.permissions();
    QVERIFY(!(permissions & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)));
#endif
    QCOMPARE(f.write("SYNTHETIC-NOT-A-KEY"), qint64(19));
    f.close();
#ifdef Q_OS_WIN
    QByteArray privateBytes;
    QVERIFY(deskflow::lab::WindowsCredentialFiles::readPrivateFile(path, privateBytes));
    QCOMPARE(privateBytes, QByteArray("SYNTHETIC-NOT-A-KEY"));
#endif
    QFile duplicate(path);
    QVERIFY(!deskflow::lab::CredentialStore::openPrivateCandidateFile(duplicate));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("SYNTHETIC-NOT-A-KEY"));
    f.close();
    QFile foreign(root + "/tls/deskflow.pem");
    QVERIFY(!deskflow::lab::CredentialStore::openPrivateCandidateFile(foreign));
    QVERIFY(!foreign.exists());
  }
  void nativeLaunchPlanEscapesPathAndRestartsCrashesOnly()
  {
    const auto bytes = launchAgent(
        qEnvironmentVariable("DESKFLOW_TEST_ROOT") +
        "/candidate & 中文/DeskflowCandidate.app/Contents/MacOS/DeskflowCandidate"
    );
    QVERIFY(!bytes.isEmpty());
    QXmlStreamReader reader(bytes);
    while (!reader.atEnd())
      reader.readNext();
    QVERIFY(!reader.hasError());
    QVERIFY(bytes.contains("&amp;"));
    QVERIFY(bytes.contains("SuccessfulExit"));
    QVERIFY(bytes.contains("<false/>"));
    QVERIFY(bytes.contains("ThrottleInterval"));
    QVERIFY(!bytes.contains("local.deskflow"));
    QVERIFY(launchAgent("/Applications/Deskflow.app/Contents/MacOS/Deskflow").isEmpty());
  }
};
int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  if (argc == 3 && QByteArray(argv[1]) == "--settings-fixture") {
    const auto parent = QFileInfo(qEnvironmentVariable("DESKFLOW_TEST_ROOT")).canonicalFilePath();
    const auto root = QString::fromLocal8Bit(argv[2]);
    if (parent.isEmpty() || QFileInfo(root).canonicalFilePath() != root || QFileInfo(root).absolutePath() != parent ||
        !QRegularExpression("\\A[0-9a-f]{32}\\z").match(QFileInfo(root).fileName()).hasMatch())
      return 2;
    if (!deskflow::lab::IsolatedSession::activate(root, parent)) {
      qCritical().noquote() << "Fixture activation:" << deskflow::lab::IsolatedSession::inspect(root, parent).error;
      return 2;
    }
    const auto fixture = root + "/fixture-public.conf";
    const auto profile = deskflow::lab::IsolatedSession::active()->settings;
    if (!QFile::copy(fixture, profile)) {
      qCritical() << "Fixture profile copy failed";
      return 2;
    }
    qputenv("XDG_CONFIG_HOME", (root + "/config").toUtf8());
    qputenv("XDG_STATE_HOME", (root + "/state").toUtf8());
    const bool ok = Settings::value("gui/userPaused").toBool() &&
                    Settings::value("gui/identityBindings").toString() == "synthetic-bindings" &&
                    Settings::value("gui/automaticRecovery").toString() == "synthetic-recovery" &&
                    Settings::value("gui/runtimeProfile").toString() == "synthetic-runtime" &&
                    !Settings::value("gui/unknown-obsolete-key").isValid();
    Settings::save(false);
    if (!QFile::remove(fixture) || !QFile::copy(profile, fixture)) {
      qCritical() << "Fixture result copy failed";
      return 2;
    }
    return ok ? 0 : 3;
  }
  QTemporaryDir privateRoot;
  if (!privateRoot.isValid()) {
    qCritical().noquote() << "Private fixture directory:" << privateRoot.errorString();
    return 2;
  }
  qputenv("DESKFLOW_TEST_ROOT", QFileInfo(privateRoot.path()).canonicalFilePath().toUtf8());
  CandidatePolicyTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "CandidatePolicyTests.moc"
