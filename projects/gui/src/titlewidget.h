#ifndef TITLEWIDGWT_H
#define TITLEWIDGWT_H

#include <QWidget>

#include <memory>


namespace Ui {
	class TitleWidget;
}
class Solution;


class TitleWidget : public QWidget
{
Q_OBJECT

public:
	TitleWidget(QWidget* parent = nullptr);

	void setSolution(std::shared_ptr<Solution> solution);

private:
	Ui::TitleWidget* ui;

	std::shared_ptr<Solution> solution;
};

#endif // TITLEWIDGWT_H
