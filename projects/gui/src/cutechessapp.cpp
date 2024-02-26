// modified: Tolius, 2024.

/*
    This file is part of Cute Chess.
    Copyright (C) 2008-2018 Cute Chess authors

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cutechessapp.h"

#include <QCoreApplication>
#include <QDir>
#include <QTime>
#include <QFileInfo>
#include <QSettings>

#include <mersenne.h>
#include <enginemanager.h>
#include <gamemanager.h>
#include <board/boardfactory.h>
#include <chessgame.h>
#include <humanbuilder.h>

#include "mainwindow.h"
#include "settingsdlg.h"
#ifndef Q_OS_WIN32
#	include <sys/types.h>
#	include <pwd.h>
#endif


CuteChessApplication::CuteChessApplication(int& argc, char* argv[])
	: QApplication(argc, argv),
	  m_settingsDialog(nullptr),
	  m_engineManager(nullptr),
	  m_gameManager(nullptr),
	  m_initialWindowCreated(false)
{
	Mersenne::initialize(QTime(0,0,0).msecsTo(QTime::currentTime()));

	// Set the application icon
	QIcon icon;
	icon.addFile(":/icons/cutechess_512x512.png");
	icon.addFile(":/icons/cutechess_256x256.png");
	icon.addFile(":/icons/cutechess_128x128.png");
	icon.addFile(":/icons/cutechess_64x64.png");
	icon.addFile(":/icons/cutechess_32x32.png");
	icon.addFile(":/icons/cutechess_24x24.png");
	icon.addFile(":/icons/cutechess_16x16.png");
	setWindowIcon(icon);

	setQuitOnLastWindowClosed(false);

	QCoreApplication::setOrganizationName("tolius-solver");
	QCoreApplication::setOrganizationDomain("antichess.onrender.com");
	QCoreApplication::setApplicationName("Solver");
	QCoreApplication::setApplicationVersion(SOLVER_VERSION);

	// Use Ini format on all platforms
	QSettings::setDefaultFormat(QSettings::IniFormat);

	// Load the engines
	engineManager()->loadEngines(configPath() + QLatin1String("/engines.json"));

	// Set styles
	m_settingsDialog = new SettingsDialog;
	QDir resources(":/styles/");
	QStringList styles = resources.entryList(QStringList("*.qss"));
	if (!styles.isEmpty())
	{
		QString def_style = QSettings().value("ui/theme", "Combinear.qss").toString();
		QString s = styles.contains(def_style) ? def_style : styles.front();
		onStylesChanged(s);
	}

	connect(this, SIGNAL(lastWindowClosed()), this, SLOT(onLastWindowClosed()));
	connect(this, SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()));
	connect(m_settingsDialog, SIGNAL(stylesChanged(const QString&)), this, SLOT(onStylesChanged(const QString&)));
}

CuteChessApplication::~CuteChessApplication()
{
	delete m_settingsDialog;
}

CuteChessApplication* CuteChessApplication::instance()
{
	return static_cast<CuteChessApplication*>(QApplication::instance());
}

QString CuteChessApplication::userName()
{
	#ifdef Q_OS_WIN32
	return qgetenv("USERNAME");
	#else
	if (QSettings().value("ui/use_full_user_name", true).toBool())
	{
		auto pwd = getpwnam(qgetenv("USER"));
		if (pwd != nullptr)
			return QString(pwd->pw_gecos).split(',')[0];
	}
	return qgetenv("USER");
	#endif
}

QString CuteChessApplication::configPath()
{
	// We want to have the exact same config path in "gui" and
	// "cli" applications so that they can share resources
	QSettings settings;
	QFileInfo fi(settings.fileName());
	QDir dir(fi.absolutePath());

	if (!dir.exists())
		dir.mkpath(fi.absolutePath());

	return fi.absolutePath();
}

EngineManager* CuteChessApplication::engineManager()
{
	if (m_engineManager == nullptr)
		m_engineManager = new EngineManager(this);

	return m_engineManager;
}

GameManager* CuteChessApplication::gameManager()
{
	if (m_gameManager == nullptr)
	{
		m_gameManager = new GameManager(this);
		int concurrency = 1; //QSettings().value("tournament/concurrency", 1).toInt();
		m_gameManager->setConcurrency(concurrency);
	}

	return m_gameManager;
}

QList<MainWindow*> CuteChessApplication::gameWindows()
{
	m_gameWindows.removeAll(nullptr);

	QList<MainWindow*> gameWindowList;
	const auto gameWindows = m_gameWindows;
	for (const auto& window : gameWindows)
		gameWindowList << window.data();

	return gameWindowList;
}

MainWindow* CuteChessApplication::newGameWindow(ChessGame* game)
{
	MainWindow* mainWindow = new MainWindow(game, m_settingsDialog);
	m_gameWindows.prepend(mainWindow);
	mainWindow->show();
	m_initialWindowCreated = true;

	return mainWindow;
}

void CuteChessApplication::newDefaultGame()
{
	// default game is a human versus human game using standard variant and
	// infinite time control
	ChessGame* game = new ChessGame(Chess::BoardFactory::create("antichess"),
		new PgnGame());

	game->setTimeControl(TimeControl("inf"));
	game->pause();

	connect(game, SIGNAL(started(ChessGame*)),
		this, SLOT(newGameWindow(ChessGame*)));

	gameManager()->newGame(game,
			       new HumanBuilder(userName()),
			       new HumanBuilder(userName()));
}

void CuteChessApplication::showGameWindow(int index)
{
	auto gameWindow = m_gameWindows.at(index);
	gameWindow->activateWindow();
	gameWindow->raise();
}

void CuteChessApplication::showSettingsDialog()
{
	showDialog(m_settingsDialog);
}

void CuteChessApplication::onQuitAction()
{
	closeDialogs();
	closeAllWindows();
}

void CuteChessApplication::onLastWindowClosed()
{
	if (!m_initialWindowCreated)
		return;

	if (m_gameManager != nullptr)
	{
		connect(m_gameManager, SIGNAL(finished()), this, SLOT(quit()));
		m_gameManager->finish();
	}
	else
		quit();
}

void CuteChessApplication::onAboutToQuit()
{
}

void CuteChessApplication::showDialog(QWidget* dlg)
{
	Q_ASSERT(dlg != nullptr);

	if (dlg->isMinimized())
		dlg->showNormal();
	else
		dlg->show();

	dlg->raise();
	dlg->activateWindow();
}

void CuteChessApplication::closeDialogs()
{
	if (m_settingsDialog)
		m_settingsDialog->close();
}

void CuteChessApplication::onStylesChanged(const QString& styles)
{
	QFile styleFile(QString(":/styles/%1").arg(styles));
	styleFile.open(QFile::ReadOnly);
	QString sheet(styleFile.readAll());
	setStyleSheet(sheet);
}
