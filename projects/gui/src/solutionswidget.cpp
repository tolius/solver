#include "solutionswidget.h"
#include "ui_solutionswidget.h"  

#include "solutionsmodel.h"
#include "solutionitem.h"
#include "solutiondlg.h"
#include "solver.h"
#include "importdlg.h"
#include "gameviewer.h"

#include <QFileDialog>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeView>
#include <QGroupBox>
#include <QMessageBox>


SolutionsWidget::SolutionsWidget(SolutionsModel* solutionsModel, QWidget* parent, GameViewer* gameViewer)
	: QWidget(parent)
	, ui(new Ui::SolutionsWidget)
	, solutionsModel(solutionsModel)
	, gameViewer(gameViewer)
{
	ui->setupUi(this);

	ui->treeView->setModel(solutionsModel);
	ui->treeView->expandRecursively(QModelIndex());
	//ui->treeView->setAlternatingRowColors(true);
	//ui->treeView->setRootIsDecorated(false);
	ui->btn_Edit->setVisible(false);

	QString dir = QSettings().value("solutions/path", "").toString();
	setDirectory(dir);

	connect(ui->btn_BrowseSolutions, SIGNAL(clicked()), this, SLOT(on_openAll()));
	connect(ui->btn_New, SIGNAL(clicked()), this, SLOT(on_newSolution()));
	connect(ui->btn_Edit, SIGNAL(clicked()), this, SLOT(on_editSolution()));
	connect(ui->btn_Open, SIGNAL(clicked()), this, SLOT(on_openSolution()));
	connect(ui->btn_Import, SIGNAL(clicked()), this, SLOT(on_importSolutions()));
	connect(ui->btn_Delete, SIGNAL(clicked()), this, SLOT(on_deleteSolution()));
	connect(ui->treeView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(on_itemSelected(QModelIndex)));
	connect(ui->treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this,
	        SLOT(on_selectedItemChanged(const QItemSelection&, const QItemSelection&)));
	connect(ui->treeView->model(), &QAbstractItemModel::rowsInserted,
	        [this](const QModelIndex& parent, int first, int last) { ui->treeView->expandRecursively(parent); });
}

SolutionsWidget::~SolutionsWidget()
{
	delete ui;
}

QString SolutionsWidget::fixDirectory(const QString& dir)
{
	QString directory;
	if (dir.isEmpty())
	{
		directory = QCoreApplication::applicationDirPath();
		QDir d(directory);
		d.mkdir(Solution::DATA);
		d.mkdir(Solution::BOOKS);
	}
	else
	{
		directory = dir;
		QFileInfo fi(directory + QDir::separator() + Solution::DATA);
		bool has_subfolders = fi.exists();
		if (has_subfolders) {
			QFileInfo fi(directory + QDir::separator() + Solution::BOOKS);
			has_subfolders = fi.exists();
		}
		if (!has_subfolders) {
			QFileInfo fi_data(directory + "/../" + Solution::DATA);
			if (fi_data.exists() && fi_data.isDir()) {
				QFileInfo fi_books(directory + "/../" + Solution::BOOKS);
				if (fi_books.exists() && fi_books.isDir())
					directory += "/../";
			}
		}
	}
	return QDir::cleanPath(directory);
}

void SolutionsWidget::setDirectory(const QString& dir, bool to_fix)
{
	directory = to_fix ? fixDirectory(dir) : dir;
	QSettings().setValue("solutions/path", directory);
	ui->edit_Solutions->setText(directory);

	QFileInfo fi(directory);
	bool exists = fi.exists();
	ui->btn_New->setEnabled(exists);
	ui->btn_Import->setEnabled(exists);
	ui->label_FolderDoesNotExist->setVisible(!exists);

	for (int i = 0; i < SolutionItem::NUM_COLUMNS; i++)
		ui->treeView->resizeColumnToContents(i);
}

void SolutionsWidget::setSolver(std::shared_ptr<Solver> solver)
{
	if (this->solver)
		disconnect(this->solver.get(), nullptr, this, nullptr);
	this->solver = solver;
	if (solver)
		connect(solver.get(), SIGNAL(solvingStatusChanged()), this, SLOT(on_solvingStatusChanged()));
	on_solvingStatusChanged();
}

void SolutionsWidget::on_openAll()
{
	QString def_folder = QSettings().value("solutions/path", "").toString();
	QString dir = QFileDialog::getExistingDirectory(this, tr("Select Solutions Directory"), def_folder, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (!dir.isEmpty())
	{
		auto model = dynamic_cast<SolutionsModel*>(ui->treeView->model());
		if (!model)
			return;
		model->deleteAll();
		QString path_dir = fixDirectory(dir);
		auto [solutions, warning_message] = Solution::loadFolder(path_dir);
		model->addSolutions(solutions);
		ui->treeView->expandRecursively(QModelIndex());
		setDirectory(dir, false);
		if (!warning_message.isEmpty())
			QMessageBox::warning(this, QApplication::applicationName(), warning_message);
	}
}

void SolutionsWidget::on_newSolution()
{
	QString folder = QSettings().value("solutions/path", "").toString();
	if (folder.isEmpty() || !ui->btn_New->isEnabled()) {
		QMessageBox::warning(this, QApplication::applicationName(), tr("Failed to create a new solution.\nFirst please specify a folder."));
		return;
	}
	QString tag;
	for (auto& idx : ui->treeView->selectionModel()->selectedIndexes())
	{
		if (!idx.isValid() || idx.column() != 0)
			continue;
		auto p = static_cast<SolutionItem*>(idx.internalPointer());
		if (!p)
			continue;
		if (p->hasChildren()) {
			if (p->name().isEmpty())
				continue;
			tag = p->name();
		}
		else
		{
			auto solution = p->solution();
			if (!solution || solution->subTag().isEmpty())
				continue;
			tag = solution->subTag();
		}
		break;
	}
	auto data = std::make_shared<SolutionData>();
	data->tag = tag;
	data->folder = folder;
	SolutionDialog dlg(dynamic_cast<QWidget*>(this->parent()), data, gameViewer);
	auto res = dlg.exec();
	if (res == QDialog::Accepted) {
		emit solutionAdded(dlg.resultData());
		for (int i = 0; i < SolutionItem::NUM_COLUMNS; i++)
			ui->treeView->resizeColumnToContents(i);
	}
}

bool SolutionsWidget::editSolution(SolutionItem* item)
{
	if (!item || item->hasChildren())
		return false;

	auto solution = item->solution();
	if (!solution) // || !solution->isValid())
		return false;

	SolutionDialog dlg(dynamic_cast<QWidget*>(this->parent()), solution->mainData(), gameViewer);
	dlg.setWindowTitle("Edit solution");
	auto res = dlg.exec();
	if (res == QDialog::Accepted)
		solution->edit(dlg.resultData());
	return true;
}

void SolutionsWidget::on_editSolution()
{
	for (auto& idx : ui->treeView->selectionModel()->selectedIndexes())
	{
		if (!idx.isValid() || idx.column() != 0)
			continue;
		auto item = static_cast<SolutionItem*>(idx.internalPointer());
		bool was_processed = editSolution(item);
		if (was_processed)
			return;
	}
}

void SolutionsWidget::on_importSolutions()
{
	ImportDialog dlg(dynamic_cast<QWidget*>(this->parent()));
	connect(&dlg, SIGNAL(solutionImported(const std::filesystem::path&)), this, SIGNAL(solutionImported(const std::filesystem::path&)));
	auto res = dlg.exec();
	//if (res == QDialog::Accepted)
	//	;
}

void SolutionsWidget::on_itemSelected(QModelIndex index)
{
	if (!index.isValid())
		return;
	auto item = static_cast<SolutionItem*>(index.internalPointer());
	if (!item)
		return;
	if (item->highlight()) {
		editSolution(item);
		return;
	}
	emit solutionSelected(index);
	for (int i = 0; i < SolutionItem::NUM_COLUMNS; i++)
		ui->treeView->resizeColumnToContents(i);
	updateEditButton(item->highlight());
}

void SolutionsWidget::on_selectedItemChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	for (auto& s : selected)
		for (auto& idx : s.indexes())
			if (idx.isValid()) {
				auto p = static_cast<SolutionItem*>(idx.internalPointer());
				if (p && !p->hasChildren()) {
					ui->btn_Delete->setEnabled(true);
					ui->btn_Open->setEnabled(true);
					updateEditButton(p->highlight());
					return;
				}
			}
	ui->btn_Delete->setEnabled(false);
	ui->btn_Open->setEnabled(false);
	updateEditButton(false);
}

void SolutionsWidget::on_openSolution()
{
	for (auto& idx : ui->treeView->selectionModel()->selectedIndexes())
	{
		if (!idx.isValid() || idx.column() != 0)
			continue;
		auto p = static_cast<SolutionItem*>(idx.internalPointer());
		if (p && !p->hasChildren()) {
			on_itemSelected(idx);
			return;
		}
	}
}

void SolutionsWidget::on_deleteSolution()
{
	auto selected = ui->treeView->selectionModel()->selectedIndexes();
	for (auto& idx : selected)
	{
		if (!idx.isValid() || idx.column() != 0)
			continue;
		auto p = static_cast<SolutionItem*>(idx.internalPointer());
		if (p && !p->hasChildren())
		{
			auto are_you_sure = [this](const QString& text)
			{
				int ret = QMessageBox::question(this, QApplication::applicationName(), text, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
				                                QMessageBox::No);
				return ret == QMessageBox::Yes;
			};
			auto message = [this](const QString& text)
			{
				QMessageBox::warning(this, QApplication::applicationName(), text);
			};
			bool is_deleted = p->solution()->remove(are_you_sure, message);
			if (is_deleted)
				emit solutionDeleted(idx);
		}
	}
}

void SolutionsWidget::updateEditButton(bool visible)
{
	if (visible)
	{
		ui->btn_Open->setVisible(false);
		ui->btn_Edit->setVisible(true);
	}
	else
	{
		ui->btn_Edit->setVisible(false);
		ui->btn_Open->setVisible(true);
	}
}

void SolutionsWidget::on_solvingStatusChanged()
{
	bool is_busy = solver && solver->isBusy();
	ui->btn_Edit->setEnabled(!is_busy);
}
