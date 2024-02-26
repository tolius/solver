#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

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

private slots:
	void onStylesChanged(QString styles);
private:
	void onEngineChanged(const QString& engine_name);

signals:
	void stylesChanged(const QString& styles);
	void engineChanged(const QString& engine_filename);
	void engineHashChanged(int hash_size);
	void engineNumThreadsChanged(int num_threads);
	void bookCacheChanged(int cache_size);

private:
	void readSettings();

	Ui::SettingsDialog* ui;
};

#endif // SETTINGSDIALOG_H
