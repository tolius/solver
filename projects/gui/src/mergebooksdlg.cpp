#include "mergebooksdlg.h"
#include "ui_mergebooksdlg.h"

#include "solution.h"

#include <QMessageBox>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>

#include <list>
#include <array>
#include <tuple>

using namespace std;


MergeBooksDialog::MergeBooksDialog(QWidget* parent, std::shared_ptr<SolverResults> solver)
	: QDialog(parent)
	, ui(new Ui::MergeBooksDialog)
	, book_type(FileType_SIZE)
{
	ui->setupUi(this);
	this->solver = solver;

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	
	ui->label_Opening->setText(solver->solution()->nameToShow(false));

	ui->table_Books->horizontalHeader()->setStretchLastSection(true);
	ui->table_Books->insertRow(0);
	auto num = new QTableWidgetItem("main");
	num->setTextAlignment(Qt::AlignCenter);
	ui->table_Books->setItem(0, 0, num);
	auto opening_name = new QTableWidgetItem(ui->label_Opening->text());
	ui->table_Books->setItem(0, 1, opening_name);

	updateMainBookType(FileType_SIZE);
	if (book_type == FileType_SIZE)
	{
		warning(tr("The solution \"%1\" has no books. Please first generate a book.").arg(ui->label_Opening->text()));
		accept();
		return;
	}
	
	connect(ui->btn_MainBookAuto, &QPushButton::clicked, this, [this]() { updateMainBookType(FileType_SIZE); });
	connect(ui->btn_MainBookLowerLevel, &QPushButton::clicked, this, [this]() { updateMainBookType(FileType_book); });
	connect(ui->btn_MainBookUpperLevel, &QPushButton::clicked, this, [this]() { updateMainBookType(FileType_book_upper); });
	connect(ui->btn_MainBookShort, &QPushButton::clicked, this, [this]() { updateMainBookType(FileType_book_short); });

	connect(ui->btn_Add, &QPushButton::clicked, this, &MergeBooksDialog::on_AddClicked);
	connect(ui->btn_Clear, &QPushButton::clicked, this, &MergeBooksDialog::on_ClearClicked);
	connect(ui->btn_OK, &QPushButton::clicked, this, &MergeBooksDialog::on_OKClicked);
}

FileType MergeBooksDialog::autoSelectBook() const
{
	using TypeBtn = tuple<FileType, QToolButton*>;
	array<TypeBtn, 3> type_buttons = 
	{
		TypeBtn(FileType_book_short, ui->btn_MainBookShort), 
		TypeBtn(FileType_book,       ui->btn_MainBookLowerLevel), 
		TypeBtn(FileType_book_upper, ui->btn_MainBookUpperLevel)
	};

	FileType auto_type = FileType_SIZE;
	for (auto& [t, btn] : type_buttons)
	{
		bool exists = solver->solution()->fileExists(t);
		btn->setEnabled(exists);
		if (exists && auto_type == FileType_SIZE)
			auto_type = t;
	}

	return auto_type;
}

void MergeBooksDialog::updateMainBookType(FileType type)
{
	ui->btn_MainBookAuto->blockSignals(true);
	ui->btn_MainBookLowerLevel->blockSignals(true);
	ui->btn_MainBookUpperLevel->blockSignals(true);
	ui->btn_MainBookShort->blockSignals(true);
	
	ui->btn_MainBookAuto->setChecked(type == FileType_SIZE);
	ui->btn_MainBookLowerLevel->setChecked(type == FileType_book);
	ui->btn_MainBookUpperLevel->setChecked(type == FileType_book_upper);
	ui->btn_MainBookShort->setChecked(type == FileType_book_short);

	book_type = (type == FileType_SIZE) ? autoSelectBook() : type;
	QString book_path = (book_type == FileType_SIZE) ? "" : solver->solution()->path(book_type);
	auto path = new QTableWidgetItem(book_path);
	ui->table_Books->setItem(0, 2, path);

	ui->btn_MainBookAuto->blockSignals(false);
	ui->btn_MainBookLowerLevel->blockSignals(false);
	ui->btn_MainBookUpperLevel->blockSignals(false);
	ui->btn_MainBookShort->blockSignals(false);
}

void MergeBooksDialog::warning(const QString& info)
{
	QMessageBox::warning(this, QApplication::applicationName(), info);
}

void MergeBooksDialog::on_AddClicked()
{
	QString book_folder = solver->solution()->bookFolder();
	QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Books"), book_folder, tr("Book (*.bin)"));
	for (auto& file : files)
	{
		bool is_OK = true;
		for (int i = 0; i < ui->table_Books->rowCount(); i++) {
			const auto item = ui->table_Books->item(i, 2);
			if (item && item->text() == file) {
				warning(tr("The book \"%1\" has already been added to the list.").arg(file));
				is_OK = false;
				break;
			}
		}
		if (!is_OK)
			continue;
		QFileInfo fileInfo(file);
		QString name = fileInfo.baseName().replace('_', ' ');
		name.remove(" short");
		name.remove(" up");
		auto [line, _] = parse_line(name, false);
		if (line.empty())
		{
			warning(tr("The book \"%1\" has the wrong name. Skipped.").arg(name));
			continue;
		}
		int row = ui->table_Books->rowCount();
		ui->table_Books->insertRow(row);
		auto num = new QTableWidgetItem(QString::number(row + 1));
		num->setTextAlignment(Qt::AlignCenter);
		ui->table_Books->setItem(row, 0, num);
		auto opening = new QTableWidgetItem(name);
		ui->table_Books->setItem(row, 1, opening);
		auto path = new QTableWidgetItem(file);
		ui->table_Books->setItem(row, 2, path);

		ui->btn_OK->setEnabled(true);
		ui->btn_Clear->setEnabled(true);
	}
}

void MergeBooksDialog::on_ClearClicked()
{
	assert(ui->table_Books->rowCount() >= 1);
	while (int row = ui->table_Books->rowCount() - 1)
		ui->table_Books->removeRow(row--);
	ui->btn_OK->setEnabled(false);
	ui->btn_Clear->setEnabled(false);
}

void MergeBooksDialog::on_OKClicked()
{
	try
	{
		list<QString> books;
		for (int row = 0; row < ui->table_Books->rowCount(); row++)
		{
			books.push_back(ui->table_Books->item(row, 2)->text());
		}
		QString file_to_save = solver->solution()->path(book_type);
		QString ext = '.' + solver->solution()->DATA_EXT;
		if (file_to_save.endsWith(ext))
		{
			file_to_save = QString("%1_merge%2").arg(file_to_save.leftRef(file_to_save.length() - ext.length())).arg(ext);
		}
		else
		{
			warning(QString("Book \"%1\" has an incorrect extension.").arg(file_to_save));
			return;
		}
		solver->merge_books(books, file_to_save.toStdString());
	}
	catch (exception e)
	{
		warning(QString("Failed to merge books: %1").arg(e.what()));
		return;
	}
	catch (...)
	{
		warning("Failed to merge books.");
		return;
	}

	/// Accept.
	accept();
}
