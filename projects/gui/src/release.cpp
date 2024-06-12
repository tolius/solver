#include "release.h"
#include "ui_releasewidget.h"

#include "solverresults.h"
#include "solutionbook.h"

#include <QMessageBox>

#include <array>
#include <tuple>

using namespace std;


Release::Release(QWidget* parent)
	: QWidget(parent)
	, ui(new Ui::ReleaseWidget)
	, book_type(NO_BOOK)
{
	ui->setupUi(this);

	auto set_readonly = [](QCheckBox* checkbox)
	{
		checkbox->setAttribute(Qt::WA_TransparentForMouseEvents);
		checkbox->setFocusPolicy(Qt::NoFocus);
	};
	set_readonly(ui->chk_Up_AllBranches);
	set_readonly(ui->chk_Up_Improved);
	set_readonly(ui->chk_Up_Verified);
	set_readonly(ui->chk_Up_Expanded);
	set_readonly(ui->chk_Low_AllBranches);
	set_readonly(ui->chk_Low_Improved);
	set_readonly(ui->chk_Low_Verified);
	set_readonly(ui->chk_Low_Shortened);
	//ui->widget_UpperLevel->setEnabled(false);
	//ui->widget_LowerLevel->setEnabled(false);
	ui->widget_ResultInfo->setVisible(false);

	connect(ui->btn_BookNone,       &QToolButton::toggled, this, [this]() { onBookTypeChanged(NO_BOOK);             });
	connect(ui->btn_BookLowerLevel, &QToolButton::toggled, this, [this]() { onBookTypeChanged(FileType_book);       });
	connect(ui->btn_BookUpperLevel, &QToolButton::toggled, this, [this]() { onBookTypeChanged(FileType_book_upper); });
	connect(ui->btn_BookShort,      &QToolButton::toggled, this, [this]() { onBookTypeChanged(FileType_book_short); });

	connect(ui->btn_Verify, &QPushButton::clicked, this, &Release::verify);
	connect(ui->btn_Shorten, &QPushButton::clicked, this, &Release::shorten);
}

void Release::setSolver(std::shared_ptr<SolverResults> solver)
{
	if (this->solver)
		disconnect(this->solver.get(), nullptr, this, nullptr);
	this->solver = solver;
	if (solver)
		connect(solver.get(), &Solver::solvingStatusChanged, this, [this]() { updateData(); });
	book_type = NO_BOOK;
	updateData();
}

void Release::blockBookSignals(bool flag)
{
	for (auto btn : { ui->btn_BookNone, ui->btn_BookLowerLevel, ui->btn_BookUpperLevel, ui->btn_BookShort })
		btn->blockSignals(flag);
}

void Release::onBookTypeChanged(FileType type)
{
	if (book_type == type)
		return;

	auto btn = type == FileType_book  ? ui->btn_BookLowerLevel
	    : type == FileType_book_upper ? ui->btn_BookUpperLevel
	    : type == FileType_book_short ? ui->btn_BookShort
	                                  : ui->btn_BookNone;
	book_type = type;
	if (!btn->isChecked())
	{
		blockBookSignals(true);
		btn->setChecked(true);
		blockBookSignals(false);
	}

	updateData();
}

void Release::updateData()
{
	if (book_type != NO_BOOK)
	{
		bool exists = solver->solution()->fileExists(book_type);
		if (!exists)
			book_type = NO_BOOK;
	}

	using TypeBtn = tuple<FileType, QToolButton*>;
	array<TypeBtn, 3> type_buttons = 
	{
		TypeBtn(FileType_book_short, ui->btn_BookShort), 
		TypeBtn(FileType_book,       ui->btn_BookLowerLevel), 
		TypeBtn(FileType_book_upper, ui->btn_BookUpperLevel)
	};

	for (auto& [t, btn] : type_buttons)
	{
		bool exists = solver->solution()->fileExists(t);
		btn->setEnabled(exists);
		if (exists && book_type == NO_BOOK)
		{
			book_type = t;
			blockBookSignals(true);
			btn->setChecked(true);
			blockBookSignals(false);
		}
	}

	ui->btn_BookNone->setEnabled(book_type == NO_BOOK);
	if (book_type == NO_BOOK)
	{
		blockBookSignals(true);
		ui->btn_BookNone->setChecked(true);
		blockBookSignals(false);
	}

	QSettings s(solver->solution()->path(FileType_spec), QSettings::IniFormat);
	s.beginGroup("info");
	int state_upper_level = s.value("state_upper_level", (int)SolutionInfoState::unknown).toInt();
	int state_lower_level = s.value("state_lower_level", (int)SolutionInfoState::unknown).toInt();
	s.endGroup();
	//ui->widget_UpperLevel->setEnabled(book_type == FileType_book_upper);
	//ui->widget_LowerLevel->setEnabled(book_type == FileType_book || book_type == FileType_book_short);
	ui->chk_Up_AllBranches->setChecked(state_upper_level & (int)SolutionInfoState::all_branches);
	ui->chk_Up_Improved->setChecked(state_upper_level & (int)SolutionInfoState::improved);
	ui->chk_Up_Verified->setChecked(state_upper_level & (int)SolutionInfoState::verified);
	ui->chk_Up_Expanded->setChecked(state_upper_level & (int)SolutionInfoState::expanded);
	ui->chk_Low_AllBranches->setChecked(state_lower_level & (int)SolutionInfoState::all_branches);
	ui->chk_Low_Improved->setChecked(state_lower_level & (int)SolutionInfoState::improved);
	ui->chk_Low_Verified->setChecked(state_lower_level & (int)SolutionInfoState::verified);
	ui->chk_Low_Shortened->setChecked(state_lower_level & (int)SolutionInfoState::shortened);

	ui->btn_Verify->setEnabled(book_type != NO_BOOK && solver && !solver->isBusy());
	ui->btn_Shorten->setEnabled(book_type == FileType_book && solver && !solver->isBusy());
}

void Release::warning(const QString& info)
{
	QMessageBox::warning(this, QApplication::applicationName(), info);
}

void Release::run(const QString& action, std::function<void()> function)
{
	if (!solver)
	{
		warning(QString("To %1 the solution, please open/load it first.").arg(action));
		return;
	}
	if (solver->isBusy())
	{
		warning(QString("To %1 the solution, please stop the evaluation first.").arg(action));
		return;
	}
	if (book_type == NO_BOOK)
	{
		warning(QString("To %1 the solution, please select the book type first.").arg(action));
		return;
	}
	if (!solver->solution()->fileExists(book_type))
	{
		warning("Failed to open the selected book.");
		return;
	}

	try
	{
		function();
	}
	catch (exception e)
	{
		warning(QString("Failed to %1 the solution: %2").arg(action).arg(e.what()));
		return;
	}
	catch (...)
	{
		warning(QString("Failed to %1 the solution.").arg(action));
		return;
	}
}

void Release::verify()
{
	run("verify", [this]() { solver->verify(book_type); });
}

void Release::shorten()
{
	run("shorten", [this]() { solver->shorten(); });
}
