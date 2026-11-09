/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "milliygram/mg_themes.h"

#include "milliygram/mg_settings.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "storage/localstorage.h"
#include "ui/abstract_button.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "window/themes/window_theme.h"
#include "window/themes/window_theme_editor_box.h"
#include "window/themes/window_themes_embedded.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "lang/lang_keys.h"

#include "styles/style_basic.h"
#include "styles/style_layers.h"

#include <QtGui/QPainter>

#include <array>

namespace MilliyGram {
namespace {

using Type = Window::Theme::EmbeddedType;

struct Preset {
	Type type;
	const char *accent;
	const char *name;
	const char *kind;
	const char *path = nullptr; // alohida mavzu fayli (rang moslashtirilmaydi)
};

constexpr auto kClassicNightPath = ":/gui/night-classic.tdesktop-theme";

constexpr auto kPresets = std::array<Preset, 8>{ {
	{ Type::Night, "8b6dff", "Tun: binafsha", "Tungi mavzu" },
	{ Type::Night, "5288c1", "Telegram klassik (tungi)", "Asl Telegram Desktop mavzusi", kClassicNightPath },
	{ Type::Night, "16a3a3", "Tun: Samarqand firuzasi", "Tungi mavzu" },
	{ Type::Night, "d6458f", "Tun: atlas pushtisi", "Tungi mavzu" },
	{ Type::Night, "1ea5d6", "Tun: bayroq ko'ki", "Tungi mavzu" },
	{ Type::Night, "e0a526", "Tun: oltin", "Tungi mavzu" },
	{ Type::DayBlue, "45bce7", "Kunduz: moviy", "Kunduzgi mavzu" },
	{ Type::DayBlue, "52b440", "Kunduz: yashil", "Kunduzgi mavzu" },
} };

[[nodiscard]] QString Uz(const char *text) {
	return QString::fromUtf8(text);
}

[[nodiscard]] QString SchemePath(Type type) {
	const auto schemes = Window::Theme::EmbeddedThemes();
	const auto i = ranges::find(
		schemes,
		type,
		&Window::Theme::EmbeddedScheme::type);
	return (i != end(schemes)) ? i->path : QString();
}

[[nodiscard]] QColor SchemeAccent(Type type) {
	const auto schemes = Window::Theme::EmbeddedThemes();
	const auto i = ranges::find(
		schemes,
		type,
		&Window::Theme::EmbeddedScheme::type);
	return (i != end(schemes)) ? i->accentColor : QColor();
}

[[nodiscard]] QString PresetPath(const Preset &preset) {
	return preset.path ? Uz(preset.path) : SchemePath(preset.type);
}

[[nodiscard]] bool IsActive(const Preset &preset) {
	const auto &object = Window::Theme::Background()->themeObject();
	if (object.cloud.id || object.pathAbsolute != PresetPath(preset)) {
		return false;
	} else if (preset.path) {
		return true;
	}
	const auto &colors = Core::App().settings().themesAccentColors();
	const auto current = colors.get(preset.type).value_or(
		SchemeAccent(preset.type));
	return current == QColor(Uz("#") + Uz(preset.accent));
}

void ApplyPreset(
		not_null<Window::SessionController*> controller,
		const Preset &preset) {
	using namespace Window::Theme;

	if (Background()->editingTheme()) {
		controller->show(Ui::MakeInformBox(
			tr::lng_theme_editor_cant_change_theme()));
		return;
	}
	if (!preset.path) {
		auto &settings = Core::App().settings();
		settings.setSystemAccentColorEnabled(false);
		settings.themesAccentColors().set(
			preset.type,
			QColor(Uz("#") + Uz(preset.accent)));
		Local::writeSettings();
	}

	const auto path = PresetPath(preset);
	const auto night = (preset.type != Type::DayBlue)
		&& (preset.type != Type::Default);
	if (IsNightMode() == night) {
		ApplyDefaultWithPath(path);
		KeepApplied();
	} else {
		ToggleNightModeWithConfirmation(
			&controller->window(),
			[=] {
				ToggleNightMode(path);
				KeepApplied();
			});
	}
}

} // namespace

void ShowThemesBox(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(Uz("Mavzular")));
		box->setWidth(st::boxWideWidth);

		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Uz(
				"Tayyor mavzuni tanlang. Yangi mavzuni o'zingiz "
				"yaratish uchun pastdagi tugmani bosing.")),
			st::boxLabel));

		const auto floating = box->addRow(object_ptr<Ui::Checkbox>(
			box,
			rpl::single(Uz("Suzuvchi yumaloq panellar")),
			FloatingPanels(),
			st::defaultBoxCheckbox));
		floating->checkedValue(
		) | rpl::skip(1) | rpl::on_next([](bool checked) {
			SetFloatingPanels(checked);
		}, floating->lifetime());
		const auto horizontal = box->addRow(object_ptr<Ui::Checkbox>(
			box,
			rpl::single(Uz("Jildlar tepada (tab ko'rinishida)")),
			Core::App().settings().chatFiltersHorizontal(),
			st::defaultBoxCheckbox));
		horizontal->checkedValue(
		) | rpl::skip(1) | rpl::on_next([](bool checked) {
			Core::App().settings().setChatFiltersHorizontal(checked);
			Core::App().saveSettingsDelayed();
		}, horizontal->lifetime());

		const auto rowHeight = st::normalFont->height * 3;
		for (const auto &preset : kPresets) {
			const auto button = box->addRow(
				object_ptr<Ui::AbstractButton>(box),
				QMargins());
			button->resize(button->width(), rowHeight);
			button->setClickedCallback([=] {
				ApplyPreset(controller, preset);
			});
			button->paintRequest(
			) | rpl::on_next([=](QRect) {
				auto p = QPainter(button);
				auto hq = PainterHighQualityEnabler(p);
				const auto active = IsActive(preset);
				const auto accent = QColor(Uz("#") + Uz(preset.accent));
				const auto skip = st::normalFont->height / 2;
				if (active || button->isOver()) {
					p.setPen(Qt::NoPen);
					p.setBrush(st::windowBgOver);
					p.drawRoundedRect(
						button->rect().marginsRemoved(
							{ skip, 2, skip, 2 }),
						skip,
						skip);
				}
				const auto size = rowHeight - 2 * skip;
				p.setPen(Qt::NoPen);
				p.setBrush(accent);
				p.drawEllipse(QRect(2 * skip, skip, size, size));

				const auto left = 2 * skip + size + skip;
				p.setPen(st::windowFg);
				p.setFont(st::semiboldFont);
				p.drawText(
					QRect(left, skip, button->width() - left, size / 2),
					Qt::AlignLeft | Qt::AlignBottom,
					Uz(preset.name));
				p.setPen(st::windowSubTextFg);
				p.setFont(st::normalFont);
				p.drawText(
					QRect(
						left,
						skip + size / 2,
						button->width() - left,
						size / 2),
					Qt::AlignLeft | Qt::AlignTop,
					Uz(preset.kind));
				if (active) {
					p.setPen(accent);
					p.setFont(st::semiboldFont);
					p.drawText(
						QRect(0, 0, button->width() - 3 * skip, button->height()),
						Qt::AlignRight | Qt::AlignVCenter,
						Uz("\xE2\x9C\x93"));
				}
			}, button->lifetime());
		}

		const auto window = &controller->window();
		box->addButton(rpl::single(Uz("Mavzu yaratish")), [=] {
			box->closeBox();
			window->show(Box(Window::Theme::CreateBox, window));
		});
		box->addButton(rpl::single(Uz("Yopish")), [=] {
			box->closeBox();
		});
	}));
}

} // namespace MilliyGram
