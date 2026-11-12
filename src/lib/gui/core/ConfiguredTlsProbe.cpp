/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "ConfiguredTlsProbe.h"
#include "common/Settings.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkProxy>
#include <QSslConfiguration>
#include <QSslSocket>
#ifndef Q_OS_WIN
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace deskflow::gui {
namespace {
QString fingerprint(const QSslCertificate &certificate)
{
  return QString::fromLatin1(certificate.digest(QCryptographicHash::Sha256).toHex());
}

bool certificateIsCurrent(const QSslCertificate &certificate)
{
  const auto now = QDateTime::currentDateTimeUtc();
  return !certificate.isNull() && certificate.effectiveDate().isValid() && certificate.expiryDate().isValid() &&
         certificate.effectiveDate() <= now && now < certificate.expiryDate();
}

bool pinnedCertificateErrors(const QList<QSslError> &errors)
{
  for (const auto &error : errors) {
    switch (error.error()) {
    case QSslError::SelfSignedCertificate:
    case QSslError::SelfSignedCertificateInChain:
    case QSslError::UnableToGetLocalIssuerCertificate:
    case QSslError::UnableToVerifyFirstCertificate:
    case QSslError::CertificateUntrusted:
    case QSslError::HostNameMismatch:
      break;
    default:
      return false;
    }
  }
  return true;
}
} // namespace

ProbeIdentity configuredProbeIdentity()
{
  if (!Settings::value(Settings::Security::TlsEnabled).toBool() ||
      !Settings::value(Settings::Security::CheckPeers).toBool())
    return {};
  const auto path = Settings::value(Settings::Security::Certificate).toString();
  QByteArray pem;
#ifdef Q_OS_WIN
  const QFileInfo info(path);
  if (!QDir::isAbsolutePath(path) || info.isSymLink() || !info.isFile() ||
      info.canonicalFilePath().compare(QDir::cleanPath(QDir::fromNativeSeparators(path)), Qt::CaseInsensitive) != 0)
    return {};
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 65536)
    return {};
  pem = file.read(65537);
  if (pem.size() > 65536 || file.error() != QFileDevice::NoError)
    return {};
#else
  const auto encoded = QFile::encodeName(path);
  const int fd = ::open(encoded.constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0)
    return {};
  struct stat status{};
  if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_uid != geteuid() || (status.st_mode & 0022) ||
      status.st_size <= 0 || status.st_size > 65536) {
    close(fd);
    return {};
  }
  QFile file;
  if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    close(fd);
    return {};
  }
  pem = file.read(65537);
  if (pem.size() > 65536 || file.error() != QFileDevice::NoError)
    return {};
#endif
  const auto certificates = QSslCertificate::fromData(pem);
  QSslKey key(pem, QSsl::Rsa);
  if (key.isNull())
    key = QSslKey(pem, QSsl::Ec);
  return {certificates, key};
}

ConfiguredTlsProbe::ConfiguredTlsProbe(QObject *parent, DiscoveryScopeSource scopes, IdentitySource identity)
    : TlsProbeExecutor(parent),
      m_scopes(std::move(scopes)),
      m_identity(std::move(identity))
{
  m_deadline.setSingleShot(true);
  m_deadline.setInterval(5000);
  connect(&m_deadline, &QTimer::timeout, this, [this] { finish(false, true); });
  m_scopeTimer.setInterval(100);
  connect(&m_scopeTimer, &QTimer::timeout, this, [this] {
    if (!m_scopes().contains(m_scope))
      finish(false, true);
  });
}
ConfiguredTlsProbe::~ConfiguredTlsProbe()
{
  cancel();
}

bool ConfiguredTlsProbe::ready() const
{
  const auto identity = m_identity ? m_identity() : ProbeIdentity{};
  return QSslSocket::supportsSsl() && identity.valid() && certificateIsCurrent(identity.certificates.first());
}

bool ConfiguredTlsProbe::allowsEndpoint(const DiscoveryScope &scope, const QHostAddress &address) const
{
  return validProbeAddress(scope, address);
}

void ConfiguredTlsProbe::cancel()
{
  m_context = {};
  m_deadline.stop();
  m_scopeTimer.stop();
  if (auto *socket = m_socket) {
    m_socket = nullptr;
    disconnect(socket, nullptr, this, nullptr);
    socket->abort();
    socket->deleteLater();
  }
}

void ConfiguredTlsProbe::finish(bool verified, bool retryable)
{
  if (!m_context.valid())
    return;
  const auto context = m_context;
  const auto numeric = m_numeric;
  cancel();
  if (verified)
    Q_EMIT completed(context, numeric, {context, 2, deskflow::auth::Outcome::Verified, context.expected});
  else
    Q_EMIT failed(context, retryable);
}

void ConfiguredTlsProbe::start(
    const deskflow::auth::Context &context, const DiscoveryScope &scope, const QString &numeric, int port
)
{
  cancel();
  if (!context.valid())
    return;
  m_context = context;
  m_scope = scope;
  m_numeric = numeric;
  const QHostAddress address(numeric);
  if (context.expected.isEmpty() || !permitsPort(port) || !m_scopes().contains(scope) ||
      !allowsEndpoint(scope, address)) {
    finish(false, false);
    return;
  }
  const auto identity = m_identity ? m_identity() : ProbeIdentity{};
  if (!identity.valid() || !certificateIsCurrent(identity.certificates.first()) || !QSslSocket::supportsSsl()) {
    finish(false, false);
    return;
  }
  auto *socket = new QSslSocket(this);
  m_socket = socket;
  socket->setReadBufferSize(4096);
  socket->setProxy(QNetworkProxy::NoProxy);
  auto config = QSslConfiguration::defaultConfiguration();
  config.setProtocol(QSsl::TlsV1_2OrLater);
  config.setPeerVerifyMode(QSslSocket::VerifyPeer);
  config.setLocalCertificateChain(identity.certificates);
  config.setPrivateKey(identity.key);
  socket->setSslConfiguration(config);
  connect(socket, &QSslSocket::sslErrors, this, [this, socket](const QList<QSslError> &errors) {
    // Deskflow trusts an explicitly saved certificate, rather than a DNS name
    // or public CA. Unrelated certificate failures remain fatal.
    if (socket != m_socket)
      return;
    if (!certificateIsCurrent(socket->peerCertificate()) ||
        fingerprint(socket->peerCertificate()) != m_context.expected || !pinnedCertificateErrors(errors)) {
      finish(false, false);
      return;
    }
    socket->ignoreSslErrors(errors);
  });
  connect(socket, &QAbstractSocket::connected, this, [this, socket] {
    if (socket != m_socket)
      return;
    if (!m_scopes().contains(m_scope) || socket->localAddress() != m_scope.address ||
        socket->peerAddress() != QHostAddress(m_numeric)) {
      finish(false, true);
      return;
    }
    socket->startClientEncryption();
  });
  connect(socket, &QSslSocket::encrypted, this, [this, socket] {
    if (socket != m_socket)
      return;
    const bool verified =
        socket == m_socket && m_scopes().contains(m_scope) && socket->localAddress() == m_scope.address &&
        socket->peerAddress() == QHostAddress(m_numeric) && certificateIsCurrent(socket->peerCertificate()) &&
        fingerprint(socket->peerCertificate()) == m_context.expected;
    finish(verified, !verified && !m_scopes().contains(m_scope));
  });
  connect(socket, &QAbstractSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError error) {
    if (socket == m_socket)
      finish(false, error != QAbstractSocket::SslHandshakeFailedError);
  });
  connect(socket, &QAbstractSocket::disconnected, this, [this, socket] {
    if (socket == m_socket)
      finish(false, true);
  });
  if (!socket->bind(scope.address, 0)) {
    finish(false, true);
    return;
  }
  m_deadline.start();
  m_scopeTimer.start();
  socket->connectToHost(numeric, quint16(port), QIODevice::ReadWrite, QAbstractSocket::IPv4Protocol);
}

TlsProbeExecutor *createConfiguredTlsProbe(QObject *parent, DiscoveryScopeSource scopes)
{
  return new ConfiguredTlsProbe(parent, std::move(scopes), configuredProbeIdentity);
}
} // namespace deskflow::gui
