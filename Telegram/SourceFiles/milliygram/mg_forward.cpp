/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "milliygram/mg_forward.h"

#include "milliygram/mg_settings.h"

#include "api/api_common.h"
#include "api/api_sending.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "boxes/abstract_box.h"
#include "boxes/share_box.h"
#include "chat_helpers/compose/compose_show.h"
#include "data/data_chat_participant_status.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "main/main_session.h"
#include "ui/boxes/choose_date_time.h"
#include "ui/toast/toast.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"

#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_layers.h"

#include <QtCore/QDateTime>
#include <QtCore/QRegularExpression>

namespace MilliyGram {
namespace {

struct State {
	TextWithTags original;
	TextWithTags text;
	FullMsgId editable;
	bool hasMedia = false;
	bool withMedia = true;
	bool spoiler = false;
	bool silent = false;
	TimeId scheduled = 0;
};

[[nodiscard]] QString Uz(const char *text) {
	return QString::fromUtf8(text);
}

[[nodiscard]] TextWithTags ToTags(const TextWithEntities &text) {
	return TextWithTags{
		text.text,
		TextUtilities::ConvertEntitiesToTextTags(text.entities)
	};
}

// Havolalarni (va ixtiyoriy ravishda @username larni) olib tashlaydi.
[[nodiscard]] QString RemoveLinks(QString text, bool mentionsToo) {
	static const auto links = QRegularExpression(
		Uz("(?:https?://|www\\.|t\\.me/|telegram\\.me/)\\S+"),
		QRegularExpression::CaseInsensitiveOption);
	static const auto mentions = QRegularExpression(
		Uz("@[A-Za-z0-9_]{3,}"));
	static const auto spaces = QRegularExpression(Uz("[ \\t]{2,}"));
	static const auto lines = QRegularExpression(Uz("\\n{3,}"));

	text.remove(links);
	if (mentionsToo) {
		text.remove(mentions);
	}
	text.replace(spaces, Uz(" "));
	text.replace(lines, Uz("\n\n"));
	return text.trimmed();
}

// Izoh oxiriga qo'shiladigan imzoni so'rash oynasi.
void ShowSignatureBox(Fn<void(QString)> done) {
	Ui::show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(Uz("Imzoni o'rnatish")));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::NoNewlines,
			rpl::single(Uz("Masalan: @kanalim")),
			TextWithTags{ ForwardSignature() }));
		const auto save = [=] {
			const auto text = field->getLastText().trimmed();
			SetForwardSignature(text);
			box->closeBox();
			if (done) {
				done(text);
			}
		};
		field->submits(
		) | rpl::on_next(save, field->lifetime());
		box->setFocusCallback([=] {
			field->setFocusFast();
		});
		box->addButton(rpl::single(Uz("Saqlash")), save);
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

[[nodiscard]] QString ScheduleText(TimeId time) {
	return time
		? Uz("Yuborish vaqti: ")
			+ QDateTime::fromSecsSinceEpoch(time).toString(
				Uz("dd.MM.yyyy HH:mm"))
		: Uz("Yuborish vaqti: hozir");
}

void SendCustom(
		not_null<Main::Session*> session,
		const MessageIdsList &ids,
		const std::shared_ptr<State> &state,
		std::vector<not_null<Data::Thread*>> &&threads,
		TextWithTags &&comment,
		Api::SendOptions options) {
	const auto owner = &session->data();
	const auto items = owner->idsToItems(ids);
	if (items.empty() || threads.empty()) {
		return;
	}
	options.silent = options.silent || state->silent;
	options.mediaSpoiler = state->spoiler;
	if (!options.scheduled && state->scheduled) {
		options.scheduled = state->scheduled;
	}
	auto &api = session->api();
	for (const auto &thread : threads) {
		if (!comment.text.isEmpty()) {
			auto message = Api::MessageToSend(
				Api::SendAction(thread, options));
			message.textWithTags = comment;
			message.action.clearDraft = false;
			api.sendMessage(std::move(message));
		}
		for (const auto &item : items) {
			const auto media = item->media();
			const auto plain = !media || media->webpage();
			const auto photo = plain ? nullptr : media->photo();
			const auto document = plain ? nullptr : media->document();
			const auto text = (item->fullId() == state->editable)
				? state->text
				: ToTags(item->originalText());
			if (photo || document) {
				if (state->withMedia) {
					auto message = Api::MessageToSend(
						Api::SendAction(thread, options));
					message.textWithTags = text;
					message.action.clearDraft = false;
					if (photo) {
						Api::SendExistingPhoto(
							std::move(message),
							not_null<PhotoData*>(photo));
					} else {
						Api::SendExistingDocument(
							std::move(message),
							not_null<DocumentData*>(document));
					}
					continue;
				}
			} else if (!plain) {
				// So'rovnoma, kontakt va h.k.: yuboruvchisiz oddiy uzatiladi.
				if (state->withMedia) {
					auto action = Api::SendAction(thread, options);
					action.clearDraft = false;
					api.forwardMessages(
						Data::ResolvedForwardDraft{
							.items = { item },
							.options = Data::ForwardOptions::NoSenderNames,
						},
						std::move(action));
					continue;
				}
			}
			if (!text.text.trimmed().isEmpty()) {
				auto message = Api::MessageToSend(
					Api::SendAction(thread, options));
				message.textWithTags = text;
				message.action.clearDraft = false;
				api.sendMessage(std::move(message));
			}
		}
	}
}

void ShowChooseChat(
		std::shared_ptr<ChatHelpers::Show> show,
		MessageIdsList ids,
		std::shared_ptr<State> state) {
	const auto session = &show->session();
	const auto owner = &session->data();
	const auto items = owner->idsToItems(ids);
	if (items.empty()) {
		return;
	}
	const auto history = items.front()->history();
	show->show(Box<ShareBox>(ShareBox::Descriptor{
		.session = session,
		.countMessagesCallback = ShareBox::DefaultForwardCountMessages(
			history,
			ids),
		.submitCallback = [=](
				std::vector<not_null<Data::Thread*>> &&threads,
				Fn<bool()> checkPaid,
				TextWithTags comment,
				Api::SendOptions options,
				Data::ForwardOptions) {
			if (threads.empty()) {
				return;
			} else if (!checkPaid()) {
				return;
			}
			SendCustom(
				session,
				ids,
				state,
				std::move(threads),
				std::move(comment),
				options);
			show->hideLayer();
			show->showToast({
				.text = { Uz("Maxsus uzatildi") },
			});
		},
		.filterCallback = [=](not_null<Data::Thread*> thread) {
			return Data::CanSendTexts(thread);
		},
		.titleOverride = rpl::single(Uz("Maxsus uzatish: chatni tanlang")),
		.moneyRestrictionError = ShareMessageMoneyRestrictionError(),
	}), Ui::LayerOption::CloseOther);
}

} // namespace

void ShowCustomForward(
		std::shared_ptr<ChatHelpers::Show> show,
		MessageIdsList ids) {
	const auto session = &show->session();
	const auto owner = &session->data();
	const auto items = owner->idsToItems(ids);
	if (items.empty()) {
		return;
	}

	const auto state = std::make_shared<State>();
	// Tahrirlanadigan xabar: matni bor birinchi xabar, bo'lmasa media xabar.
	auto editable = items.front();
	for (const auto &item : items) {
		const auto media = item->media();
		if (media && !media->webpage()
			&& (media->photo() || media->document())) {
			state->hasMedia = true;
		}
	}
	for (const auto &item : items) {
		if (!item->originalText().text.isEmpty()) {
			editable = item;
			break;
		}
	}
	state->editable = editable->fullId();
	state->original = ToTags(editable->originalText());
	state->text = state->original;

	Ui::show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(Uz("Maxsus uzatish")));
		box->setWidth(st::boxWideWidth);

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz("1. Izohni tahrirlang")),
			st::boxLabel));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::factcheckField,
			Ui::InputField::Mode::MultiLine,
			rpl::single(Uz("Izoh / matn")),
			state->original));
		field->setMaxHeight(st::boxWideWidth / 2);

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz("2. Tezkor amallar")),
			st::boxLabel));
		const auto actions = box->addRow(object_ptr<Ui::RpWidget>(box));
		auto buttons = std::make_shared<std::vector<Ui::LinkButton*>>();
		const auto addAction = [=](const QString &text, Fn<void()> callback) {
			const auto button = Ui::CreateChild<Ui::LinkButton>(
				actions,
				text);
			button->setClickedCallback(std::move(callback));
			buttons->push_back(button);
		};
		addAction(Uz("Asl matn"), [=] {
			field->setTextWithTags(state->original);
		});
		addAction(Uz("Havolalarni o'chirish"), [=] {
			field->setText(RemoveLinks(field->getLastText(), false));
		});
		addAction(Uz("@ va linklarni tozalash"), [=] {
			field->setText(RemoveLinks(field->getLastText(), true));
		});
		addAction(Uz("Formatsiz"), [=] {
			field->setText(field->getLastText());
		});
		const auto appendSignature = [=](const QString &signature) {
			if (signature.isEmpty()) {
				return;
			}
			auto now = field->getTextWithAppliedMarkdown();
			if (!now.text.isEmpty()) {
				now.text += Uz("\n\n");
			}
			now.text += signature;
			field->setTextWithTags(now);
		};
		addAction(Uz("Imzo qo'shish"), [=] {
			const auto signature = ForwardSignature();
			if (signature.isEmpty()) {
				ShowSignatureBox(appendSignature);
			} else {
				appendSignature(signature);
			}
		});
		addAction(Uz("Imzoni o'rnatish"), [=] {
			ShowSignatureBox(nullptr);
		});
		actions->widthValue(
		) | rpl::on_next([=](int width) {
			const auto hskip = st::normalFont->spacew * 4;
			const auto vskip = st::normalFont->height / 2;
			auto x = 0;
			auto y = 0;
			auto rowHeight = 0;
			for (const auto button : *buttons) {
				const auto w = button->width();
				const auto h = button->height();
				if (x > 0 && x + w > width) {
					x = 0;
					y += rowHeight + vskip;
					rowHeight = 0;
				}
				button->moveToLeft(x, y);
				x += w + hskip;
				rowHeight = std::max(rowHeight, h);
			}
			actions->resize(width, y + rowHeight);
		}, actions->lifetime());

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz("3. Yuborish")),
			st::boxLabel));
		const auto withMedia = box->addRow(object_ptr<Ui::Checkbox>(
			box,
			rpl::single(Uz("Mediani ham yuborish")),
			true,
			st::defaultBoxCheckbox));
		withMedia->setDisabled(!state->hasMedia);
		const auto spoiler = box->addRow(object_ptr<Ui::Checkbox>(
			box,
			rpl::single(Uz("Spoiler bilan yashirish")),
			false,
			st::defaultBoxCheckbox));
		const auto silent = box->addRow(object_ptr<Ui::Checkbox>(
			box,
			rpl::single(Uz("Ovozsiz yuborish")),
			false,
			st::defaultBoxCheckbox));
		const auto schedule = box->addRow(object_ptr<Ui::LinkButton>(
			box,
			ScheduleText(0)));
		schedule->setClickedCallback([=] {
			Ui::show(Box([=](not_null<Ui::GenericBox*> inner) {
				Ui::ChooseDateTimeBox(inner, {
					.title = rpl::single(Uz("Yuborish vaqti")),
					.submit = rpl::single(Uz("Belgilash")),
					.done = [=](TimeId time) {
						state->scheduled = time;
						schedule->setText(ScheduleText(time));
						inner->closeBox();
					},
					.min = [] { return base::unixtime::now() + 60; },
					.time = (state->scheduled
						? state->scheduled
						: base::unixtime::now() + 3600),
					.max = [] {
						return base::unixtime::now() + 365 * 24 * 3600;
					},
				});
				inner->addLeftButton(rpl::single(Uz("Hozir yuborish")), [=] {
					state->scheduled = 0;
					schedule->setText(ScheduleText(0));
					inner->closeBox();
				});
			}));
		});

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz(
				"Xabar sizning nomingizdan, \"Forwarded from\" belgisisiz "
				"yuboriladi. Pastdagi \"Chatni tanlash\" tugmasini bosing.")),
			st::boxDividerLabel));

		box->setFocusCallback([=] {
			field->setFocusFast();
		});
		box->addButton(rpl::single(Uz("Chatni tanlash")), [=] {
			state->text = field->getTextWithAppliedMarkdown();
			state->withMedia = withMedia->checked();
			state->spoiler = spoiler->checked();
			state->silent = silent->checked();
			box->closeBox();
			ShowChooseChat(show, ids, state);
		});
		box->addButton(rpl::single(Uz("Bekor qilish")), [=] {
			box->closeBox();
		});
	}));
}

} // namespace MilliyGram
