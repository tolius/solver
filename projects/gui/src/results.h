#ifndef RESULTS_H
#define RESULTS_H

#include "positioninfo.h"
#include "movenumbertoken.h"
#include "movetoken.h"
#include "movecommenttoken.h"
#include "board/move.h"
#include "board/board.h"

#include <QWidget>
#include <QPointer>
#include <QUrl>
#include <QPair>
#include <QList>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>


namespace Chess
{ 
	class GenericMove; 
}
class PgnGame;
class ChessGame;
class QTimer;
class FlowLayout;
class Solution;
class Solver;
class QHBoxLayout;

namespace Ui {
	class ResultsWidget;
}

class Results : public QWidget
{
Q_OBJECT

public:
	Results(QWidget* parent = nullptr);

	void setSolution(std::shared_ptr<Solution> solution);
	void setSolver(std::shared_ptr<Solver> solver);
	void setGame(ChessGame* game);

public slots:
	QString positionChanged();

signals:
	void addComment(int ply, const QString& score);

private slots:
	void onMoveMade(const Chess::GenericMove& move, const QString& sanString, const QString& comment);
private:
	void onDataSourceChanged(EntrySource source);

private:
	Ui::ResultsWidget* ui;
	FlowLayout* m_flowLayout;

	std::shared_ptr<Solution> solution;
	std::shared_ptr<Solver> solver;
	EntrySource data_source;
	ChessGame* game;
};

#endif // RESULTS_H
