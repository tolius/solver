#ifndef MERGEBOOKSDIALOG_H
#define MERGEBOOKSDIALOG_H

#include "solverresults.h"

#include <QString>
#include <QDialog>

#include <memory>


namespace Ui {
	class MergeBooksDialog;
}


class MergeBooksDialog : public QDialog
{
	Q_OBJECT

public:
	MergeBooksDialog(QWidget* parent, std::shared_ptr<SolverResults> solver);

private:
	Solution::FileType autoSelectBook() const;
	void updateMainBookType(Solution::FileType type);
	void warning(const QString& info);

private slots:
	void on_AddClicked();
	void on_ClearClicked();
	void on_OKClicked();

private:
	Ui::MergeBooksDialog* ui;
	std::shared_ptr<SolverResults> solver;
	Solution::FileType book_type;
};

#endif // MERGEBOOKSDIALOG_H
