/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "milliygram/mg_settings.h"

#include "settings.h"

#include <QtCore/QSettings>

namespace MilliyGram {
namespace {

[[nodiscard]] QSettings &Storage() {
	static auto result = QSettings(
		cWorkingDir() + u"tdata/milliygram.ini"_q,
		QSettings::IniFormat);
	return result;
}

[[nodiscard]] rpl::variable<bool> &Ghost() {
	static auto result = rpl::variable<bool>(
		Storage().value(u"ghost/enabled"_q, false).toBool());
	return result;
}

} // namespace

bool GhostEnabled() {
	return Ghost().current();
}

void SetGhostEnabled(bool enabled) {
	if (Ghost().current() == enabled) {
		return;
	}
	Storage().setValue(u"ghost/enabled"_q, enabled);
	Storage().sync();
	Ghost() = enabled;
}

rpl::producer<bool> GhostValue() {
	return Ghost().value();
}

QString ForwardSignature() {
	return Storage().value(u"forward/signature"_q).toString();
}

void SetForwardSignature(const QString &signature) {
	Storage().setValue(u"forward/signature"_q, signature);
	Storage().sync();
}

} // namespace MilliyGram
