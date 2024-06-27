#include "solutionsmodel.h"

#include "solution.h"
#include "board/move.h"

#include <QFont>


SolutionsModel::SolutionsModel(const SolutionCollection& solutions, QObject* parent)
	: QAbstractItemModel(parent)
{
	rootItem = new SolutionItem("", nullptr);
	addSolutions(solutions);
}

SolutionsModel::~SolutionsModel()
{
	delete rootItem;
}

void SolutionsModel::addSolutions(const SolutionCollection& solutions)
{
	for (const auto& [tag, items] : solutions)
	{
		if (tag.isEmpty())
		{
			for (auto& solution : items)
			{
				rootItem->appendChild(new SolutionItem(solution->nameToShow(), solution, rootItem));
			}
		}
		else
		{
			auto item = new SolutionItem(tag, nullptr, rootItem);
			rootItem->appendChild(item);
			for (auto& solution : items)
			{
				item->appendChild(new SolutionItem(solution->nameToShow(), solution, item));
			}
		}
	}
}

void SolutionsModel::addSolution(std::shared_ptr<Solution> solution)
{
	auto& tag = solution->subTag();
	if (tag.isEmpty())
	{
		int i = 0;
		for (; i < rootItem->childCount(); i++)
			if (rootItem->child(i)->hasChildren())
				break;
		beginInsertRows(QModelIndex(), i, i);
		rootItem->appendChild(new SolutionItem(solution->nameToShow(), solution, rootItem));
	}
	else
	{
		bool is_added = false;
		for (int i = 0; i < rootItem->childCount(); i++)
		{
			auto item = rootItem->child(i);
			if (item->hasChildren() && item->name() == tag)
			{
				beginInsertRows(index(i, 0), item->childCount(), item->childCount());
				item->appendChild(new SolutionItem(solution->nameToShow(), solution, item));
				is_added = true;
				break;
			}
		}
		if (!is_added) {
			auto item = new SolutionItem(tag, nullptr, rootItem);
			int i = rootItem->childCount();
			beginInsertRows(QModelIndex(), i, i);
			rootItem->appendChild(item);
			item->appendChild(new SolutionItem(solution->nameToShow(), solution, item));
		}
	}
	endInsertRows();
	//emit this->dataChanged(QModelIndex(), QModelIndex());
}

void SolutionsModel::deleteSolution(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	SolutionItem* item = static_cast<SolutionItem*>(index.internalPointer());
	if (!item)
		return;
	auto parent = item->parentItem();
	if (!parent)
		parent = rootItem;
	auto parent_idx = index.parent();
	beginRemoveRows(parent_idx, index.row(), index.row());
	parent->deleteChild(item);
	endRemoveRows();
	if (parent == rootItem)
		return;
	if (parent->childCount() == 0)
	{
		SolutionItem* parent_item = static_cast<SolutionItem*>(parent_idx.internalPointer());
		if (parent_item) {
			auto root = parent_idx.parent();
			beginRemoveRows(root, parent_idx.row(), parent_idx.row());
			rootItem->deleteChild(parent_item);
			endRemoveRows();
		}
	}
}

void SolutionsModel::deleteAll()
{
	beginResetModel();
	rootItem->clear();
	endResetModel();
}

//void SolutionsModel::setSolutions(const QList<SolutionItem>& solutions)
//{
//	beginResetModel();
//	this->solutions = solutions;
//	endResetModel();
//}

QModelIndex SolutionsModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	SolutionItem* parentItem;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<SolutionItem*>(parent.internalPointer());
	if (!parentItem)
		return QModelIndex();

	SolutionItem* childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	return QModelIndex();
}

QModelIndex SolutionsModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	SolutionItem* childItem = static_cast<SolutionItem*>(index.internalPointer());
	if (!childItem)
		return QModelIndex();
	SolutionItem* parentItem = childItem->parentItem();

	if (parentItem == rootItem)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

int SolutionsModel::rowCount(const QModelIndex& parent) const
{
	SolutionItem* parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<SolutionItem*>(parent.internalPointer());
	if (!parentItem)
		return 0;

	return parentItem->childCount();
}

int SolutionsModel::columnCount(const QModelIndex& parent) const
{
	if (parent.isValid()) {
		auto item = static_cast<SolutionItem*>(parent.internalPointer());
		return item->columnCount();
	}
	return rootItem->columnCount();
}

QVariant SolutionsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();
	SolutionItem* item = static_cast<SolutionItem*>(index.internalPointer());
	if (!item)
		return QVariant();

	switch (role)
	{
	case Qt::DisplayRole:
		return item->data(index.column());
	case Qt::TextAlignmentRole:
		switch (index.column())
		{
		case 0:
			return int(Qt::AlignVCenter | Qt::AlignLeft);
		case 1:
			return int(Qt::AlignVCenter | Qt::AlignRight);
		case 2:
			return int(Qt::AlignVCenter | Qt::AlignRight);
		case 3:
			return int(Qt::AlignVCenter | Qt::AlignRight);
		default:
			return QVariant();
		}
	case Qt::FontRole:
		if (item->highlight())
		{
			QFont boldFont;
			boldFont.setBold(true);
			return boldFont;
		}
		return QVariant();
	}

	return QVariant();
}

Qt::ItemFlags SolutionsModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return QAbstractItemModel::flags(index);
}

QVariant SolutionsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		//return rootItem->data(section);
		switch (section)
		{
		case 0:
			return tr("Opening");
		case 1:
			return tr("Score");
		case 2:
			return tr("Info");
		case 3:
			return tr("# Nodes");
		default:
			return QVariant();
		}
	}
	if (role == Qt::TextAlignmentRole)
	{
		switch (section)
		{
		case 0:
			return int(Qt::AlignVCenter | Qt::AlignLeft);
		case 1:
			return int(Qt::AlignVCenter | Qt::AlignRight);
		case 2:
			return int(Qt::AlignVCenter | Qt::AlignRight);
		case 3:
			return int(Qt::AlignVCenter | Qt::AlignRight);
		default:
			return QVariant();
		}
	}

	return QVariant();
}

bool SolutionsModel::hasChildren(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return QAbstractItemModel::hasChildren(parent);
	SolutionItem* item = static_cast<SolutionItem*>(parent.internalPointer());
	if (!item)
		return QAbstractItemModel::hasChildren(parent);
	return item->hasChildren();
}

//void SolutionsModel::setSolution(const SolutionItem& solution)
//{
//	for (int i = 0; i < solutions.size(); i++)
//	{
//		if (solutions.at(i).name == solution.name && solutions.at(i).tag == solution.tag)
//		{
//			if (solution.isEmpty())
//			{
//				beginRemoveRows(QModelIndex(), i, i);
//				solutions.removeAt(i);
//				endRemoveRows();
//			}
//			else
//			{
//				solutions[i] = solution;
//				QModelIndex index = this->index(i, 1);
//				emit dataChanged(index, index);
//			}
//
//			return;
//		}
//	}
//
//	if (solution.isEmpty())
//		return;
//
//	int pos = solutions.size();
//	beginInsertRows(QModelIndex(), pos, pos);
//	solutions.append(solution);
//	endInsertRows();
//}

SolutionItem* SolutionsModel::item(const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;
	return static_cast<SolutionItem*>(index.internalPointer());
}

void SolutionsModel::highlight(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	SolutionItem* item = static_cast<SolutionItem*>(index.internalPointer());
	if (!item || item->highlight() || item->hasChildren())
		return;
	rootItem->resetHighlight();
	item->setHighlight(true);
	emit this->dataChanged(QModelIndex(), QModelIndex());
}
