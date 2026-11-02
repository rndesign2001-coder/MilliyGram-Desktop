/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace MilliyGram {

inline constexpr auto kVersion = "1.1.0";

// Sharpa rejimi: o'qilganlik, "yozmoqda…", onlayn holat va hikoya ko'rishlari serverga yuborilmaydi.
[[nodiscard]] bool GhostEnabled();
void SetGhostEnabled(bool enabled);
[[nodiscard]] rpl::producer<bool> GhostValue();

} // namespace MilliyGram
