/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "RecoveryProbe.h"
#include <QSslCertificate>
#include <QSslError>
#include <QSslKey>
#include <functional>

class QSslSocket;

namespace deskflow::gui {
struct ProbeIdentity
{
  QList<QSslCertificate> certificates;
  QSslKey key;
  bool valid() const
  {
    return !certificates.isEmpty() && !certificates.first().isNull() && !key.isNull();
  }
};

// A certificate-only connection: it never sends Deskflow input or a client name.
class ConfiguredTlsProbe : public TlsProbeExecutor
{
  Q_OBJECT
public:
  using IdentitySource = std::function<ProbeIdentity()>;
  ConfiguredTlsProbe(QObject *, DiscoveryScopeSource, IdentitySource);
  ~ConfiguredTlsProbe() override;
  void start(const deskflow::auth::Context &, const DiscoveryScope &, const QString &, int) override;
  void cancel() override;
  bool selectPreparedSession(const QString &) override
  {
    return false;
  }
  bool ready() const override;
  bool permitsPort(int port) const override
  {
    return port >= 1024 && port <= 65535;
  }

protected:
  virtual bool allowsEndpoint(const DiscoveryScope &, const QHostAddress &) const;

private:
  DiscoveryScopeSource m_scopes;
  IdentitySource m_identity;
  deskflow::auth::Context m_context;
  DiscoveryScope m_scope;
  QString m_numeric;
  QSslSocket *m_socket = nullptr;
  QTimer m_deadline, m_scopeTimer;
  void finish(bool verified, bool retryable);
};

ProbeIdentity configuredProbeIdentity();
TlsProbeExecutor *createConfiguredTlsProbe(QObject *, DiscoveryScopeSource);
} // namespace deskflow::gui
