#include "evaluation.h"
#include "ui_evaluationwidget.h"

#include "gameviewer.h"
#include "flowlayout/flowlayout.h"
#include "chessgame.h"
#include "solution.h"
#include "solver.h"
#include "solutionbook.h"
#include "engineconfiguration.h"
#include "enginefactory.h"
#include "engineoption.h"
#include "uciengine.h"
#include "chessplayer.h"
#include "chessgame.h"
#include "humanplayer.h"
#include "enginebuilder.h"
#include "board/board.h"
#include "board/boardfactory.h"
#include "cutechessapp.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QKeyEvent>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QStyleOptionButton>

#include <list>

using namespace std::chrono;


constexpr static quint64 NODES_PER_S = 1'500'000;
constexpr static int NO_PROGRESS_TIME = 12; // [s]
constexpr static int EG_WIN_THRESHOLD = 15200;
constexpr static int START_DEPTH = 10;
constexpr static uint8_t UNKNOWN_ENGINE_VERSION = 0xFE;

QString score_to_text(int score)
{
	if (score == MoveEvaluation::NULL_SCORE)
		return "NULL";
	return SolutionEntry::score2Text(static_cast<qint16>(score));
}

EngineSession::EngineSession()
{
	reset();
}

void EngineSession::reset()
{
	start_time = std::chrono::steady_clock::time_point::min();
	multi_pv = 5;
	is_auto = false;
	multi_mode = 0;
	prev_multi_mode = 0;
	was_multi = false;
	to_repeat = false;
}

EvalUpdate::EvalUpdate()
{
	clear();
	depth = 0;
	reset = false;
	t_update.push_back(steady_clock::now());
}

void EvalUpdate::clear()
{
	depth = 1;
	score = MoveEvaluation::NULL_SCORE;
	nps = 0;
	tb_hits = 0;
	time_ms = 0;
	best_move = "";
	move_sequence = "";
	labels.clear();
	progress = 0;
	reset = true;
}

void EvalUpdate::update_labels(std::vector<std::array<QLabel*, 3>>& move_labels)
{
	for (auto& [i, d] : labels)
	{
		move_labels[i][0]->setText(QString::number(d.depth) + ':');
		move_labels[i][1]->setText(score_to_text(d.score));
		move_labels[i][2]->setText(d.san);
	}
	labels.clear();
}

std::chrono::milliseconds EvalUpdate::interval() const
{
	if (t_update.size() < WINDOW)
		return 0ms;
	auto now = steady_clock::now();
	auto elapsed_big = now - t_update.front();
	auto elapsed_small = now - t_update.back();
	auto interval = (4s - elapsed_big) / 10 - elapsed_small;
	return interval < 0s ? 0ms : duration_cast<milliseconds>(interval);
}

void EvalUpdate::set_updated()
{
	if (t_update.size() >= WINDOW)
		t_update.pop_front();
	t_update.push_back(steady_clock::now());
}

Evaluation::Evaluation(GameViewer* game_viewer, QWidget* parent)
	: QWidget(parent)
	, ui(new Ui::EvaluationWidget)
	, game_viewer(game_viewer)
	, engine(nullptr)
	, engine_version(UNKNOWN_ENGINE_VERSION)
	, game(nullptr)
	, opponent(new HumanPlayer(nullptr))
	, solver_status(SolverStatus::Manual)
	, is_position_update(false)
	, is_restart(false)
	, to_keep_solving(false)
	, curr_game_key(0)
{
	ui->setupUi(this);

	board_.reset(Chess::BoardFactory::create("antichess"));
	board_->setFenString(board_->defaultFenString());
	ui->label_CurrentLine->setVisible(QSettings().value("ui/show_current_line", false).toBool());

	session.reset();
	timer_engine.setInterval(1s);
	connect(&timer_engine, SIGNAL(timeout()), this, SLOT(onTimerEngine()));
	timer_sync.setSingleShot(true);
	connect(&timer_sync, SIGNAL(timeout()), this, SLOT(onSyncPositions()));
	timer_eval.setSingleShot(true);
	connect(&timer_eval, SIGNAL(timeout()), this, SLOT(onUpdateEval()));
	int board_update = QSettings().value("solver/board_update", (int)UpdateFrequency::standard).toInt();
	frequency_sync = value_to_frequency(board_update);

	//m_flowLayout = new FlowLayout(ui->widget_Solution, 6, 6);
	//ui->widget_Solution->setLayout(m_flowLayout);
	move_labels = {
		{ ui->label_m01a, ui->label_m01b, ui->label_m01c }, { ui->label_m02a, ui->label_m02b, ui->label_m02c },
		{ ui->label_m03a, ui->label_m03b, ui->label_m03c }, { ui->label_m04a, ui->label_m04b, ui->label_m04c },
		{ ui->label_m05a, ui->label_m05b, ui->label_m05c }, { ui->label_m06a, ui->label_m06b, ui->label_m06c },
		{ ui->label_m07a, ui->label_m07b, ui->label_m07c }, { ui->label_m08a, ui->label_m08b, ui->label_m08c },
		{ ui->label_m09a, ui->label_m09b, ui->label_m09c }, { ui->label_m10a, ui->label_m10b, ui->label_m10c },
		{ ui->label_m11a, ui->label_m11b, ui->label_m11c }, { ui->label_m12a, ui->label_m12b, ui->label_m12c },
		{ ui->label_m13a, ui->label_m13b, ui->label_m13c }, { ui->label_m14a, ui->label_m14b, ui->label_m14c },
		{ ui->label_m15a, ui->label_m15b, ui->label_m15c }, { ui->label_m16a, ui->label_m16b, ui->label_m16c },
		{ ui->label_m17a, ui->label_m17b, ui->label_m17c }, { ui->label_m18a, ui->label_m18b, ui->label_m18c }
	};

	is_endgame = false;
	move_score = MoveEvaluation::NULL_SCORE;
	depth_limit = REAL_DEPTH_LIMIT;
	is_super_boost = false;

	ui->label_AppVersion->setText(tr("App: %1").arg(CuteChessApplication::applicationVersion()));
	clearEvals();
	ui->label_Time->setText("0 s");
	clearEvalLabels();
	ui->btn_Start->setEnabled(false);
	ui->btn_Stop->setEnabled(false);
	ui->btn_Auto->setEnabled(false);
	ui->btn_Manual->setEnabled(false);
	ui->btn_Auto->setChecked(false);
	ui->btn_Manual->setChecked(true);
	ui->btn_Save->setEnabled(false);
	ui->btn_Override->setEnabled(false);
	ui->btn_Override->setChecked(false);
	ui->btn_Sync->setEnabled(false);

	auto autoMenu = new QMenu;
	action_auto = new QAction("Auto (default)", this);
	action_auto->setToolTip(ui->btn_Auto->toolTip());
	action_auto_from_here = new QAction("Auto from here", this);
	action_auto_from_here->setEnabled(false);
	action_auto_from_here->setToolTip("Start solving the current position by automatically moving from position to position");
	action_replicate_Watkins = new QAction("Replicate Watkins solution one to one", this);
	action_replicate_Watkins->setToolTip("Copy moves from the existing Watkins solution one to one");
	action_replicate_Watkins_Override = new QAction("Replicate Watkins solution and revise it", this);
	action_replicate_Watkins_Override->setToolTip("Copy moves from the existing Watkins solution and revise it");
	action_replicate_Watkins_EG = new QAction("Replicate Watkins solution with endgames", this);
	action_replicate_Watkins_EG->setToolTip("Copy moves from the existing Watkins solution and add all endgames / overridden moves");
	autoMenu->addAction(action_auto);
	autoMenu->addAction(action_auto_from_here);
	//autoMenu->addSeparator();
	autoMenu->addAction(action_replicate_Watkins);
	autoMenu->addAction(action_replicate_Watkins_Override);
	autoMenu->addAction(action_replicate_Watkins_EG);
	autoMenu->setToolTipsVisible(true);
	ui->btn_Auto->setMenu(autoMenu);

	auto syncMenu = new QMenu;
	action_sync = new QAction("Auto sync", this);
	action_sync->setCheckable(true);
	action_sync->setChecked(false);
	action_sync->setToolTip("Automatically synchronise the evaluated position with the board periodically");
	syncMenu->addAction(action_sync);
	syncMenu->setToolTipsVisible(true);
	ui->btn_Sync->setMenu(syncMenu);

	std::array<QToolButton*, 8> buttons = { ui->btn_MultiPV_Auto, ui->btn_MultiPV_1, ui->btn_MultiPV_2,  ui->btn_MultiPV_3,
		                                    ui->btn_MultiPV_4,    ui->btn_MultiPV_6, ui->btn_MultiPV_12, ui->btn_MultiPV_18 };

	for (auto btn : buttons) {
		int val = btn->text().toInt();
		multiPV_buttons[val] = btn;
		connect(btn, &QToolButton::toggled, this, [=]() { this->onMultiPVClicked(val); });
	}

	int font_size = QSettings().value("ui/font_size", 8).toInt();
	if (font_size <= 0)
		font_size = 8;
	fontSizeChanged(font_size);
	connect(CuteChessApplication::instance(), SIGNAL(fontSizeChanged(int)), this, SLOT(fontSizeChanged(int)));

	connect(ui->btn_Auto, &QPushButton::toggled, this, [this](bool flag) { setMode(flag ? SolverStatus::Auto : SolverStatus::Manual); });
	connect(ui->btn_Manual, &QPushButton::toggled, this, [this](bool flag) { setMode(flag ? SolverStatus::Manual : SolverStatus::Auto); });
	connect(ui->btn_Save, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
	connect(ui->btn_Override, SIGNAL(toggled(bool)), this, SLOT(onOverrideToggled(bool)));
	connect(ui->btn_Start, &QPushButton::clicked, this, [this]() { onEngineToggled(true); });
	connect(ui->btn_Stop, &QPushButton::clicked, this, [this]() { onEngineToggled(false); });
	connect(ui->btn_Sync, &QToolButton::clicked, this, [this]() { onSyncPositions(board_); });
	connect(action_sync, SIGNAL(toggled(bool)), this, SLOT(onSyncToggled(bool)));
	connect(action_auto, SIGNAL(triggered()), this, SLOT(onAutoTriggered()));
	connect(action_auto_from_here, &QAction::triggered, this, [this]() { setMode(SolverStatus::AutoFromHere); });
	connect(action_replicate_Watkins, SIGNAL(triggered()), this, SLOT(onReplicateWatkinsTriggered()));
	connect(action_replicate_Watkins_EG, SIGNAL(triggered()), this, SLOT(onReplicateWatkinsEGTriggered()));
	connect(action_replicate_Watkins_Override, SIGNAL(triggered()), this, SLOT(onReplicateWatkinsOverrideTriggered()));

	setEngine();
}

void Evaluation::updatePGNline()
{
	if (ui->label_CurrentLine->isVisible()) {
		curr_game_key = game->board()->key();
		auto& ref_line = game_viewer->currentPGNline();
		QString line = highlight_difference(ref_line, curr_pgn_line);
		ui->label_CurrentLine->setText(line);
	}
}

void Evaluation::updateBoard(Chess::Board* board)
{
	if (!board)
		return;
	if (board_->key() == board->key()) {
		setStyleSheet("");
		return;
	}
	ui->btn_Save->setEnabled(false);
	//ui->btn_Save->setText("Save move");
	ui->btn_Override->setChecked(false);
	board_.reset(board->copy());
	if (ui->label_CurrentLine->isVisible())
	{
		curr_pgn_line = get_move_stack(board_, false, 499);
		updatePGNline();
		emit currentLineChanged(curr_pgn_line);
	}
	updateSync();
}

QString bkg_color_style(const QColor& bkg)
{
	return QString("background-color: rgba(%1, %2, %3, %4);").arg(bkg.red()).arg(bkg.green()).arg(bkg.blue()).arg(bkg.alpha());
}

void Evaluation::updateSync()
{
	bool is_on = engine && engine->isReady() && game && game->board();
	ui->btn_Sync->setEnabled(is_on);
	ui->widget_Engine->setStyleSheet("");
	bool needs_sync = is_on && game->board()->key() != board_->key();
	if (!needs_sync)
		return;

	int add_tint = 20; // 4 * std::max(3, QSettings().value("ui/status_highlighting", 5).toInt());
	QColor bkg = ui->widget_Engine->palette().color(QWidget::backgroundRole());
	bkg = QColor(bkg.red() + add_tint, bkg.green() + add_tint, bkg.blue() + add_tint);
	ui->widget_Engine->setStyleSheet(bkg_color_style(bkg));
}

void Evaluation::updateOverride()
{
	ui->btn_Override->setEnabled(solver 
	                             && solver->solution() 
	                             && game->board()
	                             && game->board()->sideToMove() == solver->solution()->winSide()
	                             && solver->isCurrentBranch(game->board()));
}

void Evaluation::updateSave(bool check_curr_pos)
{	
	bool enabled = solver && solver->solution() && board_ && (board_->sideToMove() == solver->solution()->winSide()) && board_->key() == curr_key;
	if (enabled && check_curr_pos && solver->isSolving())
		enabled = game->board() && solver->positionToSolve()->key() == game->board()->key();
	if (enabled)
		enabled = solver->whatToSolve() ? solver->whatToSolve()->alt_step == NO_ALT_STEPS : solver->isCurrentBranch(board_);
	ui->btn_Save->setEnabled(enabled);
}

void Evaluation::setMode(SolverStatus new_status)
{
	auto mode = new_status == SolverStatus::CopyWatkins   ? SolverMode::Copy_Watkins
	    : new_status == SolverStatus::CopyWatkinsOverride ? SolverMode::Copy_Watkins_Override
	    : new_status == SolverStatus::CopyWatkinsEG       ? SolverMode::Copy_Watkins_EG
	                                                      : SolverMode::Standard;
	if (!solver)
		new_status = SolverStatus::Manual;
	bool is_changed = (new_status != solver_status);
	solver_status = new_status;
	
	// Update UI
	ui->btn_Auto->blockSignals(true);
	action_auto_from_here->blockSignals(true);
	ui->btn_Manual->blockSignals(true);
	ui->btn_Auto->setChecked(new_status != SolverStatus::Manual);
	ui->btn_Manual->setChecked(new_status == SolverStatus::Manual);
	ui->btn_Auto->blockSignals(false);
	action_auto_from_here->blockSignals(false);
	ui->btn_Manual->blockSignals(false);
	bool is_auto_enabled = solver 
		                && engine 
		                && new_status == SolverStatus::Manual 
		                && engine->isReady() 
		                && engine->state() != ChessPlayer::State::Thinking;
	ui->btn_Auto->setEnabled(is_auto_enabled);
	action_auto_from_here->setEnabled(is_auto_enabled && game && game->board() && game->board()->sideToMove() == solver->sideToWin()
	                                  && solver->isCurrentBranch(game->board()));
	ui->btn_Manual->setEnabled(solver && engine && new_status != SolverStatus::Manual && engine->isReady());
	updateStartStop();
	ui->widget_NumBestMoves->setVisible(engine && new_status == SolverStatus::Manual && engine->isReady());
	ui->label_LoadingEngine->setVisible(!engine || !engine->isReady());
	// if (ui->btn_Auto->isChecked() || ui->btn_AutoFromHere->isChecked())
	//	ui->btn_MultiPV_Auto->setChecked(true);

	if (solver && is_changed)
	{
		if (solver_status == SolverStatus::Manual) {
			timer_sync.stop();
			solver->stop();
		}
		else if (engine)
		{
			auto pos = (solver_status == SolverStatus::AutoFromHere && game) ? game->board() : nullptr;
			auto warn = [this](const QString& text) { QMessageBox::warning(this, QApplication::applicationName(), text); };
			solver->start(pos, warn, mode);
		}
	}
	
	if (solver_status == SolverStatus::Manual && engine) // && game && board_ != game->board())
		positionChanged();
}

void Evaluation::updateStartStop()
{
	bool is_ok = engine && engine->isReady();
	using s = ChessPlayer::State;
	ui->btn_Start->setEnabled(is_ok && solver_status == SolverStatus::Manual && ((engine->state() == s::Observing) || (engine->state() == s::Idle)));
	ui->btn_Stop->setEnabled(is_ok && (engine->state() == ChessPlayer::State::Thinking || solver_status != SolverStatus::Manual));
	bool is_auto_enabled = solver && is_ok && solver_status == SolverStatus::Manual && engine->state() != ChessPlayer::State::Thinking;
	ui->btn_Auto->setEnabled(is_auto_enabled);
	action_auto_from_here->setEnabled(is_auto_enabled && game && game->board() && game->board()->sideToMove() == solver->sideToWin() && solver->isCurrentBranch(game->board()));
}

void Evaluation::clearEvals()
{
	curr_key = board_->key();
	curr_depth = 1;
	best_time = 0;
	progress_time = 0;
	t_progress = 0;
	best_score = MoveEvaluation::NULL_SCORE;
	abs_score = MoveEvaluation::NULL_SCORE;
	is_score_ok = true;
	is_good_moves = true;
	good_moves.clear();
	is_only_move = false;
	best_move.clear();
}

void Evaluation::clearEvalLabels()
{
	for (auto& line : move_labels)
		for (auto& label : line)
			label->clear();
	ui->label_Depth->clear();
	ui->label_Mate->clear();
	ui->label_BestMove->clear();
	ui->label_EngineMoves->clear();
	ui->label_Speed->setText(tr("%L1 Mn/s").arg(0, 5, 'f', 2, QChar('0')));
	ui->label_EGTB->setText(tr("%L1 kn/s").arg(0, 5, 'f', 2, QChar('0')));
	ui->btn_Save->setText("Save move");
	ui->progressBar->setValue(0);
}

void Evaluation::setSolver(std::shared_ptr<Solver> solver)
{
	if (this->solver == solver)
		return;
	if (engine && engine->state() == ChessPlayer::State::Thinking) {
		to_keep_solving = false;
		stopEngine();
	}
	this->solver = solver;
	//solver->moveToThread(&solver_thread);
	connect(solver.get(), &Solver::evaluatePosition, this, &Evaluation::onEvaluatePosition);
	connect(solver.get(), &Solver::solvingStatusChanged, this, &Evaluation::onSolvingStatusChanged);
	//solver_thread.start();
	ui->label_SolutionInfo->setText("");
	ui->widget_SolutionInfo->setVisible(false);
	//ui->widget_Solution->setVisible(solution && !solution->isEmpty());
	bool enable_Watkins = solver && solver->solution() && solver->solution()->hasWatkinsSolution();
	action_replicate_Watkins->setEnabled(enable_Watkins);
	action_replicate_Watkins_EG->setEnabled(enable_Watkins);
	action_replicate_Watkins_Override->setEnabled(enable_Watkins);
	setMode(SolverStatus::Manual);
}

void Evaluation::setGame(ChessGame* game)
{
	if (this->game)
		disconnect(this->game);
	this->game = game;
	if (game) {
		connect(this->game, SIGNAL(moveMade(Chess::GenericMove, QString, QString)), this, SLOT(positionChanged()));
		connect(this->game, SIGNAL(positionSet()), this, SLOT(positionChanged()));
		updateBoard(game->board());
	}
	if (!game || !game->board())
		ui->btn_Override->setEnabled(false);
}

void Evaluation::setEngine(const QString* filename)
{
	ui->label_Engine->setText("");
	ui->label_EngineVersion->setText("");
	ui->label_EGTBs->setText("Tablebase: 0");
	if (engine) {
		engine->quit();
		engine->deleteLater();
		engine = nullptr;
	}
	/// Configuration
	QSettings s;
	s.beginGroup("engine");
	QString engine_filename = filename ? *filename : s.value("filename").toString();
	if (engine_filename.isEmpty()) {
		if (!filename)
			QMessageBox::warning(this, QApplication::applicationName(),
				tr("Failed to load engine. Please select the engine in settings."));
		setMode(SolverStatus::Manual);
		return;
	}
	EngineConfiguration config;
	//config.setName("UciEngine"); // if it's "UciEngine", the name will be changed according to the engine output "id name ..."
#ifdef _WIN32
	config.setCommand(engine_filename);
#else
	config.setCommand("./" + engine_filename);
#endif
	config.addArgument("solver");
	QString path_exe = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
	QString path_engines_dir = QDir::toNativeSeparators(path_exe + "/engines/");
	QString path_engine = path_engines_dir + engine_filename;
	QFileInfo fi_engine(path_engine);
	if (!fi_engine.exists()) {
		QMessageBox::warning(this, QApplication::applicationName(),
		    tr("Failed to load engine: %1\n\n Please select the engine in settings.").arg(fi_engine.filePath()));
		setMode(SolverStatus::Manual);
		return;
	}
	QFileInfo fi_egtb(QDir::toNativeSeparators(path_exe + "/EGTB"));
	QString path_egtb = fi_egtb.filePath();
	if (!fi_egtb.exists()) {
		QMessageBox::warning(this, QApplication::applicationName(),
		                     tr("Failed to load EGTB: %1\n\n Please check the paths in settings.").arg(path_egtb));
		setMode(SolverStatus::Manual);
		return;
	}
	config.setWorkingDirectory(path_engines_dir);
	config.setProtocol("uci");
	config.setTimeoutScale(120.0);
	/// Create an instance
	EngineBuilder builder(config);
	QString error;
	engine = qobject_cast<UciEngine*>(builder.create(nullptr, nullptr, this, &error));
	if (engine == nullptr) {
		QMessageBox::critical(this, tr("Engine Error"), error);
		setMode(SolverStatus::Manual);
		return;
	}
	/// Options
	int hash = static_cast<int>(s.value("hash", 1.0).toDouble() * 1024);
	engine->setOption("Hash", hash);
	engine->setOption("Threads", s.value("threads", 2).toInt());
	// engine->setOption("UCI_Variant", "antichess");
	engine->setOption("SyzygyPath", path_egtb);
	engine->setOption("SyzygyProbeLimit", 4);
	/// Connections
	connect(engine, SIGNAL(ready()), this, SLOT(onEngineReady()));
	connect(engine, SIGNAL(infoMessage(const QString&)), this, SLOT(onEngineInfo(const QString&)));
	connect(engine, SIGNAL(disconnected()), this, SLOT(onEngineQuit()));
	// connect(engine, SIGNAL(destroyed()), this, SIGNAL(detectionFinished()));
	connect(engine, SIGNAL(thinking(const MoveEvaluation&)), this, SLOT(onEngineEval(const MoveEvaluation&)));
	connect(engine, SIGNAL(stoppedThinking()), this, SLOT(onEngineStopped()));
	//connect(engine, SIGNAL(debugMessage(QString)), this, SIGNAL(Message(const QString&)));
	connect(engine, SIGNAL(moveMade(const Chess::Move&)), this, SLOT(onEngineFinished()));

	setMode(SolverStatus::Manual);
}

void Evaluation::onEngineReady()
{
	Q_ASSERT(engine != nullptr);
	Q_ASSERT(engine->state() != ChessPlayer::Disconnected);

	qDeleteAll(options);
	options.clear();

	for (const EngineOption* option : engine->options())
		options << option->copy();
	variants = engine->variants();

	engine->newGame(Chess::Side::NoSide, opponent, board_.get());
	//?? waiting here?
	QString name = engine->name();
	ui->label_Engine->setText(name);
	int i = name.lastIndexOf(' ');
	if (i > 0)
	{
		auto version = name.midRef(i + 1);
		engine_version = UNKNOWN_ENGINE_VERSION;
		if (version.length() == 6)
		{
			QString str_num = version.right(2) + version.mid(2, 2) + version.left(2);
			bool ok;
			int num = str_num.toInt(&ok);
			if (ok) {
				engine_version = 
				      (240903 <= num && num <= 240910) ? LATEST_ENGINE_VERSION // 5 // fix static eval overflow
				    : (240701 <= num && num <= 240831) ? 4 // fix en passant in endgames
				    : (240226 <= num && num <= 240630) ? 3 // use F-SF depths, use new EGTB
				    : (num == 230811)                  ? 2 // increase depth when fast mate and lots of pieces
				    : (num == 230803)                  ? 1 // add go ... mate xx
				    : (230409 <= num && num <= 230415) ? 1
				    : (230301 <= num && num <= 230401) ? 0
				                                       : UNKNOWN_ENGINE_VERSION;
				ui->label_Engine->setText(name.left(i + 1) + str_num);
			}
		}
		if (engine_version == UNKNOWN_ENGINE_VERSION) {
			ui->label_EngineVersion->setText("(unknown version)");
			QMessageBox::critical(this, tr("Engine Error"), tr("Unknown engine.\n\nStatus code %1").arg(version));
			engine->quit();
			engine->deleteLater();
			engine = nullptr;
			return;
		}
		else {
			ui->label_EngineVersion->setText(tr("v.%1").arg(engine_version));
		}
	}
	else
	{
		ui->label_EngineVersion->setText("");
	}
	positionChanged();
	setMode(SolverStatus::Manual);
}

void Evaluation::onEngineInfo(const QString& info)
{
	if (info.startsWith("Found ") && (info.endsWith(" EGTBs") || info.endsWith(" EGTB"))) {
		int i = info.lastIndexOf(' ');
		auto num = info.midRef(6, i - 6);
		bool ok;
		int n = num.toInt(&ok);
		if (ok)
			ui->label_EGTBs->setText(tr("Tablebase: %1").arg(num));
	}
	else if (info.startsWith("EGTB ")) {
		int i = info.indexOf(" of ", 5);
		if (i > 0) {
			bool ok;
			int n1 = info.midRef(5, i - 5).toInt(&ok);
			if (!ok)
				return;
			int n2 = info.midRef(i + 4).toInt(&ok);
			if (!ok)
				return;
			if (n1 == n2)
				ui->label_EGTBs->setText(tr("Tablebase: %1").arg(n1));
			else
				ui->label_EGTBs->setText(tr("Tablebase: %1").arg(info.midRef(5)));
		}
	}
}

void Evaluation::onEngineQuit()
{
	if (engine == nullptr)
		return;
	Q_ASSERT(engine->state() == ChessPlayer::Disconnected);
	if (engine->hasError())
		QMessageBox::critical(this, tr("Engine Error"), engine->errorString());
	engine->deleteLater();
	engine = nullptr;
	setMode(SolverStatus::Manual);
}

std::tuple<std::shared_ptr<SolutionEntry>, Chess::Move> Evaluation::currData() const
{
	if (!solver || !board_
			|| best_move.isEmpty() || best_score == MoveEvaluation::NULL_SCORE
			|| best_score < -MATE_VALUE || best_score > MATE_VALUE
			|| curr_key != board_->key()
			|| curr_depth < 2
			|| best_time < 0)
		return { nullptr, Chess::Move() };

	auto move = board_->moveFromString(best_move);
	if (move.isNull())
		return { nullptr, Chess::Move() };
	auto pgMove = OpeningBook::moveToBits(board_->genericMove(move));

	quint32 depth_time = static_cast<quint32>(std::min((int)REAL_DEPTH_LIMIT, curr_depth));
	depth_time |= static_cast<quint32>(std::min(0xFFFF, best_time)) << 16;
	depth_time |= static_cast<quint32>(engine_version) << 8;
	qint16 score = static_cast<qint16>(best_score);
	auto data = std::make_shared<SolutionEntry>(pgMove, score, depth_time);
	return { data, move };
}

void Evaluation::onSaveClicked()
{
	auto now = steady_clock::now();
	if (now - t_last_T1_change < 1000ms)
		return;
	t_last_T1_change = now;
	auto [data, move] = currData();
	bool is_action = solver->save(board_, move, data, is_only_move, false);
	if (is_action || move.isNull() || !board_ || curr_key != board_->key())
		return;
	std::shared_ptr<Chess::Board> ref_board(board_->copy());
	ref_board->makeMove(move);
	onSyncPositions(ref_board);
}

void Evaluation::onOverrideToggled(bool flag)
{
	int add_tint = 4 * std::max(3, QSettings().value("ui/status_highlighting", 5).toInt());
	if (flag)
	{
		QColor bkg = ui->btn_Override->palette().color(QWidget::backgroundRole());
		bkg.setBlue(bkg.blue() + add_tint);
		ui->btn_Override->setStyleSheet(bkg_color_style(bkg));
		emit tintChanged(QColor(0, 0, add_tint, 0), true);
	}
	else
	{
		ui->btn_Override->setStyleSheet("");
		emit tintChanged(QColor());
	}
}

void Evaluation::positionChanged()
{
	ui->btn_Override->setEnabled(false);
	if (!game || !engine) {
		ui->btn_Sync->setEnabled(false);
		setStyleSheet("");
		return;
	}
	if (ui->btn_Override->isChecked() && game->board() && !game->board()->MoveHistory().empty())
	{
		auto prev_key = game->board()->MoveHistory().back().key;
		std::shared_ptr<SolutionEntry> data;
		if (prev_key == board_->key())
			data = std::get<0>(currData());
		solver->saveOverride(game->board(), data);
		ui->btn_Override->setChecked(false);
	}
	updateSave(true);
	Chess::Board* new_board = (solver_status != SolverStatus::Manual && solver) ? solver->positionToSolve().get() : game->board();
	if (!new_board)
		new_board = game->board();
	using s = ChessPlayer::State;
	bool is_ready = engine->isReady() && (engine->state() == s::Observing || engine->state() == s::Idle);
	if (!is_ready)
	{
		updateSync();
		updateOverride();
		if (engine->state() == s::Thinking) {
			if (board_->key() == new_board->key())
				return;
			is_restart = true;
			stopEngine();
		}
		is_position_update = true;
		return;
	}
	is_position_update = false;
	reportWinStatus(WinStatus::Unknown);//?? check the solution if it's winning?
	ui->label_SolutionInfo->setText("");
	ui->widget_SolutionInfo->setVisible(false);
	updateBoard(new_board); // updateSync() is called here
	updateOverride();
	action_auto_from_here->setEnabled(ui->btn_Auto->isEnabled() && solver && game->board() && game->board()->sideToMove() == solver->sideToWin() && solver->isCurrentBranch(game->board()));
	if (!board_)
		return;
	engine->write(tr("position fen %1").arg(board_->fenString()));
	/// UI
	if (!solver || !solver->solution())
		return;
	QString info = solver->solution()->positionInfo(board_);
	if (info.isEmpty())
		return;
	ui->label_SolutionInfo->setText(info);
	ui->widget_SolutionInfo->setVisible(true);
}


void Evaluation::gotoCurrentMove()
{
	if (!solver)// || !solver->solution())
		return;
	onSyncPositions();
}

void Evaluation::engineChanged(const QString& engine_filename)
{
	if (engine && engine->state() == ChessPlayer::State::Thinking) {
		to_keep_solving = false;
		stopEngine();
	}
	setEngine(&engine_filename);
}

void Evaluation::engineHashChanged(int hash_size)
{
	if (!engine)
		return;
	if (engine->state() == ChessPlayer::State::Thinking) {
		to_keep_solving = false;
		stopEngine();
	}
	//int hash_size = static_cast<int>(QSettings().value("engine/hash", 1.0).toDouble() * 1024);
	engine->setOption("Hash", hash_size);
}

void Evaluation::engineNumThreadsChanged(int num_threads)
{
	if (!engine)
		return;
	if (engine->state() == ChessPlayer::State::Thinking) {
		to_keep_solving = false;
		stopEngine();
	}
	engine->setOption("Threads", QSettings().value("engine/threads", 2).toInt());
}

void Evaluation::onAutoTriggered()
{
	if (!ui->btn_Auto->isEnabled() || ui->btn_Auto->isChecked() || action_auto_from_here->isChecked())
		return;
	this->setMode(SolverStatus::Auto);
}

void Evaluation::onReplicateWatkinsTriggered()
{
	if (!ui->btn_Auto->isEnabled() || ui->btn_Auto->isChecked() || action_auto_from_here->isChecked())
		return;
	this->setMode(SolverStatus::CopyWatkins);
}

void Evaluation::onReplicateWatkinsOverrideTriggered()
{
	if (!ui->btn_Auto->isEnabled() || ui->btn_Auto->isChecked() || action_auto_from_here->isChecked())
		return;
	this->setMode(SolverStatus::CopyWatkinsOverride);
}

void Evaluation::onReplicateWatkinsEGTriggered()
{
	if (!ui->btn_Auto->isEnabled() || ui->btn_Auto->isChecked() || action_auto_from_here->isChecked())
		return;
	this->setMode(SolverStatus::CopyWatkinsEG);
}

void Evaluation::onSyncToggled(bool flag)
{
	if (flag)
		onSyncPositions(board_);
	else
		timer_sync.stop();
}

void Evaluation::onSyncPositions(std::shared_ptr<Chess::Board> ref_board)
{
	if (!game)
		return;
	auto game_board = game->board();
	if (!game_board)
		return;
	if (!ref_board) {
		if (!solver)
			return;
		ref_board = solver->positionToSolve();
	}
	if (!ref_board)
		return;

	timer_sync.stop();
	t_last_sync = steady_clock::now();
	try
	{
		std::lock_guard<std::mutex> lock_source(ref_board->change_mutex);
		game->setPosition(ref_board.get());
	}
	catch (std::exception e)
	{
		emit Message(tr("Failed to synchronise the board: %1.").arg(e.what()), MessageType::std);
	}
	//updateSync();
	//updateOverride();
	//positionChanged();
}

void Evaluation::startEngine()
{
	if (engine->state() != ChessPlayer::State::Observing && engine->state() != ChessPlayer::State::Idle)
		return;

	eval_pv_1 = "";
	session.is_auto = (solver_status != SolverStatus::Manual); //!! multiPV_buttons...
	to_keep_solving = session.is_auto && solver;
	int mate = 0;
	bool to_repeat = session.to_repeat;
	if (to_repeat)
	{
		session.to_repeat = false;
		if (session.multi_mode != session.prev_multi_mode) {
			bool to_save_multi = (solver 
								  && solver->solution()
								  && session.prev_multi_mode > 0 
								  && session.multi_mode == 0
								  && board_->sideToMove() == solver->solution()->winSide()
								  && solver->positionToSolve()
								  && solver->positionToSolve()->key() == board_->key());
			session.prev_multi_mode = session.multi_mode;
			int boost_value = is_super_boost ? 11 : 5;
			session.multi_pv = (session.multi_mode == 0 || !solver) ? 1 
								: (session.multi_mode == 1) ? boost_value 
								: solver->settings().multiPV_2_num;
			if (to_save_multi) {
				auto [data, move] = currData();
				solver->save(board_, move, data, is_only_move, true);
			}
		}
	}
	else
	{
		if (!board_->result().isNone())
			return; // otherwise engine returns "null"
		bool auto_multi_pv = ui->btn_MultiPV_Auto->isChecked();
		clearEvals();
		bool is_solving = solver
			&& solver->solution()
			&& (board_->sideToMove() == solver->solution()->winSide())
			&& solver->positionToSolve()
			&& (solver->positionToSolve()->key() == board_->key());
		bool is_multi_boost;
		if (is_solving)
		{
			auto ss = solver->whatToSolve();
			if (ss) {
				is_endgame = ss->is_endgame;
				move_score = ss->move_score;
				depth_limit = solver->settings().max_depth;
				is_super_boost = ss->is_super_boost;
				is_multi_boost = ss->is_multi_boost;
				mate = ss->mate;
				ui->label_Alt->setText(ss->alt_step >= 0 ? tr("A%1").arg(ss->alt_step) : "");
			}
			else {
				is_solving = false;
			}
		}
		if (!is_solving)
		{
			is_endgame = false;
			move_score = MoveEvaluation::NULL_SCORE;
			depth_limit = solver ? solver->settings().max_depth : REAL_DEPTH_LIMIT;
			is_super_boost = false;
			is_multi_boost = true;
			ui->label_Alt->setText("");
		}
		ui->label_Time->setText("0 s");
		if (solver && (session.is_auto || auto_multi_pv))
		{
			const auto& s = solver->settings();
			int boost_value = is_super_boost ? 11 : 5;
			session.multi_mode = !is_multi_boost ? 0 : s.multiPV_2_stop_time > 0 ? 2 : 1;
			session.multi_pv = (session.multi_mode == 2) ? s.multiPV_2_num : (session.multi_mode == 1) ? boost_value : 1;
		}
		else
		{
			session.multi_mode = 0;
			for (auto& [val, btn] : multiPV_buttons) {
				if (btn->isChecked()) {
					session.multi_pv = std::max(1, val);
					break;
				}
			}
		}
		session.prev_multi_mode = session.multi_mode;
		session.was_multi = (/*(!session.is_auto && !auto_multi_pv) ||*/ session.multi_mode == 0); //!! false;
		session.start_time = steady_clock::now();
		timer_engine.start();
		updateSave(false);
		//ui->btn_Save->setText("Save move");
	}
	num_pieces = board_->numPieces();
	engine->setOption("MultiPV", session.multi_pv);
	quint64 num_nodes = (solver && session.is_auto) ? solver->settings().max_search_time / 2 * NODES_PER_S : 0;
	engine->go(board_.get(), num_nodes, mate);
	if (!to_repeat) {
		std::lock_guard<std::mutex> lock(eval_data.mtx);
		eval_data.clear();
		if (timer_eval.isActive())
			timer_eval.stop();
		timer_eval.start(500ms);
		updateStartStop();
	}
}

void Evaluation::stopEngine(bool to_repeat)
{
	if (!to_repeat)
		session.to_repeat = false;
	engine->stopThinking();
}

void Evaluation::onEngineToggled(bool flag)
{
	if (solver_status != SolverStatus::Manual && flag)
		return;

	if (flag) {
		startEngine();
	}
	else {
		to_keep_solving = false;
		stopEngine();
	//	if (solver_status != SolverStatus::Manual)
	//		setMode(SolverStatus::Manual);
	}
}

void Evaluation::reportWinStatus(WinStatus status)
{
	if (ui->btn_Override->isChecked())
		return;

	int tint = QSettings().value("ui/status_highlighting", 5).toInt();

	switch (status)
	{
	case WinStatus::Win:
		emit tintChanged(QColor(0, tint * 4, 0, 0));
		break;
	case WinStatus::EGWin:
		emit tintChanged(QColor(0, tint * 2, tint, 0));
		break;
	case WinStatus::EGLoss:
		emit tintChanged(QColor(tint * 2, 0, tint, 0));
		break;
	case WinStatus::Loss:
		emit tintChanged(QColor(tint * 4, 0, 0, 0));
		break;
	case WinStatus::Unknown:
	default:
		emit tintChanged(QColor());
		break;
	}
}

QStringList Evaluation::process_moves(const MoveEvaluation& eval)
{
	QStringList san_moves;
	std::shared_ptr<Chess::Board> board = board_;
	try
	{
		int pv = eval.pvNumber();
		bool is_new_pv_1 = (pv == 1);
		QString new_eval_pv = eval.pv();
		if (is_new_pv_1) {
			if (eval_pv_1.startsWith(new_eval_pv))
				return QStringList(best_move);
			eval_pv_1 = new_eval_pv;
			board.reset(board->copy());
		}
		auto moves = new_eval_pv.split(' ', Qt::SkipEmptyParts);
		for (size_t i = 0; i < std::min(20, moves.size()); i++)
		{
			auto move = board->moveFromLanString(moves[i]);
			if (move.isNull())
				break;
			QString san = board->moveString(move, Chess::Board::StandardAlgebraic);
			san_moves.append(san);
			if (!is_new_pv_1)
				break;
			board->makeMove(move);
		}
	}
	catch (std::exception e)
	{
		emit Message(QString("Evaluation exception: %1").arg(e.what()), MessageType::error);
	}
	return san_moves;
}

void Evaluation::onEngineEval(const MoveEvaluation& eval)
{
	if (eval.score() == MoveEvaluation::NULL_SCORE || eval.pvNumber() == 0)
		return;
	int depth = eval.depth();
	if (depth < START_DEPTH || depth < curr_depth)
		return;
	int pv = std::max(1, eval.pvNumber());
	auto san_moves = process_moves(eval);
	QString eval_best_move = san_moves.empty() ? "" : san_moves.takeFirst();
	
	static std::mutex prc_mtx;
	std::lock_guard<std::mutex> prc_lock(prc_mtx);
	processEngineOutput(eval, eval_best_move);
	
	std::lock_guard<std::mutex> lock(eval_data.mtx);
	if (pv == 1 && eval_data.depth <= eval.depth())
	{
		eval_data.depth = eval.depth();
		eval_data.score = eval.score();
		eval_data.time_ms = eval.time();
		eval_data.nps = eval.nps();
		eval_data.tb_hits = eval.tbHits();
		eval_data.best_move = eval_best_move;
		if (!san_moves.isEmpty())
			eval_data.move_sequence = get_san_sequence(board_->plyCount() + 3, san_moves);
	}
	if (pv - 1 < move_labels.size())
	{
		if (session.multi_mode == 0 && pv == 1)
		{
			auto it = eval_data.labels.find(0);
			if (it == eval_data.labels.end() && move_labels[0][2]->text() != eval_best_move)
			{
				for (int i = 1;  i < move_labels.size(); i++)
				{
					if (move_labels[i][2]->text() == move_labels[0][2]->text()) {
						move_labels[i][0]->setText(move_labels[0][0]->text());
						move_labels[i][1]->setText(move_labels[0][1]->text());
						break;
					}
				}
			}
		}
		eval_data.labels[pv - 1] = { eval.depth(), eval.score(), eval_best_move };
		if (timer_eval.isActive()) {
			if (eval_data.reset) {
				timer_eval.stop();
				timer_eval.start(eval_data.interval());
			}
		}
		else {
			timer_eval.start(eval_data.interval());
		}
	}
}

void Evaluation::onUpdateEval()
{
	std::lock_guard<std::mutex> lock(eval_data.mtx);
	timer_eval.stop();
	eval_data.set_updated();
	if (eval_data.reset) {
		clearEvalLabels();
		eval_data.reset = false;
	}
	if (eval_data.best_move.isEmpty())
		return;
	ui->label_Depth->setText(tr("%1:").arg(eval_data.depth));
	ui->label_Mate->setText(score_to_text(eval_data.score));
	if (!eval_data.best_move.isEmpty())
	{
		ui->label_BestMove->setText(eval_data.best_move);
		ui->btn_Save->setText("Save " + eval_data.best_move);
		ui->label_EngineMoves->setText(eval_data.move_sequence);
	}
	ui->label_Speed->setText(tr("%L1 Mn/s").arg(static_cast<double>(eval_data.nps) / 1'000'000, 5, 'f', 2, QChar('0')));
	if (eval_data.time_ms == 0)
	{
		ui->label_EGTB->setText(tr("%L1 kn/s").arg(0, 5, 'f', 2, QChar('0')));
	}
	else
	{
		double speed_egtb = static_cast<double>(eval_data.tb_hits) / eval_data.time_ms;
		ui->label_EGTB->setText(tr("%L1 kn/s").arg(speed_egtb, 5, 'f', 2, QChar('0')));
	}

	eval_data.update_labels(move_labels);
	ui->progressBar->setValue(eval_data.progress);
}

void Evaluation::processEngineOutput(const MoveEvaluation& eval, const QString& str_move)
{
	int pv = std::max(1, eval.pvNumber());
	int depth = eval.depth();
	int curr_score = eval.score();
	constexpr static int NULL_SCORE = MoveEvaluation::NULL_SCORE;
	bool is_good_update = (best_score == NULL_SCORE || best_score < ABOVE_EG || curr_score >= best_score);
	int delta_depth = is_good_update ? depth - curr_depth : 0;
	if (is_good_update)
		curr_depth = depth;
	quint64 nodeCount = eval.nodeCount();
	quint64 nodes = nodeCount + eval.tbHits() * 100;
	const SolverSettings* s = solver ? &solver->settings() : nullptr;
	bool is_bad_move = (curr_score < -10'000);
	if (pv == 1)
	{
		quint64 curr_time = nodes / NODES_PER_S;
		if (delta_depth > 0)
			progress_time = (progress_time > 0) ? (progress_time + (curr_time - best_time) / delta_depth) / 2
				: (best_time > 0)               ? (curr_time - best_time) / delta_depth
												: 0;
		if (is_good_update)
		{
			best_time = curr_time;
			if (best_score != NULL_SCORE && (abs(best_score - curr_score) >= 100 || (curr_score > ABOVE_EG && best_score != curr_score)))
				t_progress = best_time;
			best_move = str_move;
			best_score = curr_score;
			abs_score = is_endgame ? abs(best_score) : best_score;

			if (solver && abs_score < s->min_score && board_->sideToMove() == solver->sideToWin()) {
				auto now = steady_clock::now();
				if (now - t_last_not_win_warning >= 5'000ms) {
					emit Message(tr("...NOT WINNING at depth=%1! %2 in %3").arg(depth).arg(str_move).arg(get_move_stack(board_)), MessageType::warning);
					t_last_not_win_warning = now;
				}
			}
			is_score_ok = (!solver || !s->dont_lose_winning_sequence || move_score == NULL_SCORE
							|| abs(move_score) <= WIN_THRESHOLD || abs_score > abs(move_score));
			depth_limit = get_max_depth(best_score, num_pieces);
		}
		else
		{
			is_score_ok = false;
		}
		if (is_good_moves)
			good_moves.clear();
		if (game && !game->isFinished() && game->board() && game->board()->key() == board_->key()) {
			if (eval.score() >= WIN_THRESHOLD)
				reportWinStatus(WinStatus::Win);
			else if (eval.score() <= -WIN_THRESHOLD)
				reportWinStatus(WinStatus::Loss);
			else if (eval.score() >= EG_WIN_THRESHOLD)
				reportWinStatus(WinStatus::EGWin);
			else if (eval.score() <= -EG_WIN_THRESHOLD)
				reportWinStatus(WinStatus::EGLoss);
		}
	}
	else if (!is_endgame && (move_score == NULL_SCORE || !solver || abs(move_score) < s->score_limit - 1))
	{
		if (pv == 2)
			is_only_move = true;
		is_only_move = is_only_move && is_bad_move;
	}
	if (/* session.multi_mode > 0 &&*/ !is_endgame && (!solver || s->show_gui) && game && game->board() && game->board()->key() == board_->key())
	{
		if (is_bad_move && (!solver || board_->sideToMove() == solver->sideToWin()))
			updateBadMove(str_move);
		else if (is_good_moves)
			good_moves.insert(str_move);
	}
	/// Check the results
	bool no_progress = (best_time - t_progress > NO_PROGRESS_TIME || best_time == 0 || (solver && s && best_score >= s->multiPV_stop_score));
	if (best_move.isEmpty())
		emit Message(tr("!!NONE MOVE in %1").arg(get_move_stack(board_, false, 400)), MessageType::warning);
	if (session.is_auto && pv == 1 && is_only_move && solver
		&& (abs_score > abs(move_score)
		    || (no_progress && s && abs_score < s->score_limit && (move_score == NULL_SCORE || abs(move_score) < s->score_limit - 1))))
	{
		updateProgress(100);
		stopEngine();
		return;
	}
	checkProgress(nodes, no_progress, depth);
}

void Evaluation::updateBadMove(const QString& bad_move)
{
	bool is_first = is_good_moves;
	is_good_moves = false;
	if (is_first)
	{
		emit reportGoodMoves(good_moves);
		return;
	}
	auto it = good_moves.find(bad_move);
	if (it == good_moves.end())
		return;
	good_moves.erase(it);
	emit reportBadMove(bad_move);
}

void Evaluation::checkProgress(quint64 nodes, bool no_progress, int depth)
{
	if (!solver)
		return;
	const SolverSettings& s = solver->settings();
	constexpr static int NULL_SCORE = MoveEvaluation::NULL_SCORE;
	quint64 move_time = nodes / NODES_PER_S;
	int ti = (int)move_time;
	bool is_score_good = false;
	if (abs_score > s.score_to_add_time || is_endgame || is_super_boost || (move_score == NULL_SCORE && abs(move_score) > ABOVE_EG))
	{
		// Check if depth is sufficient
		is_score_good = (move_score == NULL_SCORE || abs(move_score) <= ABOVE_EG || abs_score > abs(move_score));
		if (session.is_auto && no_progress && is_score_good && depth > depth_limit)
		{
			updateProgress(100);
			if (solver_status != SolverStatus::Manual) {
				stopEngine();
				return;
			}
			else {
				session.is_auto = false;
			}
		}
		if (ti > s.std_engine_time)
		{
			// Check if progress is expected
			if (!is_score_ok || !no_progress || (abs_score <= 15'000 && progress_time < s.add_engine_time / 5)
			    || (15'000 < abs_score && abs_score <= ABOVE_EG) || (progress_time <= 0)
			    || (depth + s.add_engine_time / progress_time > get_max_depth(abs_score + 1, num_pieces)))
			{
				ti -= s.add_engine_time;
			}
			if (!is_score_ok)
			{
				if (best_time - t_progress > s.std_engine_time && best_score < 32767 - 28)
				{
					if (progress_time < s.add_engine_time) {
						ti -= s.add_engine_time;
						emit ShortMessage("=");
					}
					else {
						emit ShortMessage("-");
					}
				}
				else
				{
					ti -= s.add_engine_time_to_ensure_winning_sequence;
					emit ShortMessage("+");
				}
			}
		}
	}
	int depth_to_stop = is_score_good ? std::min(s.max_depth, depth_limit) : s.max_depth;
	int max_val = no_progress ? 99 : 98;
	int depth_progress = std::max(1, depth - START_DEPTH) * max_val / std::max(1, depth_to_stop - START_DEPTH);
	int time_progress = ti * max_val / s.std_engine_time / session.multi_pv;
	int progress_value = std::max(1, std::min(max_val, std::max(depth_progress, time_progress)));
	if (progress_value > eval_data.progress)
		updateProgress(progress_value);
	if (no_progress && (ti > s.std_engine_time || depth >= s.max_depth) && (session.is_auto || session.multi_pv == 1))
	{
		updateProgress(100);
		if (session.is_auto)
		{
			if (!is_score_ok) {
				QString move_stack = get_move_stack(board_);
				emit Message(tr("...Bad sequence! %1 <= %2: %3 in %4")
				                 .arg(score_to_text(abs_score))
				                 .arg(score_to_text(move_score))
				                 .arg(best_move)
				                 .arg(move_stack),
				             MessageType::warning);
			}
			if (solver_status != SolverStatus::Manual) {
				stopEngine();
				return;
			}
			else {
				session.is_auto = false;
			}
		}
	}
	if (session.multi_mode == 1 || (session.multi_mode == 2 && abs_score > WIN_THRESHOLD))
	{
		if (move_time > s.multiPV_stop_time || abs_score > WIN_THRESHOLD)
		{
			session.multi_mode = 0;
			session.was_multi = true;
			session.to_repeat = true;
			stopEngine(true);
			return;
		}
	}
	else if (session.multi_mode == 2)
	{
		if (move_time > s.multiPV_2_stop_time)
		{
			session.multi_mode = 1;
			session.to_repeat = true;
			stopEngine(true);
			return;
		}
	}
	else if (!session.was_multi && depth < s.multiPV_boost_depth
	         && abs_score < s.multiPV_stop_score) // && move_time < s.multiPV_stop_time)  // <  s.min_depth)
	{
		session.multi_mode = 1;
		session.to_repeat = true;
		stopEngine(true);
		return;
	}
}

void Evaluation::updateProgress(int val)
{
	std::lock_guard<std::mutex> lock(eval_data.mtx);
	eval_data.progress = val;
	if (!timer_eval.isActive())
		timer_eval.start(eval_data.interval());
}

void Evaluation::onEngineStopped()
{
	auto& eval = engine->evaluation();
	if (eval.depth() > 200) // happens in endgames where onEngineEval() may not be called even once
		onEngineEval(eval);
}

void Evaluation::onEngineFinished()
{
	if (is_position_update || is_restart) {
		timer_engine.stop();
		session.reset();
		is_position_update = false;
		positionChanged();
		if (is_restart) {
			is_restart = false;
			startEngine();
		}
	}
	else if (session.to_repeat) {
		startEngine();
	}
	else {
		timer_engine.stop();
		session.reset();
		if (solver_status != SolverStatus::Manual && solver) {
			if (to_keep_solving) {
				auto [data, move] = currData();
				solver->process(board_, move, data, is_only_move);
			}
			else {
				solver->stop();
				setMode(SolverStatus::Manual);
			}
		}
	}
	updateStartStop();
}

void Evaluation::onTimerEngine()
{
	auto time_s = duration_cast<seconds>(steady_clock::now() - session.start_time).count();
	if (time_s < 5 * 60) // 5 min
		ui->label_Time->setText(tr("%L1 s").arg(time_s));
	else if (time_s < 5 * 60 * 60) // 5 h
		ui->label_Time->setText(tr("%L1 min").arg(time_s / 60));
	else if (time_s < 100 * 60 * 60) // 100 h
		ui->label_Time->setText(tr("%L1 h").arg(time_s / 60 / 60));
	else
		ui->label_Time->setText(tr("%L1 days").arg(time_s / 24 / 60 / 60));
}

void Evaluation::onMultiPVClicked(int multiPV)
{
	//for (auto& [btn, val] : multiPV_buttons) 
	//{
	//	btn->blockSignals(true);
	//	btn->setChecked(val == multiPV);
	//	btn->blockSignals(false);
	//}
	auto btn = multiPV_buttons[multiPV];
	if (btn->isChecked())
		return;
	btn->blockSignals(true);
	btn->setChecked(true);
	btn->blockSignals(false);
	if (solver_status == SolverStatus::Manual && multiPV > 0 && engine && engine->state() == ChessPlayer::State::Thinking)
	{
		is_restart = true;
		to_keep_solving = false;
		stopEngine();
	}
}

void Evaluation::onEvaluatePosition()
{
	if (solver_status == SolverStatus::Manual)
	{
		solver->stop();
		return;
	}
	if (solver) {
		auto solver_board = solver->positionToSolve();
		if (solver_board && game->board() && game->board()->key() != solver_board->key())
		{
			if (frequency_sync != UpdateFrequency::never && action_sync->isChecked())
			{
				auto delay = (frequency_sync == UpdateFrequency::always) ? 500ms
				    : (frequency_sync == UpdateFrequency::frequently)    ? 1s
				    : (frequency_sync == UpdateFrequency::infrequently)  ? 30s
				                                                         : 4s;
				auto now = steady_clock::now();
				auto elapsed = now - t_last_sync;
				if (elapsed >= delay)
					onSyncPositions();
				else if (!timer_sync.isActive())
					timer_sync.start(duration_cast<milliseconds>(delay - elapsed));
			}
		}
	}
	positionChanged();
	startEngine();
}

void Evaluation::onSolvingStatusChanged()
{
	if (!solver || !solver->isSolving())
		setMode(SolverStatus::Manual);
}

void Evaluation::fontSizeChanged(int size)
{
	QString big = QString("font-size: %1pt; font-weight: bold;").arg(size * 12 / 8);
	ui->label_LoadingEngine->setStyleSheet(big);
	ui->label_Mate->setStyleSheet(big);
	ui->label_BestMove->setStyleSheet(big);
	QString small = QString("font-size: %1pt;").arg(size * 6 / 8);
	QString bigger = QString("font-size: %1pt;").arg(size * 9 / 8);
	for (auto& line : move_labels) {
		line[0]->setStyleSheet(small);
		line[2]->setStyleSheet(bigger);
	}
}

void Evaluation::boardUpdateFrequencyChanged(UpdateFrequency frequency)
{
	frequency_sync = frequency;
}

void Evaluation::showCurrentLine(bool show_curr_line)
{
	ui->label_CurrentLine->setVisible(show_curr_line);
	if (!show_curr_line) {
		curr_pgn_line = "";
		ui->label_CurrentLine->setText("");
	}
	else if (board_) {
		curr_pgn_line = get_move_stack(board_, false, 499);
		ui->label_CurrentLine->setText(curr_pgn_line);
	}
	emit currentLineChanged(curr_pgn_line);
}

const QString& Evaluation::currentPGNline() const
{
	return curr_pgn_line;
}

void Evaluation::gameLineChanged()
{
	if (game && game->board() && curr_game_key != game->board()->key())
		updatePGNline();
}
