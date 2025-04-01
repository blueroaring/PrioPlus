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
 * Author: F.Y. Xue <xue.fyang@foxmail.com>
 */
#include "poseidon-header.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(PoseidonHeader);

PoseidonHeader::PoseidonHeader()
    : m_mpd(0)
{
}

TypeId
PoseidonHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::PoseidonHeader").SetParent<Header>().AddConstructor<PoseidonHeader>();
    return tid;
}

TypeId
PoseidonHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
PoseidonHeader::GetSerializedSize() const
{
    return sizeof(m_mpd) + sizeof(m_reserved);
}

void
PoseidonHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU16(m_mpd);
    for (int i = 0; i < 5; i++)
    {
        start.WriteHtonU16(m_reserved[i]);
    }
}

uint32_t
PoseidonHeader::Deserialize(Buffer::Iterator start)
{
    m_mpd = start.ReadNtohU16();
    for (int i = 0; i < 5; i++)
    {
        m_reserved[i] = start.ReadNtohU16();
    }
    return GetSerializedSize();
}

void
PoseidonHeader::Print(std::ostream& os) const
{
    os << "PoseidonHeader: mpd=" << m_mpd << std::endl;
}

void
PoseidonHeader::UpdateMpd(uint16_t mpd)
{
    m_mpd = std::max(m_mpd, mpd);
}

} // namespace ns3
