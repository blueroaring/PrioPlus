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

#ifndef CSV_WRITER_H
#define CSV_WRITER_H

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ns3
{

class CsvWriter
{
  public:
    CsvWriter(const std::string& filepath, std::size_t columnNum, char delimiter = ',');
    CsvWriter(std::ostream* const stream, std::size_t columnNum, char delimiter = ',');
    void SetColumnCount(std::size_t columnNum);
    [[maybe_unused]] std::size_t GetColumnCount() const;
    [[maybe_unused]] char Delimiter() const;

    template <class T>
    void WriteNextValue(T value);

  private:
    std::ofstream m_fileStream;
    std::ostream* const m_stream;
    char m_delimiter;
    std::size_t m_currentColumn;
    std::size_t m_totalColumn;

}; // class CsvWriter

/****************************************************
 *      Template implementations.
 ***************************************************/

template <class T>
void
CsvWriter::WriteNextValue(T value)
{
    (*m_stream) << value;
    m_currentColumn = (m_currentColumn + 1) % m_totalColumn;
    if (m_currentColumn > 0)
    {
        (*m_stream) << m_delimiter;
    }
    else
    { // finish one row
        (*m_stream) << '\n';
    }
}

} // namespace ns3

#endif // CSV_WRITER_H
