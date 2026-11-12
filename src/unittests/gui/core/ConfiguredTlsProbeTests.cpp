/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "gui/core/ConfiguredTlsProbe.h"
#include "net/SecureUtils.h"
#include <QFile>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <openssl/pem.h>

using namespace deskflow::gui;
namespace {
QString pin(const ProbeIdentity &identity)
{
  return QString::fromLatin1(identity.certificates.first().digest(QCryptographicHash::Sha256).toHex());
}
ProbeIdentity identity(const QString &path)
{
  deskflow::generatePemSelfSignedCert(path, 2048);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  const auto pem = file.readAll();
  return {QSslCertificate::fromData(pem), QSslKey(pem, QSsl::Rsa)};
}
ProbeIdentity expired(ProbeIdentity value)
{
  const auto certPem = value.certificates.first().toPem();
  auto *bio = BIO_new_mem_buf(certPem.constData(), int(certPem.size()));
  auto *cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  const auto keyPem = value.key.toPem();
  bio = BIO_new_mem_buf(keyPem.constData(), int(keyPem.size()));
  auto *key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (!cert || !key) {
    X509_free(cert);
    EVP_PKEY_free(key);
    return {};
  }
  X509_gmtime_adj(X509_getm_notBefore(cert), -7200);
  X509_gmtime_adj(X509_getm_notAfter(cert), -3600);
  X509_sign(cert, key, EVP_sha256());
  bio = BIO_new(BIO_s_mem());
  PEM_write_bio_X509(bio, cert);
  char *data = nullptr;
  const auto size = BIO_get_mem_data(bio, &data);
  value.certificates = QSslCertificate::fromData(QByteArray(data, int(size)));
  BIO_free(bio);
  X509_free(cert);
  EVP_PKEY_free(key);
  return value;
}
const DiscoveryScope loopback{1, "loopback-fixture", QHostAddress("127.0.0.1"), 8};
class LoopbackProbe : public ConfiguredTlsProbe
{
public:
  using ConfiguredTlsProbe::ConfiguredTlsProbe;

protected:
  bool allowsEndpoint(const DiscoveryScope &scope, const QHostAddress &address) const override
  {
    return scope == loopback && address == QHostAddress("127.0.0.1");
  }
};
class TlsServer : public QTcpServer
{
public:
  ProbeIdentity serverIdentity;
  QString clientPin;
  qint64 applicationBytes = 0;
  void incomingConnection(qintptr descriptor) override
  {
    auto *socket = new QSslSocket(this);
    if (!socket->setSocketDescriptor(descriptor)) {
      socket->deleteLater();
      return;
    }
    socket->setLocalCertificateChain(serverIdentity.certificates);
    socket->setPrivateKey(serverIdentity.key);
    socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    connect(socket, &QSslSocket::sslErrors, socket, [this, socket](const QList<QSslError> &errors) {
      if (QString::fromLatin1(socket->peerCertificate().digest(QCryptographicHash::Sha256).toHex()) == clientPin)
        socket->ignoreSslErrors(errors);
      else
        socket->abort();
    });
    connect(socket, &QSslSocket::readyRead, socket, [this, socket] { applicationBytes += socket->readAll().size(); });
    connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
    socket->startServerEncryption();
  }
};
} // namespace
class ConfiguredTlsProbeTests : public QObject
{
  Q_OBJECT
  QTemporaryDir m_directory;
  ProbeIdentity m_server, m_client;
  deskflow::auth::Context context(const QString &expected)
  {
    return {QString(32, 'a'), expected, 1};
  }
private Q_SLOTS:
  void initTestCase()
  {
    QVERIFY(m_directory.isValid());
    QVERIFY(QSslSocket::supportsSsl());
    m_server = identity(m_directory.filePath("server.pem"));
    m_client = identity(m_directory.filePath("client.pem"));
    QVERIFY(m_server.valid());
    QVERIFY(m_client.valid());
    qRegisterMetaType<deskflow::auth::Context>();
    qRegisterMetaType<deskflow::auth::Report>();
  }
  void realTlsHandshakeVerifiesSavedPinWithoutSendingDeskflowData()
  {
    TlsServer server;
    server.serverIdentity = m_server;
    server.clientPin = pin(m_client);
    QVERIFY(server.listen(QHostAddress::LocalHost));
    LoopbackProbe probe(nullptr, [] { return QVector<DiscoveryScope>{loopback}; }, [this] { return m_client; });
    QVERIFY(probe.ready());
    QSignalSpy verified(&probe, &TlsProbeExecutor::completed), failed(&probe, &TlsProbeExecutor::failed);
    const auto c = context(pin(m_server));
    probe.start(c, loopback, "127.0.0.1", server.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(verified.count(), 1, 6000);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(server.applicationBytes, 0);
    const auto report = qvariant_cast<deskflow::auth::Report>(verified.first()[2]);
    QCOMPARE(report.context, c);
    QCOMPARE(report.outcome, deskflow::auth::Outcome::Verified);
    QCOMPARE(report.observed, c.expected);
  }
  void mismatchedSavedPinFailsWithoutTrustChanges()
  {
    TlsServer server;
    server.serverIdentity = m_server;
    server.clientPin = pin(m_client);
    QVERIFY(server.listen(QHostAddress::LocalHost));
    LoopbackProbe probe(nullptr, [] { return QVector<DiscoveryScope>{loopback}; }, [this] { return m_client; });
    QSignalSpy verified(&probe, &TlsProbeExecutor::completed), failed(&probe, &TlsProbeExecutor::failed);
    probe.start(context(pin(m_client)), loopback, "127.0.0.1", server.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 6000);
    QCOMPARE(verified.count(), 0);
    QVERIFY(!failed.first()[1].toBool());
  }
  void pinnedExpiredCertificateStillFails()
  {
    TlsServer server;
    server.serverIdentity = expired(m_server);
    server.clientPin = pin(m_client);
    QVERIFY(server.serverIdentity.valid());
    QVERIFY(server.listen(QHostAddress::LocalHost));
    LoopbackProbe probe(nullptr, [] { return QVector<DiscoveryScope>{loopback}; }, [this] { return m_client; });
    QSignalSpy verified(&probe, &TlsProbeExecutor::completed), failed(&probe, &TlsProbeExecutor::failed);
    probe.start(context(pin(server.serverIdentity)), loopback, "127.0.0.1", server.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 6000);
    QCOMPARE(verified.count(), 0);
    QVERIFY(!failed.first()[1].toBool());
  }
  void productionPolicyRejectsLoopbackBeforeConnecting()
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    ConfiguredTlsProbe probe(nullptr, [] { return QVector<DiscoveryScope>{loopback}; }, [this] { return m_client; });
    QSignalSpy failed(&probe, &TlsProbeExecutor::failed);
    probe.start(context(pin(m_server)), loopback, "127.0.0.1", server.serverPort());
    QCOMPARE(failed.count(), 1);
    QVERIFY(!server.hasPendingConnections());
  }
  void missingConfiguredIdentityDoesNotConnect()
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    LoopbackProbe probe(nullptr, [] { return QVector<DiscoveryScope>{loopback}; }, [] { return ProbeIdentity{}; });
    QVERIFY(!probe.ready());
    QSignalSpy failed(&probe, &TlsProbeExecutor::failed);
    probe.start(context(pin(m_server)), loopback, "127.0.0.1", server.serverPort());
    QCOMPARE(failed.count(), 1);
    QVERIFY(!server.hasPendingConnections());
  }
  void cancelDiscardsPendingHandshake()
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    LoopbackProbe probe(nullptr, [] { return QVector<DiscoveryScope>{loopback}; }, [this] { return m_client; });
    QSignalSpy failed(&probe, &TlsProbeExecutor::failed), verified(&probe, &TlsProbeExecutor::completed);
    probe.start(context(pin(m_server)), loopback, "127.0.0.1", server.serverPort());
    probe.cancel();
    QTest::qWait(200);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(verified.count(), 0);
  }
  void cancelledHandshakeCannotCompleteANewerAttempt()
  {
    QTcpServer stalled;
    QVERIFY(stalled.listen(QHostAddress::LocalHost));
    TlsServer server;
    server.serverIdentity = m_server;
    server.clientPin = pin(m_client);
    QVERIFY(server.listen(QHostAddress::LocalHost));
    LoopbackProbe probe(nullptr, [] { return QVector<DiscoveryScope>{loopback}; }, [this] { return m_client; });
    QSignalSpy verified(&probe, &TlsProbeExecutor::completed), failed(&probe, &TlsProbeExecutor::failed);
    auto c = context(pin(m_server));
    probe.start(c, loopback, "127.0.0.1", stalled.serverPort());
    ++c.generation;
    probe.start(c, loopback, "127.0.0.1", server.serverPort());
    QTRY_COMPARE_WITH_TIMEOUT(verified.count(), 1, 6000);
    QCOMPARE(qvariant_cast<deskflow::auth::Context>(verified.first()[0]), c);
    QCOMPARE(failed.count(), 0);
  }
  void interfaceRemovalCancelsPendingHandshake()
  {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    bool available = true;
    LoopbackProbe probe(
        nullptr, [&] { return available ? QVector<DiscoveryScope>{loopback} : QVector<DiscoveryScope>{}; },
        [this] { return m_client; }
    );
    QSignalSpy failed(&probe, &TlsProbeExecutor::failed);
    probe.start(context(pin(m_server)), loopback, "127.0.0.1", server.serverPort());
    available = false;
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 1000);
    QVERIFY(failed.first()[1].toBool());
  }
};
QTEST_GUILESS_MAIN(ConfiguredTlsProbeTests)
#include "ConfiguredTlsProbeTests.moc"
