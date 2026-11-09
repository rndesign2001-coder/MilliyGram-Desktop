/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Ui {
class RpWidget;
} // namespace Ui

namespace MilliyGram {

// Suzuvchi panellar: tashqi chet va panellar orasidagi bo'shliq.
[[nodiscard]] int PanelMargin();
[[nodiscard]] int PanelHalfGap();
[[nodiscard]] int PanelRadius();
// Panellar orqasidagi (bo'shliq) rang — joriy mavzudan hisoblanadi.
[[nodiscard]] QColor PanelGapColor();

// Panel burchaklarini yumaloq ko'rsatadigan kichik qatlamlar:
// har bir burchakka bo'shliq rangida chorak doira tashqarisi chiziladi.
class PanelCorners final {
public:
	explicit PanelCorners(not_null<QWidget*> parent);

	void update(const std::vector<QRect> &panels);
	void raise();

private:
	const not_null<QWidget*> _parent;
	std::vector<Ui::RpWidget*> _corners;
	int _used = 0;

};

} // namespace MilliyGram
