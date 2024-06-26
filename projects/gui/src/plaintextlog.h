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

#ifndef PLAIN_TEXT_LOG_H
#define PLAIN_TEXT_LOG_H

#include <QPlainTextEdit>
#include <QString>

class QContextMenuEvent;
class QAction;
namespace Chess
{
	class Board;
}

/*!
 * \brief Widget that is used to display log messages in plain text.
 */
class PlainTextLog : public QPlainTextEdit
{
	Q_OBJECT

	public:
		/*! Constructs a new plain text log with the given \a parent. */
		PlainTextLog(QWidget* parent = nullptr);
		/*!
		 * Constructs a new plain text log with the initial text \a text and
		 * given \a parent.
		 */
		PlainTextLog(const QString& text, QWidget* parent = nullptr);

		void setOpening(const QString& opening);

	signals:
	    void applyMoves(std::shared_ptr<Chess::Board> board);

	public slots:
		/*! Save the log to file \a filename. */
		void saveLogToFile(const QString& fileName);

	protected:
	    void sendMoves();

	protected:
		// Inherited from QPlainTextEdit
	    virtual void contextMenuEvent(QContextMenuEvent* event) override;
	    virtual void mouseDoubleClickEvent(QMouseEvent* e) override;

	protected:
	    QString opening;
};

#endif // PLAIN_TEXT_LOG_H

