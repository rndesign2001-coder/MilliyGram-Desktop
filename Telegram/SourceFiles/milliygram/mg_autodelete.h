/*
This file is part of MilliyGram Desktop,
an unofficial Telegram client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace MilliyGram {

// Yuborilgan xabarlarni belgilangan vaqtdan keyin hamma uchun o'chirish.
// Telegram buni server tomonida qo'llamaydi, shuning uchun MilliyGram
// o'chirish navbatini saqlab boradi va vaqti kelganda o'zi o'chiradi
// (ilova o'sha paytda yopiq bo'lsa — keyingi ochilishda o'chiriladi).
void StartAutoDelete(not_null<Main::Session*> session);

// [localFrom, localTill) oralig'idagi lokal xabarlar serverdan haqiqiy ID
// olgach, ttl soniyadan keyin o'chiriladi.
void RegisterAutoDelete(
	not_null<Main::Session*> session,
	MsgId localFrom,
	MsgId localTill,
	TimeId ttl);

} // namespace MilliyGram
