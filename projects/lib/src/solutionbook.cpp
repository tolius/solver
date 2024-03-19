#include "solutionbook.h"
#include "positioninfo.h"
#include "board/board.h"

//#include <QDataStream>
#include <sstream>


SolutionEntry::SolutionEntry()
	: OpeningBook::Entry { 0, reinterpret_cast<const quint16&>(UNKNOWN_SCORE), 0 }
{}
SolutionEntry::SolutionEntry(const OpeningBook::Entry& entry)
	: OpeningBook::Entry { entry.pgMove, entry.weight, entry.learn }
{}

SolutionEntry::SolutionEntry(const SolutionEntry& entry)
	: OpeningBook::Entry { entry.pgMove, entry.weight, entry.learn }
{}

SolutionEntry::SolutionEntry(quint16 pgMove, quint16 weight, quint32 learn)
	: OpeningBook::Entry { pgMove, weight, learn }
{}

SolutionEntry::SolutionEntry(uint64_t bytes)
{
	pgMove = bytes >> 48;
	weight = static_cast<quint16>((bytes >> 32) & 0xFFFF);
	learn = static_cast<quint32>(bytes & 0x00000000FFFFFFFF);
}

bool SolutionEntry::isNull() const
{
	return pgMove == 0;
}

qint16 SolutionEntry::score(qint16 add_plies) const
{
	qint16 w = reinterpret_cast<const qint16&>(weight);
	if (add_plies > 0 && w != UNKNOWN_SCORE)
	{
		if (w > MATE_THRESHOLD)
			w = std::min(MATE_VALUE, qint16(w + (add_plies + 1) / 2));
		else if (w < -MATE_THRESHOLD)
			w = std::max(-MATE_VALUE, w - (add_plies + 1) / 2);
	}
	return w;
	//qint16 w = weight <= WEIGHT_THRESHOLD ? weight : WEIGHT_THRESHOLD - weight;
	//return w;
}

bool SolutionEntry::is_overridden() const
{
	return (learn & OVERRIDE_MASK) == OVERRIDE_MASK;
}

quint32 SolutionEntry::depth() const
{
	return learn & quint32(0xFF);
}

quint32 SolutionEntry::version() const
{
	return (learn & 0x0000FF00) >> 8;
}

quint32 SolutionEntry::time() const
{
	return learn >> 16;
}

quint32 SolutionEntry::nodes() const
{
	return learn;
}

Chess::Move SolutionEntry::move(const Chess::Board* board) const
{
	return board->moveFromGenericMove(move());
}

Chess::Move SolutionEntry::move(std::shared_ptr<Chess::Board> board) const
{
	return board->moveFromGenericMove(move());
}

QString SolutionEntry::san(std::shared_ptr<Chess::Board> board) const
{
	return san(board.get());
}

QString SolutionEntry::san(Chess::Board* board) const
{
	if (!board)
		return "";
	return board->moveString(move(board), Chess::Board::StandardAlgebraic);
}

QString SolutionEntry::info(Chess::Board* board, bool is_book) const
{
	QString str_move;
	if (board)
	{
		auto move = board->moveFromGenericMove(this->move());
		if (!move.isNull())
			str_move = QString("%1: ").arg(board->moveString(move, Chess::Board::StandardAlgebraic));
	}
	return str_move + getScore(is_book) + " " + getNodes();
}

QString SolutionEntry::score2Text(qint16 score)
{
	qint16 abs_score = abs(score);
	if (abs_score > WIN_THRESHOLD)
		return (score < 0 ? "#-" : "#") + QString::number(MATE_VALUE - abs_score);
	if (abs_score >= MATE_VALUE - MANUAL_VALUE - 1 && abs_score <= WIN_THRESHOLD)  // -1 for nextEntries()
		return score < 0 ? "**" : "*";
	if (abs_score < 15200)
		return (score > 0 ? "+" : "") + QString::number(float(score) / 100.f, 'f', 1);
	if (abs_score == 15265 || abs_score == 15266)
		return (score < 0) ? " -EG" : " EG";
	if (15200 <= abs_score && abs_score < 15300)
		return (score > 0) ? QString(" E.%1").arg(score - 15200, 2, 10, (QChar)'0') : QString(" -E.%1").arg(-score - 15200, 2, 10, (QChar)'0');
	return (score > 0) ? QString("E%1").arg(score / 100.f - 152, 5, 'f', 2) : QString("-E%1").arg(-score / 100.f - 152, 4, 'f', 2);
}

QString SolutionEntry::getScore(bool is_book, qint16 add_plies) const
{
	qint16 s = score(add_plies);
	QString str_score = s == UNKNOWN_SCORE ? "?"
	    : !is_book                         ? score2Text(s)
	    : s == 0                           ? "Draw"
	    : s == FAKE_DRAW_SCORE             ? "Draw?"
	    : s > FAKE_MATE_VALUE              ? QString("#%1").arg(MATE_VALUE - s)
	    : s > MATE_THRESHOLD               ? QString("#%1?").arg(FAKE_MATE_VALUE - s)
	    : s < -FAKE_MATE_VALUE             ? QString("#-%1").arg(MATE_VALUE + s)
	    : s < -MATE_THRESHOLD              ? QString("#-%1").arg(FAKE_MATE_VALUE + s)
	                                       : score2Text(s); // QString("%1").arg(float(s) / 100, 0, 'f', 2);
	return str_score;
}

QString SolutionEntry::getNodes() const
{
	auto num_nodes = nodes();
	if (num_nodes == 0 || pgMove == 0)
		return "N=???";
	std::wstringstream ss;
	ss << "N=";
	ss.imbue(std::locale(""));
	ss << num_nodes;
	return QString::fromStdWString(ss.str());
}


SolutionBook::SolutionBook(AccessMode mode)
	: PolyglotBook(mode)
{
}

/*SolutionEntry SolutionBook::getEntry(QDataStream& in, quint64* key) const
{
	auto entry = PolyglotBook::readEntry(in, key);
	return SolutionEntry(entry);
}*/


std::list<SolutionEntry> SolutionBook::bookEntries(quint64 key) const
{
	std::list<SolutionEntry> book_entries;
	auto entries = OpeningBook::entries(key);
	for (auto& entry : entries)
	{
		book_entries.push_back(SolutionEntry(entry)); // TODO: move iterator
	}
	return book_entries;
}
