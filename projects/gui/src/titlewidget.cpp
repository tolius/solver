#include "titlewidget.h"
#include "ui_titlewidget.h"

#include "solution.h"
#include "board/board.h"


TitleWidget::TitleWidget(QWidget* parent)
	: QWidget(parent)
	, ui(new Ui::TitleWidget)
{
	ui->setupUi(this);
}

void TitleWidget::setSolution(std::shared_ptr<Solution> solution)
{
	this->solution = solution;
	if (!solution)
	{
		ui->label_Name->setText("");
		return;
	}
	auto [opening, branch] = solution->branchToShow(true);
	if (!branch.isEmpty())
		branch = tr("<h3>%1</h3>").arg(branch);
	ui->label_Name->setText(tr("<h1>%1</h1>%2").arg(opening).arg(branch));
}
