#ifndef SOLUTIONSWIDGET_H
#define SOLUTIONSWIDGET_H

#include "solutionsmodel.h"

#include <QWidget>
#include <QItemSelection>

#include <filesystem>


namespace Ui {
	class SolutionsWidget;
}
class GameViewer;
struct SolutionData;


class SolutionsWidget : public QWidget
{
	Q_OBJECT

public:
	explicit SolutionsWidget(SolutionsModel* solutionsModel, QWidget* parent = nullptr, GameViewer* gameViewer = nullptr);
	virtual ~SolutionsWidget();

signals:
	//void solutionsFolderChanged(const QString& dir);
	void solutionSelected(QModelIndex index);
	void solutionDeleted(QModelIndex index);
	void solutionAdded(std::shared_ptr<SolutionData> data);
	void solutionImported(const std::filesystem::path& filepath);

public slots:
	void on_openAll();
	void on_newSolution();
	void on_editSolution();
	void on_importSolutions();
	void on_openSolution();
	void on_deleteSolution();
	void on_itemSelected(QModelIndex index);
	void on_selectedItemChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
	void setDirectory(const QString& dir, bool to_fix = true);
	bool editSolution(SolutionItem* item);
	void updateEditButton(bool visible);

public:
	static QString fixDirectory(const QString& dir);

private:
	QString directory;
	SolutionsModel* solutionsModel;
	GameViewer* gameViewer;

	Ui::SolutionsWidget* ui;
};

#endif // SOLUTIONSWIDGET_H
