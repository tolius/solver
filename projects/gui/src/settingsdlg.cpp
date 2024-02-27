#include "settingsdlg.h"
#include "ui_settingsdlg.h"

#include <gamemanager.h>
#include "cutechessapp.h"

#include <QShowEvent>
#include <QSettings>
#include <QFileDialog>
#include <QDir>

#include <filesystem>
#include <thread>
#include <cmath>


namespace fs = std::filesystem;

#ifdef WIN32
#include <windows.h>
size_t get_total_memory()
{
	MEMORYSTATUSEX memory_status;
	ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
	memory_status.dwLength = sizeof(MEMORYSTATUSEX);
	if (GlobalMemoryStatusEx(&memory_status))
		return memory_status.ullTotalPhys;
	return 0;
}
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__linux__)
#include <unistd.h>
size_t get_total_memory()
{
	size_t pages = sysconf(_SC_PHYS_PAGES);
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}
#else
size_t get_system_total_memory()
{
	return 0;
}
#endif


SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent),
	  ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	readSettings();

	fs::path path_exe = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdString();
	fs::path path_engines = path_exe / "engines";
	ui->m_enginePath->setText(QDir::toNativeSeparators(QString::fromStdString(path_engines.generic_string())));
	ui->m_egtbPath->setText(QDir::toNativeSeparators(QString::fromStdString((path_exe / "EGTB").generic_string())));
	ui->m_WatkinsPath->setText(QDir::toNativeSeparators(QString::fromStdString((path_exe / "Watkins").generic_string())));

	std::list<std::string> filenames;
	for (const auto& entry : fs::directory_iterator(path_engines))
	{
		if (entry.is_regular_file())
		{
			const auto& path = entry.path();
#ifdef WIN32
			std::string extension = path.extension().generic_string();
			if (extension != ".exe")
				continue;
#endif
			filenames.push_back(path.filename().generic_string());
		}
	}
	QSettings s;
	s.beginGroup("engine");
	std::string engine_name = s.value("filename", "NO").toString().toStdString();
	if (engine_name == "NO") {
		engine_name = (filenames.size() == 1) ? filenames.front() : "";
		QSettings().setValue("engine/filename", QString::fromStdString(engine_name));
	}
	auto layout = ui->m_engines->layout();
	QLayoutItem* item;
	while ((item = layout->takeAt(0)) != 0)
	{
		auto widget = item->widget();
		if (widget)
			widget->setParent(nullptr);
		delete item;
	}
	auto no_engine = new QRadioButton("No engine", ui->m_engines);
	layout->addWidget(no_engine);
	if (engine_name.empty())
		no_engine->setChecked(true);
	connect(no_engine, &QRadioButton::toggled, this, 
		[=](bool flag) { 
			if (flag)
				onEngineChanged(""); 
		}
	);
	for (auto& name : filenames)
	{
		auto radio_button = new QRadioButton(QString::fromStdString(name), ui->m_engines);
		layout->addWidget(radio_button);
		if (name == engine_name)
			radio_button->setChecked(true);
		connect(radio_button, &QRadioButton::toggled, this, 
			[=](bool flag) { 
				if (flag)
					onEngineChanged(radio_button->text()); 
			}
		);
	}

	/// Engine settings
	ui->m_hashSize->setValue(s.value("hash", 1.0).toDouble());
	connect(ui->m_hashSize, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
	    [=](double val) {
			QSettings().setValue("engine/hash", val);
		    emit engineHashChanged(static_cast<int>(val * 1024));
		}
	);
	ui->m_numThreads->setValue(s.value("threads", 2).toInt());
	connect(ui->m_numThreads, qOverload<int>(&QSpinBox::valueChanged), this,
	    [=](int val)
	    {
		    QSettings().setValue("engine/threads", val);
		    emit engineNumThreadsChanged(val);
	    }
	);
	s.endGroup();

	auto num_threads = std::thread::hardware_concurrency();
	ui->m_infoThreads->setText(tr("Recommended value: %1 thread%2\n*Your CPU supports%3 %1 thread%2")
	                               .arg(num_threads)
	                               .arg(num_threads == 1 ? "" : "s")
	                               .arg(num_threads == 1 ? "" : " up to"));
	size_t total_memory = get_total_memory();
	if (total_memory > 0) {
		double total_memory_GB = static_cast<double>(total_memory / (1024 * 1024)) / 1024;
		double recommended_GB = (total_memory_GB >= 30.0) ? std::round(total_memory_GB - 7.0) : (total_memory_GB > 4.5) ? total_memory_GB - 4.0 : 0.5;
		if (total_memory_GB < 30.0 && abs(std::round(recommended_GB) - recommended_GB) < (total_memory_GB - 6.0) / 12)
			recommended_GB = std::round(recommended_GB);
		ui->m_infoRAM->setText(tr("Recommended value: %1 GB\n*Your system has %2 GB").arg(recommended_GB, 0, 'f', 1).arg(total_memory_GB, 0, 'f', 1));
	}

	/// General settings
	s.beginGroup("solver");
	ui->m_bookCache->setValue(s.value("book_cache", 1.0).toDouble());
	connect(ui->m_bookCache, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
		[=](double val)
		{
			QSettings().setValue("solver/book_cache", val);
			emit bookCacheChanged(static_cast<int>(val * 1024));
		}
	);
	s.endGroup();

	/// UI
	connect(ui->m_highlightLegalMovesCheck, &QCheckBox::toggled,
		this, [=](bool checked) {
			QSettings().setValue("ui/highlight_legal_moves", checked);
		}
	);

	connect(ui->m_showMoveArrowsCheck, &QCheckBox::toggled, this, 
		[=](bool checked) {
			QSettings().setValue("ui/show_move_arrows", checked);
		}
	);

	connect(ui->m_openLastSolution, &QCheckBox::toggled,
		this, [=](bool checked) {
			QSettings().setValue("solutions/open_last_solution", checked);
		}
	);

	connect(ui->m_moveAnimationSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		this, [=](int value)
	{
		QSettings().setValue("ui/move_animation_duration", value);
	});

	QDir resources(":/styles/");
	QStringList styles = resources.entryList(QStringList("*.qss"));
	for (auto& style : styles)
	{
		if (style.endsWith(".qss"))
			style = style.left(style.length() - 4);
	}
	ui->m_comboStyle->clear();
	ui->m_comboStyle->addItems(styles);
	QString style = QSettings().value("ui/theme", QString()).toString();
	if (style.endsWith(".qss"))
	{
		int i = ui->m_comboStyle->findText(style.left(style.length() - 4));
		if (i >= 0)
			ui->m_comboStyle->setCurrentIndex(i);
	}
	connect(ui->m_comboStyle, &QComboBox::currentTextChanged, this, &SettingsDialog::onStylesChanged);
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::onStylesChanged(QString styles)
{
	styles += ".qss";
	QSettings().setValue("ui/theme", styles);
	emit stylesChanged(styles);
}

void SettingsDialog::onEngineChanged(const QString& engine_name)
{
	QSettings s;
	s.beginGroup("engine");
	QString prev_filename = s.value("filename").toString();
	if (engine_name == prev_filename)
		return;
	s.setValue("filename", engine_name);
	emit engineChanged(engine_name);
}

void SettingsDialog::readSettings()
{
	QSettings s;

	s.beginGroup("ui");
	ui->m_highlightLegalMovesCheck->setChecked(
		s.value("highlight_legal_moves", true).toBool());
	ui->m_showMoveArrowsCheck->setChecked(
		s.value("show_move_arrows", true).toBool());
	ui->m_moveAnimationSpin->setValue(s.value("move_animation_duration", 300).toInt());
	s.endGroup();

	s.beginGroup("solutions");
	ui->m_openLastSolution->setChecked(s.value("open_last_solution", true).toBool());
	s.endGroup();
}
