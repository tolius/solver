#ifndef RELEASE_H
#define RELEASE_H

#include "positioninfo.h"
#include "solution.h"

#include <QWidget>

#include <memory>
#include <functional>


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
	void run(const QString& action, std::function<void()> function);
	void verify();
	void shorten();

private:
	void onBookTypeChanged(FileType type);

private:
	Ui::ReleaseWidget* ui;

	std::shared_ptr<SolverResults> solver;
	FileType book_type;

private:
	constexpr static auto NO_BOOK = FileType_SIZE;
};

#endif // RELEASE_H
