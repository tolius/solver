#ifndef SOLUTION_BOOK_H
#define SOLUTION_BOOK_H

#include "polyglotbook.h"
#include "board/move.h"

#include <QString>
#include <memory>
#include <list>


namespace Chess
{
	class Board;
}


struct LIB_EXPORT SolutionEntry : public OpeningBook::Entry
{
	SolutionEntry();
	SolutionEntry(const OpeningBook::Entry& entry);
	SolutionEntry(const SolutionEntry& entry);
	SolutionEntry(quint16 pgMove, quint16 weight, quint32 learn);
	SolutionEntry(uint64_t bytes);

	bool isNull() const;
	qint16 score(qint16 add_plies = 0) const;
	bool is_overridden() const;
	quint32 depth() const;
	quint32 version() const;
	quint32 time() const;
	quint32 nodes() const;
	Chess::Move move(const Chess::Board* board) const;
	Chess::Move move(std::shared_ptr<Chess::Board> board) const;
	using OpeningBook::Entry::move;
	QString san(std::shared_ptr<Chess::Board> board) const;
	QString san(Chess::Board* board) const;

	QString info(Chess::Board* board, bool is_book = true) const;
	QString getScore(bool is_book, qint16 add_plies = 0) const;
	QString getNodes() const;

	static QString score2Text(qint16 score);
	static bool compare(const SolutionEntry& a, const SolutionEntry& b);
};

class LIB_EXPORT SolutionBook : public PolyglotBook
{
public:
	SolutionBook(AccessMode mode = Ram);

	std::list<SolutionEntry> bookEntries(quint64 key) const;

protected:
	//SolutionEntry getEntry(QDataStream& in, quint64* key) const;
};

#endif // SOLUTION_BOOK_H
