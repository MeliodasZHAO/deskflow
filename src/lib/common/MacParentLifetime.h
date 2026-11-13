// SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#pragma once
#ifdef __APPLE__
#include <cerrno>
#include <sys/event.h>
#include <unistd.h>
namespace deskflow::mac {
inline int parentExitDescriptor(pid_t parent)
{
  if (parent <= 1 || getppid() != parent)
    return -1;
  const int fd = kqueue();
  if (fd < 0)
    return -1;
  struct kevent event;
  EV_SET(&event, parent, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, nullptr);
  if (kevent(fd, &event, 1, nullptr, 0, nullptr) < 0 || getppid() != parent) {
    close(fd);
    return -1;
  }
  return fd;
}
inline bool waitForParentExit(int fd, const timespec *timeout = nullptr)
{
  struct kevent event;
  int result;
  do {
    result = kevent(fd, nullptr, 0, &event, 1, timeout);
  } while (result < 0 && errno == EINTR);
  return result == 1 && !(event.flags & EV_ERROR) && (event.fflags & NOTE_EXIT);
}
} // namespace deskflow::mac
#endif
