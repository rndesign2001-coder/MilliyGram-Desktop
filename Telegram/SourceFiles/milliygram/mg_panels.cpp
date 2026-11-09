/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "milliygram/mg_panels.h"

#include "ui/painter.h"
#include "ui/rp_widget.h"

#include "styles/style_basic.h"
#include "styles/style_layers.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

namespace MilliyGram {
namespace {

class Corner final : public Ui::RpWidget {
public:
	explicit Corner(QWidget *parent) : RpWidget(parent) {
		setAttribute(Qt::WA_TransparentForMouseEvents);
	}

	// bit 0 — o'ng tomon, bit 1 — pastki tomon.
	void setPosition(int position) {
		if (_position != position) {
			_position = position;
			update();
		}
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		auto hq = PainterHighQualityEnabler(p);
		const auto r = float64(width());
		const auto center = QPointF(
			(_position & 1) ? 0. : r,
			(_position & 2) ? 0. : r);
		auto outer = QPainterPath();
		outer.addRect(QRectF(0., 0., r, r));
		auto inner = QPainterPath();
		inner.addEllipse(center, r, r);
		p.fillPath(outer.subtracted(inner), PanelGapColor());
	}

private:
	int _position = 0;

};

} // namespace

int PanelMargin() {
	return style::ConvertScale(10);
}

int PanelHalfGap() {
	return style::ConvertScale(5);
}

int PanelRadius() {
	return style::ConvertScale(16);
}

QColor PanelGapColor() {
	const auto bg = st::windowBg->c;
	return (bg.lightness() < 128) ? bg.darker(155) : bg.darker(112);
}

PanelCorners::PanelCorners(not_null<QWidget*> parent)
: _parent(parent) {
}

void PanelCorners::update(const std::vector<QRect> &panels) {
	const auto r = PanelRadius();
	_used = 0;
	for (const auto &rect : panels) {
		if (rect.width() < 2 * r || rect.height() < 2 * r) {
			continue;
		}
		for (auto position = 0; position != 4; ++position) {
			if (_used == int(_corners.size())) {
				_corners.push_back(Ui::CreateChild<Corner>(_parent.get()));
			}
			const auto corner = static_cast<Corner*>(_corners[_used++]);
			corner->setPosition(position);
			corner->setGeometry(
				(position & 1) ? (rect.x() + rect.width() - r) : rect.x(),
				(position & 2) ? (rect.y() + rect.height() - r) : rect.y(),
				r,
				r);
			corner->show();
			corner->raise();
		}
	}
	for (auto i = _used; i < int(_corners.size()); ++i) {
		_corners[i]->hide();
	}
}

void PanelCorners::raise() {
	for (auto i = 0; i < _used; ++i) {
		_corners[i]->raise();
	}
}

} // namespace MilliyGram
