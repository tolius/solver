#ifndef SOLUTION_ITEM_H
#define SOLUTION_ITEM_H

#include <QList>
#include <QFileInfo>
#include <QString>
#include <QVariant>

#include <memory>


class Solution;


class SolutionItem
{
public:
	explicit SolutionItem(const QString& name, std::shared_ptr<Solution> solution = nullptr, SolutionItem* parentItem = nullptr);
	~SolutionItem();

	void appendChild(SolutionItem* item);
	void deleteChild(SolutionItem* item);
	void clear();

	SolutionItem* child(int row);
	int childCount() const;
	int columnCount() const;
	QVariant data(int column) const;
	int row() const;
	bool hasChildren() const;
	bool highlight() const;
	SolutionItem* parentItem();
	void resetHighlight();
	void setHighlight(bool flag);
	std::shared_ptr<Solution> solution() const;
	const QString& name() const;

private:
	QList<SolutionItem*> m_childItems;
	SolutionItem* m_parentItem;
	std::shared_ptr<Solution> m_solution;
	QString m_name;
	bool m_highlight;

public:
	constexpr static int NUM_COLUMNS = 4;
};

#endif // SOLUTION_ITEM_H
