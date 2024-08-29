#ifndef SOLUTIONDIALOG_H
#define SOLUTIONDIALOG_H

#include "move.h"
#include "solution.h"

#include <QString>
#include <QDialog>

#include <memory>
#include <tuple>
#include <chrono>


namespace Ui {
	class SolutionDialog;
}
class GameViewer;


class SolutionDialog : public QDialog
{
	Q_OBJECT

public:
	SolutionDialog(QWidget* parent, std::shared_ptr<SolutionData> data = nullptr, GameViewer* gameViewer = nullptr);

	std::shared_ptr<SolutionData> resultData();

private:
	void updateButtons();
	void updateWatkinsError();

private slots:
	void on_OpeningChanged();
	void on_BranchChanged();
	void on_TagChanged();
	void on_OpeningEdited();
	void on_BranchEdited();
	void on_BranchToSkipChanged();
	void on_AddBranchToSkipClicked();
	void on_BranchesToSkipSelectionChanged();
	void on_DeleteSelectedBranchesClicked();
	void on_OKClicked();
	void on_BrowseWatkinsClicked();
	void on_WatkinsSolutionChanged();

private:
	Ui::SolutionDialog* ui;
	std::shared_ptr<SolutionData> data;
	Line branch_to_skip;
	std::chrono::steady_clock::time_point t_last_opening_edited;
	std::chrono::steady_clock::time_point t_last_branch_edited;
	QString board_position;
	int WatkinsStartingPly;
};

#endif // SOLUTIONDIALOG_H
