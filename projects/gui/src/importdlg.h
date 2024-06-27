#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H

#include "move.h"
#include "solution.h"

#include <QDialog>
#include <QTableWidgetItem>

#include <filesystem>
#include <memory>
#include <map>
#include <string>

namespace Ui {
	class ImportDialog;
}


struct SolutionFile
{
	std::filesystem::path path;
	uintmax_t size;
};

struct SolutionFiles
{
	std::map<std::string, SolutionFile> Std;
	std::map<std::string, SolutionFile> New;
	QTableWidgetItem* info = nullptr;
	bool is_lower = false;

	bool isImported() const;
	void set_info(const QString& text);
};


class ImportDialog : public QDialog
{
	Q_OBJECT

public:
	ImportDialog(QWidget* parent);

private:
	void updateButtons();

signals:
	void solutionImported(const std::filesystem::path& filepath);

private slots:
	void on_BrowseFolderClicked();
	void on_OpeningsSelectionChanged();
	void on_DeleteOpeningClicked();
	void on_ImportClicked();

private:
	Ui::ImportDialog* ui;

	std::map<std::string, SolutionFiles> files;
	bool is_importing;
};

#endif // IMPORTDIALOG_H
