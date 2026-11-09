/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace MilliyGram {

inline constexpr auto kVersion = "1.3.0";

// Sharpa rejimi: o'qilganlik, "yozmoqda…", onlayn holat va hikoya ko'rishlari serverga yuborilmaydi.
[[nodiscard]] bool GhostEnabled();
void SetGhostEnabled(bool enabled);
[[nodiscard]] rpl::producer<bool> GhostValue();

// Suzuvchi yumaloq panellar (chatlar ro'yxati, chat va ma'lumot paneli orasida bo'shliq).
[[nodiscard]] bool FloatingPanels();
void SetFloatingPanels(bool enabled);
[[nodiscard]] rpl::producer<bool> FloatingPanelsValue();

// Maxsus uzatishdagi "Imzo" (izoh oxiriga qo'shiladigan matn).
[[nodiscard]] QString ForwardSignature();
void SetForwardSignature(const QString &signature);

} // namespace MilliyGram
