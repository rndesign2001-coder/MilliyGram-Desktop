/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_types.h"

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace MilliyGram {

// Maxsus uzatish: yuboruvchi nomisiz, izohni tahrirlash, tezkor amallar,
// ovozsiz / spoiler / rejalashtirilgan yuborish, so'ng chatni tanlash.
void ShowCustomForward(
	std::shared_ptr<ChatHelpers::Show> show,
	MessageIdsList ids);

} // namespace MilliyGram
