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

#include "plaintextlog.h"
#include "positioninfo.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QTextBlock>

PlainTextLog::PlainTextLog(QWidget* parent)
	: QPlainTextEdit(parent)
{
	setReadOnly(true);
	setLineWrapMode(NoWrap);
}

PlainTextLog::PlainTextLog(const QString& text, QWidget* parent)
	: QPlainTextEdit(text, parent)
{
	setReadOnly(true);
	setLineWrapMode(NoWrap);
}

void PlainTextLog::contextMenuEvent(QContextMenuEvent* event)
{
	QMenu* menu = createStandardContextMenu();
	QAction* applyLogMoves = menu->addAction(tr("Apply Moves"), this, 
		[this]() {
			auto text = textCursor().block().text();
		    if (text.isEmpty())
			    return;
		    static const QString tag = "[Variant \"Antichess\"]";
		    int i1 = text.indexOf(tag);
			if (i1 < 0) {
				QMessageBox::information(this, "Log", QString("The currently selected line \"%1\" does not contain any moves. Please select a line that contains moves.").arg(text));
				return;
			}
		    int i2 = text.length() - 1;
			auto check_line_end = [&](QChar ch)
			{
				int i = text.indexOf(ch, i1);
				if (i >= 0 && i < i2)
					i2 = i;
			};
		    check_line_end('#');
		    check_line_end('+');
		    check_line_end('-');
			text = text.midRef(i1 + tag.length(), i2 - i1).trimmed().toString();
		    auto [line, board] = parse_line(text, true);
		    emit applyMoves(board);
		}
	);
	connect(menu, &QMenu::aboutToShow, this,
		[this, applyLogMoves]() {
			auto text = textCursor().block().text();
			applyLogMoves->setEnabled(!text.isEmpty() && text.indexOf("[Variant \"Antichess\"]") >= 0);
		}
	);

	menu->addSeparator();
	QAction* lineWrap = menu->addAction(tr("Line Wrap"), this, 
		[this]() {
			setLineWrapMode(lineWrapMode() == NoWrap ? WidgetWidth : NoWrap);
		}
	);
	lineWrap->setCheckable(true);
	lineWrap->setChecked(lineWrapMode() == WidgetWidth);
	menu->addSeparator();

	menu->addAction(tr("Clear Log"), this, SLOT(clear()));

	auto saveAct = menu->addAction(tr("Save Log to File..."));
	connect(saveAct, &QAction::triggered, this, 
		[=]() {
			auto dlg = new QFileDialog(this, tr("Save Log"), QString(),
				tr("Text Files (*.txt);;All Files (*.*)"));
			connect(dlg, &QFileDialog::fileSelected, this, &PlainTextLog::saveLogToFile);
			dlg->setAttribute(Qt::WA_DeleteOnClose);
			dlg->setAcceptMode(QFileDialog::AcceptSave);
			dlg->open();
		}
	);

	menu->exec(event->globalPos());
	delete menu;
}

void PlainTextLog::saveLogToFile(const QString& fileName)
{
	if (fileName.isEmpty())
		return;

	QFile file(fileName);
	if (!file.open(QFile::WriteOnly | QFile::Text))
	{
		QFileInfo fileInfo(file);

		QMessageBox msgBox(this);
		msgBox.setIcon(QMessageBox::Warning);

		switch (file.error())
		{
			case QFile::OpenError:
			case QFile::PermissionsError:
				msgBox.setText(
					tr("The file \"%1\" could not be saved because "
					   "of insufficient privileges.")
					.arg(fileInfo.fileName()));

				msgBox.setInformativeText(
					tr("Try selecting a location where you have "
					   "the permissions to create files."));
			break;

			case QFile::TimeOutError:
				msgBox.setText(
					tr("The file \"%1\" could not be saved because "
					   "the operation timed out.")
					.arg(fileInfo.fileName()));

				msgBox.setInformativeText(
					tr("Try saving the file to a local or another "
					   "network disk."));
			break;

			default:
				msgBox.setText(tr("The file \"%1\" could not be saved.")
					.arg(fileInfo.fileName()));

				msgBox.setInformativeText(file.errorString());
			break;
		}
		msgBox.exec();
		return;
	}

	QTextStream out(&file);
	out << toPlainText();
}

