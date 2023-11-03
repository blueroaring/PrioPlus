/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Pavinberg <pavin0702@gmail.com>
 */

#include "csv-writer.h"
#include "ns3/log.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("CsvWriter");

CsvWriter::CsvWriter (const std::string &filepath, std::size_t columnNum, char delimiter /* =',' */)
    : m_fileStream (filepath),
      m_stream (&m_fileStream),
      m_delimiter (delimiter),
      m_currentColumn (0),
      m_totalColumn (columnNum)
{
  NS_LOG_FUNCTION (this << filepath);
}

CsvWriter::CsvWriter (std::ostream* const stream, std::size_t columnNum, char delimiter /* =',' */)
    : m_fileStream (),
      m_stream (stream),
      m_delimiter (delimiter),
      m_currentColumn (0),
      m_totalColumn (columnNum)
{
  NS_LOG_FUNCTION (this);
}

void
CsvWriter::SetColumnCount (std::size_t columnNum)
{
  NS_LOG_FUNCTION (this << columnNum);

  m_totalColumn = columnNum;
}

std::size_t
CsvWriter::GetColumnCount () const
{
  NS_LOG_FUNCTION (this);

  return m_totalColumn;
}

char
CsvWriter::Delimiter () const
{
  NS_LOG_FUNCTION (this);
  return m_delimiter;
}
} // namespace ns3
