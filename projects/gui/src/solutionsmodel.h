#ifndef SOLUTIONS_MODEL_H
#define SOLUTIONS_MODEL_H

#include "solutionitem.h"

#include <QAbstractItemModel>
#include <QFileInfo>

#include <map>
#include <list>
#include <memory>


class Solution;


class SolutionsModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	explicit SolutionsModel(const std::map<QString, std::list<std::shared_ptr<Solution>>>& solutions, QObject* parent = nullptr);
	~SolutionsModel();

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& index) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

public:
	SolutionItem* item(const QModelIndex& index) const;
	void highlight(const QModelIndex& index);
	void addSolution(const std::shared_ptr<Solution> solution);
	void addSolutions(const std::map<QString, std::list<std::shared_ptr<Solution>>>& solutions);
	void deleteSolution(const QModelIndex& index);
	void deleteAll();

//public slots:
//	void setSolution(const SolutionItem& solution);

//private:
//	void setupModelData(const QStringList& lines, SolutionItem* parent);

private:
	SolutionItem* rootItem;
	//QList<SolutionItem> solutions;
};

#endif // SOLUTIONS_MODEL_H
