#ifndef RELEASE_H
#define RELEASE_H

#include "positioninfo.h"
#include "solution.h"

#include <QWidget>

#include <memory>


class SolverResults;

namespace Ui {
	class ReleaseWidget;
}


class Release : public QWidget
{
Q_OBJECT

public:
	Release(QWidget* parent = nullptr);

	void setSolver(std::shared_ptr<SolverResults> solver);
	void updateData();

private:
	void blockBookSignals(bool flag);
	void warning(const QString& info);
	void verify();

private:
	void onBookTypeChanged(Solution::FileType type);

private:
	Ui::ReleaseWidget* ui;

	std::shared_ptr<SolverResults> solver;
	Solution::FileType book_type;

private:
	constexpr static auto NO_BOOK = Solution::FileType_SIZE;
};

#endif // RELEASE_H
