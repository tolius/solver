#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "positioninfo.h"

#include <QDialog>


namespace Ui {
	class SettingsDialog;
}

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	SettingsDialog(QWidget* parent = nullptr);
	virtual ~SettingsDialog();

protected:
	void showEvent(QShowEvent* event) override;

private slots:
	void onStylesChanged(QString styles);
	void onScaleChanged(QString scale);
	void onTintChanged(int value);
private:
	void onEngineChanged(const QString& engine_name);

signals:
	void stylesChanged(QString styles);
	void showBoardChanged(bool show_board);
	void showCurrentLineChanged(bool show_curr_line);
	void fontSizeChanged(int size);
	void engineChanged(const QString& engine_filename);
	void engineHashChanged(int hash_size);
	void engineNumThreadsChanged(int num_threads);
	void bookCacheChanged(int cache_size);
	void boardUpdateFrequencyChanged(UpdateFrequency frequency);
	void logUpdateFrequencyChanged(UpdateFrequency frequency);
	void solverMoveOrderChanged(SolverMoveOrder order);

private:
	void readSettings();

	Ui::SettingsDialog* ui;
};

#endif // SETTINGSDIALOG_H
