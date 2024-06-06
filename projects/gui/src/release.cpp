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

	ui->widget_ResultInfo->setVisible(false);

	connect(ui->btn_BookNone,       &QToolButton::toggled, this, [this]() { onBookTypeChanged(NO_BOOK);                       });
	connect(ui->btn_BookLowerLevel, &QToolButton::toggled, this, [this]() { onBookTypeChanged(Solution::FileType_book);       });
	connect(ui->btn_BookUpperLevel, &QToolButton::toggled, this, [this]() { onBookTypeChanged(Solution::FileType_book_upper); });
	connect(ui->btn_BookShort,      &QToolButton::toggled, this, [this]() { onBookTypeChanged(Solution::FileType_book_short); });

	connect(ui->btn_Verify, &QPushButton::clicked, this, &Release::verify);
}

void Release::setSolver(std::shared_ptr<SolverResults> solver)
{
	this->solver = solver;
	updateData();
}

void Release::blockBookSignals(bool flag)
{
	for (auto btn : { ui->btn_BookNone, ui->btn_BookLowerLevel, ui->btn_BookUpperLevel, ui->btn_BookShort })
		btn->blockSignals(flag);
}

void Release::onBookTypeChanged(Solution::FileType type)
{
	if (book_type == type)
		return;

	auto btn = type == Solution::FileType_book  ? ui->btn_BookLowerLevel
	    : type == Solution::FileType_book_upper ? ui->btn_BookUpperLevel
	    : type == Solution::FileType_book_short ? ui->btn_BookShort
	                                            : ui->btn_BookNone;
	if (!btn->isChecked())
	{
		book_type = type;
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

	using TypeBtn = tuple<Solution::FileType, QToolButton*>;
	array<TypeBtn, 3> type_buttons = 
	{
		TypeBtn(Solution::FileType_book_short, ui->btn_BookShort), 
		TypeBtn(Solution::FileType_book,       ui->btn_BookLowerLevel), 
		TypeBtn(Solution::FileType_book_upper, ui->btn_BookUpperLevel)
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

	ui->btn_Verify->setEnabled(book_type != NO_BOOK && solver && !solver->isBusy());
}

void Release::warning(const QString& info)
{
	QMessageBox::warning(this, QApplication::applicationName(), info);
}

void Release::verify()
{
	if (!solver)
	{
		warning("To verify the solution, please open/load it first.");
		return;
	}
	if (solver->isBusy())
	{
		warning("To verify the solution, please stop the evaluation first.");
		return;
	}
	if (book_type == NO_BOOK)
	{
		warning("To verify the solution, please select the book type first.");
		return;
	}
	if (!solver->solution()->fileExists(book_type))
	{
		warning("Failed to open the selected book.");
		return;
	}

	try
	{
		solver->verify(book_type);
	}
	catch (exception e)
	{
		warning(QString("Failed to verify the solution: %1").arg(e.what()));
		return;
	}
	catch (...)
	{
		warning("Failed to verify the solution.");
		return;
	}
}
