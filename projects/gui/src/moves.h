#ifndef MOVES_H
#define MOVES_H

#include <QWidget>
#include <QPointer>
#include <set>
#include "board/move.h"

class ChessGame;
namespace Chess { class GenericMove; }
class FlowLayout;

namespace Ui {
	class MovesWidget;
}

class Moves : public QWidget
{
Q_OBJECT

public:
	Moves(QWidget* parent = nullptr);

	void setGame(ChessGame* game);
	void setTint(QColor tint = QColor());

public slots:
	void positionChanged();
	void goodMovesReported(const std::set<QString>& good_moves);
	void badMoveReported(const QString& bad_move);

signals:
	void gotoFirstMoveClicked();
	void gotoPreviousMoveClicked(int num);
	void gotoNextMoveClicked();
	void gotoCurrentMoveClicked();

private:
	void regenerateMoveButtons();
	void updateButtons();

private:
	Ui::MovesWidget* ui;
	QPointer<ChessGame> m_game;
	quint64 curr_key;
	FlowLayout* m_flowLayout;
};

#endif // MOVES_H
