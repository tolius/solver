#ifndef EVALUATION_H
#define EVALUATION_H

#include "movenumbertoken.h"
#include "movetoken.h"
#include "movecommenttoken.h"
#include "board/move.h"
#include "moveevaluation.h"
#include "positioninfo.h"

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
#include <vector>
#include <set>
#include <chrono>
#include <map>
#include <tuple>
#include <memory>
#include <functional>
#include <mutex>

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
class QAction;

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

struct EvalUpdate
{
	EvalUpdate();

	void clear();
	void update_labels(std::vector<std::array<QLabel*, 3>>& move_labels);
	bool is_reset() const;
	std::chrono::milliseconds interval() const;
	void set_updated();

	int depth;
	int score;
	quint64 nps;
	quint64 tb_hits;
	int time_ms;
	QString best_move;
	QString move_sequence;
	int progress;

	struct LabelData
	{
		int depth;
		int score;
		QString san;
	};
	std::map<int, LabelData> labels;

	std::list<std::chrono::steady_clock::time_point> t_update;
	std::mutex mtx;

	static constexpr size_t WINDOW = 20;
	static constexpr int DEPTH_RESET = 1;
};

class Evaluation : public QWidget
{
	Q_OBJECT

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
	enum class SolverStatus
	{
		Manual,
		Auto,
		AutoFromHere
	};

public slots:
	void positionChanged();
	void gotoCurrentMove();
	void engineChanged(const QString& engine_filename);
	void engineHashChanged(int hash_size);
	void engineNumThreadsChanged(int num_threads);
	void fontSizeChanged(int size);
	void boardUpdateFrequencyChanged(UpdateFrequency frequency);

signals:
	void tintChanged(QColor tint, bool to_color_move_buttons = false);
	void Message(const QString& msg, MessageType type = MessageType::std);
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
	void onSyncToggled(bool flag);
	void onMultiPVClicked(int multiPV);
	void onEvaluatePosition();
	void onSolvingStatusChanged();
	void onSyncPositions(std::shared_ptr<Chess::Board> ref_board = nullptr);
	void onUpdateEval();
private:
	void onEngineToggled(bool flag);

private:
	void updateBoard(Chess::Board* board);
	void updateSync();
	void updateOverride();
	void setMode(SolverStatus new_status);
	void updateStartStop();
	void clearEvals();
	void clearEvalLabels();
	std::tuple<std::shared_ptr<SolutionEntry>, Chess::Move> currData() const;
	void reportWinStatus(WinStatus status);
	void startEngine();
	void stopEngine(bool restart = false);
	QStringList process_moves(const MoveEvaluation& eval);
	void processEngineOutput(const MoveEvaluation& eval, const QString& best_move);
	void updateBadMove(const QString& san);
	void checkProgress(quint64 nodes, bool no_progress, int depth);
	void updateProgress(int val);

private:
	Ui::EvaluationWidget* ui;
	FlowLayout* m_flowLayout;
	GameViewer* game_viewer;
	std::vector<std::array<QLabel*, 3>> move_labels;
	std::map<int, QToolButton*> multiPV_buttons;
	QAction* sync_action;
	//QThread solver_thread;

	SolverStatus solver_status;
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
	bool is_position_update;
	bool is_restart;
	bool to_keep_solving;

	EngineSession session;
	QTimer timer_engine;
	QString eval_pv_1;
	std::chrono::steady_clock::time_point t_last_T1_change;

	QTimer timer_sync;
	UpdateFrequency frequency_sync;
	std::chrono::steady_clock::time_point t_last_sync;
	QTimer timer_eval;
	EvalUpdate eval_data;
};

#endif // EVALUATION_H
