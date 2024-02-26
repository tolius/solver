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

#include "polyglotbook.h"
#include <QDataStream>


PolyglotBook::PolyglotBook(AccessMode mode)
	: OpeningBook(mode)
{
}

int PolyglotBook::entrySize() const
{
	return 16;
}

OpeningBook::Entry PolyglotBook::readEntry(QDataStream& in, quint64* key) const
{
	quint16 pgMove;
	quint16 weight;
	quint32 learn;
	
	// Read the data. No need to worry about endianess,
	// because QDataStream uses big-endian by default.
	in >> *key >> pgMove >> weight >> learn;
	
	return { pgMove, weight, learn };
}

void PolyglotBook::writeEntry(const Map::const_iterator& it,
			      QDataStream& out) const
{
	quint64 key = it.key();
	auto& val = it.value();
	quint16 pgMove = val.pgMove;
	quint16 weight = val.weight;
	quint32 learn = val.learn;
	
	// Store the data. Again, big-endian is used by default.
	out << key << pgMove << weight << learn;
}
