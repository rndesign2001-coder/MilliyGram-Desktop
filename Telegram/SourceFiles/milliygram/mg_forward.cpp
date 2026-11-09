/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "milliygram/mg_forward.h"

#include "milliygram/mg_settings.h"
#include "milliygram/mg_autodelete.h"

#include "api/api_common.h"
#include "api/api_sending.h"
#include "apiwrap.h"
#include "base/unixtime.h"
#include "boxes/abstract_box.h"
#include "boxes/share_box.h"
#include "chat_helpers/compose/compose_show.h"
#include "chat_helpers/message_field.h"
#include "data/data_chat_participant_status.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "lang/lang_keys.h"
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
#include <array>
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
	TimeId autoDelete = 0;
};

// Avto-o'chirish variantlari (soniya), 0 — o'chirilmaydi.
constexpr auto kAutoDeleteOptions = std::array<TimeId, 8>{ {
	0,
	15 * 60,
	60 * 60,
	6 * 60 * 60,
	12 * 60 * 60,
	24 * 60 * 60,
	2 * 24 * 60 * 60,
	7 * 24 * 60 * 60,
} };

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

[[nodiscard]] QString AutoDeleteText(TimeId ttl) {
	const auto value = [&]() -> QString {
		if (!ttl) {
			return Uz("yo'q");
		} else if (ttl < 60 * 60) {
			return QString::number(ttl / 60) + Uz(" daqiqa");
		} else if (ttl < 24 * 60 * 60) {
			return QString::number(ttl / 3600) + Uz(" soat");
		}
		return QString::number(ttl / 86400) + Uz(" kun");
	}();
	return Uz("Avto-o'chirish: ") + value;
}

// Havola tugmalarini qatorlarga sig'dirib joylaydi.
void AddFlowLinks(
		not_null<Ui::GenericBox*> box,
		std::vector<std::pair<QString, Fn<void()>>> actions) {
	const auto container = box->addRow(object_ptr<Ui::RpWidget>(box));
	auto buttons = std::make_shared<std::vector<Ui::LinkButton*>>();
	for (auto &[text, callback] : actions) {
		const auto button = Ui::CreateChild<Ui::LinkButton>(
			container,
			text);
		button->setClickedCallback(std::move(callback));
		buttons->push_back(button);
	}
	container->widthValue(
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
		container->resize(width, y + rowHeight);
	}, container->lifetime());
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
	const auto localFrom = owner->nextLocalMessageId();
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
	const auto localTill = owner->nextLocalMessageId();
	if (state->autoDelete > 0 && !options.scheduled) {
		RegisterAutoDelete(session, localFrom, localTill, state->autoDelete);
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
			show->showToast(Ui::Toast::Config{
				.text = TextWithEntities{ Uz("Maxsus uzatildi") },
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

		// Matn formati: belgilangan qismga qo'llanadi (Ctrl+B/I/U, Ctrl+K — havola).
		InitMessageFieldHandlers({
			.session = session,
			.show = show,
			.field = field,
			.customEmojiPaused = [=] {
				return show->paused(ChatHelpers::PauseReason::Layer);
			},
		});
		const auto format = [=](const QString &tag) {
			return [=] {
				field->toggleCurrentMarkdownTag(tag);
				field->setFocusFast();
			};
		};
		using Field = Ui::InputField;
		AddFlowLinks(box, {
			{ Uz("Qalin"), format(Field::kTagBold) },
			{ Uz("Qiya"), format(Field::kTagItalic) },
			{ Uz("Tagi chizilgan"), format(Field::kTagUnderline) },
			{ Uz("Ustidan chizilgan"), format(Field::kTagStrikeOut) },
			{ Uz("Kod"), format(Field::kTagCode) },
			{ Uz("Spoiler"), format(Field::kTagSpoiler) },
			{ Uz("Iqtibos"), format(Field::kTagBlockquote) },
			{ Uz("Formatni tozalash"), [=] {
				field->clearCurrentMarkdown();
				field->setFocusFast();
			} },
		});
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz(
				"Matnni belgilab formatni tanlang. Havola: matnni belgilab Ctrl+K.")),
			st::boxDividerLabel));

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz("2. Tezkor amallar")),
			st::boxLabel));
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
		AddFlowLinks(box, {
			{ Uz("Asl matn"), [=] {
				field->setTextWithTags(state->original);
			} },
			{ Uz("Havolalarni o'chirish"), [=] {
				field->setText(RemoveLinks(field->getLastText(), false));
			} },
			{ Uz("@ va linklarni tozalash"), [=] {
				field->setText(RemoveLinks(field->getLastText(), true));
			} },
			{ Uz("Imzo qo'shish"), [=] {
				const auto signature = ForwardSignature();
				if (signature.isEmpty()) {
					ShowSignatureBox(appendSignature);
				} else {
					appendSignature(signature);
				}
			} },
			{ Uz("Imzoni o'rnatish"), [=] {
				ShowSignatureBox(nullptr);
			} },
		});

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

		const auto autoDelete = box->addRow(object_ptr<Ui::LinkButton>(
			box,
			AutoDeleteText(0)));
		autoDelete->setClickedCallback([=] {
			const auto i = ranges::find(kAutoDeleteOptions, state->autoDelete);
			const auto next = (i == end(kAutoDeleteOptions)
				|| (i + 1) == end(kAutoDeleteOptions))
				? kAutoDeleteOptions.front()
				: *(i + 1);
			state->autoDelete = next;
			autoDelete->setText(AutoDeleteText(next));
		});
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz(
				"Avto-o'chirish: yuborilgan post belgilangan vaqtdan keyin hamma "
				"uchun o'chiriladi (MilliyGram ochiq bo'lishi kerak, yopiq bo'lsa "
				"keyingi ochilishda). Vaqti belgilangan postlarga qo'llanmaydi.")),
			st::boxDividerLabel));

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
