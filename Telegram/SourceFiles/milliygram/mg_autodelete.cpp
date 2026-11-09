/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "milliygram/mg_autodelete.h"

#include "base/timer.h"
#include "base/unixtime.h"
#include "data/data_histories.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "history/history.h"
#include "main/main_session.h"
#include "settings.h"

#include <QtCore/QSettings>
#include <QtCore/QStringList>

namespace MilliyGram {
namespace {

constexpr auto kRangeLifetime = crl::time(30 * 60 * 1000);
constexpr auto kMaxTimerDelay = TimeId(60 * 60);

[[nodiscard]] QSettings &Storage() {
	static auto result = QSettings(
		cWorkingDir() + u"tdata/milliygram.ini"_q,
		QSettings::IniFormat);
	return result;
}

class Manager final {
public:
	explicit Manager(not_null<Main::Session*> session);

	void addRange(MsgId from, MsgId till, TimeId ttl);

private:
	struct Range {
		MsgId from = MsgId(0);
		MsgId till = MsgId(0);
		TimeId ttl = 0;
		crl::time added = 0;
	};
	struct Entry {
		PeerId peer = PeerId(0);
		MsgId msg = MsgId(0);
		TimeId at = 0;
	};

	[[nodiscard]] QString key() const;
	void load();
	void save();
	void check();
	void schedule();
	void deleteNow(PeerId peer, MsgId msg);

	const not_null<Main::Session*> _session;
	std::vector<Range> _ranges;
	std::vector<Entry> _entries;
	base::Timer _timer;
	rpl::lifetime _lifetime;

};

[[nodiscard]] std::map<not_null<Main::Session*>, Manager*> &Managers() {
	static auto result = std::map<not_null<Main::Session*>, Manager*>();
	return result;
}

Manager::Manager(not_null<Main::Session*> session)
: _session(session)
, _timer([=] { check(); }) {
	load();
	_session->data().itemIdChanged(
	) | rpl::on_next([=](const Data::Session::IdChange &change) {
		const auto now = crl::now();
		auto changed = false;
		for (auto i = begin(_ranges); i != end(_ranges);) {
			if (now - i->added > kRangeLifetime) {
				i = _ranges.erase(i);
				continue;
			}
			if (change.oldId >= i->from && change.oldId < i->till) {
				_entries.push_back({
					.peer = change.newId.peer,
					.msg = change.newId.msg,
					.at = base::unixtime::now() + i->ttl,
				});
				changed = true;
			}
			++i;
		}
		if (changed) {
			save();
			schedule();
		}
	}, _lifetime);

	// Ilova yopiq paytida vaqti o'tib ketganlarni biroz kutib o'chiramiz.
	_timer.callOnce(5 * crl::time(1000));
}

QString Manager::key() const {
	return u"autodelete/u"_q + QString::number(_session->userId().bare);
}

void Manager::load() {
	const auto list = Storage().value(key()).toStringList();
	for (const auto &line : list) {
		const auto parts = line.split(':');
		if (parts.size() != 3) {
			continue;
		}
		const auto peer = DeserializePeerId(parts[0].toULongLong());
		const auto msg = MsgId(parts[1].toLongLong());
		const auto at = TimeId(parts[2].toLongLong());
		if (peer && msg && at) {
			_entries.push_back({ .peer = peer, .msg = msg, .at = at });
		}
	}
}

void Manager::save() {
	auto list = QStringList();
	for (const auto &entry : _entries) {
		list.push_back(QString::number(SerializePeerId(entry.peer))
			+ ':' + QString::number(entry.msg.bare)
			+ ':' + QString::number(entry.at));
	}
	if (list.isEmpty()) {
		Storage().remove(key());
	} else {
		Storage().setValue(key(), list);
	}
	Storage().sync();
}

void Manager::addRange(MsgId from, MsgId till, TimeId ttl) {
	if (ttl <= 0 || !(from < till)) {
		return;
	}
	_ranges.push_back({
		.from = from,
		.till = till,
		.ttl = ttl,
		.added = crl::now(),
	});
}

void Manager::deleteNow(PeerId peer, MsgId msg) {
	auto &owner = _session->data();
	const auto fullId = FullMsgId(peer, msg);
	if (owner.message(fullId)) {
		owner.histories().deleteMessages(MessageIdsList{ fullId }, true);
		owner.sendHistoryChangeNotifications();
	} else {
		owner.histories().deleteMessages(
			owner.history(peer),
			QVector<MTPint>(1, MTP_int(int(msg.bare))),
			true);
	}
}

void Manager::check() {
	const auto now = base::unixtime::now();
	auto due = std::vector<Entry>();
	for (auto i = begin(_entries); i != end(_entries);) {
		if (i->at <= now) {
			due.push_back(*i);
			i = _entries.erase(i);
		} else {
			++i;
		}
	}
	if (!due.empty()) {
		save();
		for (const auto &entry : due) {
			deleteNow(entry.peer, entry.msg);
		}
	}
	schedule();
}

void Manager::schedule() {
	if (_entries.empty()) {
		_timer.cancel();
		return;
	}
	auto nearest = _entries.front().at;
	for (const auto &entry : _entries) {
		nearest = std::min(nearest, entry.at);
	}
	const auto delay = std::clamp(
		nearest - base::unixtime::now(),
		TimeId(1),
		kMaxTimerDelay);
	_timer.callOnce(delay * crl::time(1000));
}

} // namespace

void StartAutoDelete(not_null<Main::Session*> session) {
	auto &managers = Managers();
	if (managers.contains(session)) {
		return;
	}
	const auto manager = session->lifetime().make_state<Manager>(session);
	managers.emplace(session, manager);
	session->lifetime().add([=] {
		Managers().erase(session);
	});
}

void RegisterAutoDelete(
		not_null<Main::Session*> session,
		MsgId localFrom,
		MsgId localTill,
		TimeId ttl) {
	StartAutoDelete(session);
	const auto i = Managers().find(session);
	if (i != end(Managers())) {
		i->second->addRange(localFrom, localTill, ttl);
	}
}

} // namespace MilliyGram
