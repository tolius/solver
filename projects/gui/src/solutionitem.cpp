#include "solutionitem.h"
#include <solution.h>
#include "board/move.h"


SolutionItem::SolutionItem(const QString& name, std::shared_ptr<Solution> solution, SolutionItem* parentItem)
	: m_name(name)
	, m_solution(solution)
	, m_parentItem(parentItem)
	, m_highlight(false)
{
}

SolutionItem::~SolutionItem()
{
	qDeleteAll(m_childItems);
}

void SolutionItem::appendChild(SolutionItem* item)
{
	m_childItems.append(item);
}

void SolutionItem::deleteChild(SolutionItem* item)
{
	m_childItems.removeOne(item);
}

void SolutionItem::clear()
{
	m_name.clear();
	m_solution.reset();
	m_highlight = false;
	m_childItems.clear();
}

SolutionItem* SolutionItem::child(int row)
{
	if (row < 0 || row >= m_childItems.size())
		return nullptr;
	return m_childItems.at(row);
}

int SolutionItem::childCount() const
{
	return m_childItems.count();
}

int SolutionItem::row() const
{
	if (m_parentItem)
		return m_parentItem->m_childItems.indexOf(const_cast<SolutionItem*>(this));

	return 0;
}

int SolutionItem::columnCount() const
{
	return NUM_COLUMNS;
}

QVariant SolutionItem::data(int column) const
{
	if (column < 0 || column >= NUM_COLUMNS)
		return QVariant();
	switch (column) {
	case 0:
		return hasChildren() ? QString("%1 (%2)").arg(m_name).arg(m_childItems.size()) : m_name;
	case 1:
		return m_solution ? m_solution->score() : "";
	case 2:
		return m_solution ? m_solution->info() : "";
	case 3:
		return m_solution ? m_solution->nodes() : "";
	default:
		return QVariant();
	}
}

bool SolutionItem::hasChildren() const
{
	return !m_solution || !m_childItems.isEmpty();
}

SolutionItem* SolutionItem::parentItem()
{
	return m_parentItem;
}

void SolutionItem::resetHighlight()
{
	m_highlight = false;
	for (auto& child : m_childItems)
		child->resetHighlight();
}

void SolutionItem::setHighlight(bool flag)
{
	m_highlight = flag;
}

std::shared_ptr<Solution> SolutionItem::solution() const
{
	return m_solution;
}

const QString& SolutionItem::name() const
{
	return m_name;
}

bool SolutionItem::highlight() const
{
	return m_highlight;
}
