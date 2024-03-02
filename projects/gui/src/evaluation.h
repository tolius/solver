#ifndef EVALUATION_H
#define EVALUATION_H

#include "movenumbertoken.h"
#include "movetoken.h"
#include "movecommenttoken.h"
#include "board/move.h"
#include "moveevaluation.h"

#include <QWidget>
#include <QPointer>
#include <QUrl>
#include <QPair>
#include <QList>
#include <QStringList>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QThread>

#include <array>
#include <set>
#include <chrono>
#include <map>
#include <tuple>
#include <memory>
#include <functional>

class PgnGame;
class ChessGame;
class ChessEngine;
class UciEngine;
class EngineOption;
class HumanPlayer;
class GameViewer;
struct SolutionEntry;
namespace Chess
{ 
	class Board;
}
class FlowLayout;
class Solution;
class Solver;
class QHBoxLayout;
class QLabel;

namespace Ui {
	class EvaluationWidget;
}

struct EngineSession
{
	std::chrono::steady_clock::time_point start_time;
	int multi_pv;
	bool is_auto;
	int multi_mode;
	int prev_multi_mode;
	bool was_multi;
	bool to_repeat;

	EngineSession();

	void reset();
};

class Evaluation : public QWidget
{
	Q_OBJECT

	constexpr static int NUM_PV_LABELS = 18;

public:
	Evaluation(GameViewer* game_viewer, QWidget* parent = nullptr);

	void setSolver(std::shared_ptr<Solver> solver);
	void setEngine(const QString* filename = nullptr);
	void setGame(ChessGame* game);

private:
	enum class WinStatus
	{
		Win,
		EGWin,
		Unknown,
		EGLoss,
		Loss
	};

public slots:
	void positionChanged();
	void gotoCurrentMove();
	void engineChanged(const QString& engine_filename);
	void engineHashChanged(int hash_size);
	void engineNumThreadsChanged(int num_threads);

signals:
	void tintChanged(QColor tint, bool to_color_move_buttons = false);
	void Message(const QString& msg);
	void ShortMessage(const QString& msg);
	void reportGoodMoves(const std::set<QString>& good_moves);
	void reportBadMove(const QString& bad_move);
	//void startSolver(std::shared_ptr<Chess::Board> board, std::function<void(QString)> message);

private slots:
	void onEngineReady();
	void onEngineInfo(const QString& info);
	void onEngineQuit();
	void onEngineEval(const MoveEvaluation& eval);
	void onEngineStopped();
	void onEngineFinished();
	void onTimerEngine();
	void onSaveClicked();
	void onOverrideToggled(bool flag);
	void onMultiPVClicked(int multiPV);
	void onEvaluatePosition();
private:
	void onEngineToggled(bool flag);

private:
	void updateBoard(Chess::Board* board);
	void setMode(bool is_auto);
	void updateStartStop();
	void clearEvals();
	void clearEvalLabels();
	std::tuple<std::shared_ptr<SolutionEntry>, Chess::Move> currData() const;
	void reportWinStatus(WinStatus status);
	void sync_positions();
	void startEngine();
	void stopEngine(bool restart = false);
	QStringList process_moves(const MoveEvaluation& eval);
	void processEngineOutput(const MoveEvaluation& eval, const QString& best_move);
	void updateBadMove(const QString& san);
	void checkProgress(quint64 nodes, bool no_progress, int depth);

private:
	Ui::EvaluationWidget* ui;
	FlowLayout* m_flowLayout;
	GameViewer* game_viewer;
	std::array<QLabel*, NUM_PV_LABELS> move_labels;
	std::map<int, QToolButton*> multiPV_buttons;
	//QThread solver_thread;

	bool is_auto;
	std::shared_ptr<Solver> solver;
	UciEngine* engine;
	uint8_t engine_version;
	ChessGame* game;
	std::shared_ptr<Chess::Board> board_;
	size_t num_pieces;
	HumanPlayer* opponent;
	QList<EngineOption*> options;
	QStringList variants;
	quint64 curr_key;
	int curr_depth;
	int best_time;
	int progress_time;
	int t_progress;
	int best_score;
	int abs_score;
	bool is_endgame;
	bool is_score_ok;
	int move_score;
	int depth_limit;
	bool is_good_moves;
	std::set<QString> good_moves;
	bool is_only_move;
	bool is_super_boost;
	QString best_move;
	std::chrono::steady_clock::time_point t_last_eval_update;
	int last_eval_depth;
	bool is_position_update;
	bool is_restart;
	bool to_keep_solving;

	EngineSession session;
	QTimer timer_engine;
	QString eval_pv_1;
	std::chrono::steady_clock::time_point t_last_T1_change;
};

#endif // EVALUATION_H
