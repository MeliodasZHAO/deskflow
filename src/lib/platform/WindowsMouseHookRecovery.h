// SPDX-FileCopyrightText: (C) 2026 Deskflow Enhanced contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#pragma once

#include <cstdint>

namespace deskflow::platform {

// Compare local cursor movement with mouse hook activity. Windows can remove a
// low-level hook without notifying its owner.
class WindowsMouseHookRecovery
{
public:
  void observationUnavailable()
  {
    m_foreground = 0;
    m_pendingMotion = false;
  }

  void cursorCaptured()
  {
    observationUnavailable();
    m_returningFromCapture = true;
  }

  bool observe(std::uintptr_t foreground, std::uint32_t activity, long x, long y)
  {
    const bool havePreviousSample = m_foreground != 0;
    const bool sameWindow = foreground == m_foreground;
    const bool moved = x != m_x || y != m_y;
    const bool hookResponded = havePreviousSample && activity != m_activity;
    m_foreground = foreground;
    m_activity = activity;
    m_x = x;
    m_y = y;

    if (m_retryCooldown > 0)
      --m_retryCooldown;

    // Wait for a stable, unrestricted desktop after game capture. At a screen
    // edge there may be no coordinate change left to detect after capture ends.
    const bool repair = sameWindow && !hookResponded && (m_pendingMotion || m_returningFromCapture) &&
                        m_retryCooldown == 0;
    m_pendingMotion = havePreviousSample && moved && !hookResponded && !repair;
    if (hookResponded || repair)
      m_returningFromCapture = false;
    if (repair)
      m_retryCooldown = kRetryWaitSamples;
    return repair;
  }

  void reset()
  {
    *this = {};
  }

private:
  static constexpr unsigned kRetryWaitSamples = 5; // One second at the desktop polling interval.
  std::uintptr_t m_foreground = 0;
  std::uint32_t m_activity = 0;
  long m_x = 0, m_y = 0;
  bool m_pendingMotion = false;
  bool m_returningFromCapture = false;
  unsigned m_retryCooldown = 0;
};

} // namespace deskflow::platform
