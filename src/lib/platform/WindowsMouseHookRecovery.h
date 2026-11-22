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
  bool observe(std::uintptr_t foreground, std::uint32_t activity, long x, long y)
  {
    const bool sameWindow = foreground == m_foreground;
    const bool moved = x != m_x || y != m_y;
    const bool hookResponded = activity != m_activity;
    m_foreground = foreground;
    m_activity = activity;
    m_x = x;
    m_y = y;

    if (!sameWindow) {
      m_pendingMotion = false;
      m_attempted = false;
      return false;
    }
    if (hookResponded) {
      m_pendingMotion = false;
      return false;
    }
    if (m_pendingMotion && !m_attempted) {
      m_attempted = true;
      return true;
    }
    m_pendingMotion = moved;
    return false;
  }

  void reset()
  {
    *this = {};
  }

private:
  std::uintptr_t m_foreground = 0;
  std::uint32_t m_activity = 0;
  long m_x = 0, m_y = 0;
  bool m_pendingMotion = false;
  bool m_attempted = false;
};

} // namespace deskflow::platform
