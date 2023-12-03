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
#include "hpcc-header.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(HpccHeader);

HpccHeader::HpccHeader()
    : m_infoBuf(0)
{
    std::memset(m_intHops, 0, sizeof(m_intHops));
}

void
HpccHeader::PushHop(uint64_t time, uint64_t bytes, uint64_t qlen, DataRate rate)
{
    if (m_nHop >= MAX_HOP)
    {
        NS_FATAL_ERROR("Too many hops");
    }
    m_intHops[m_nHop].Set(time, bytes, qlen, rate);
    m_nHop++;
}

TypeId
HpccHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::HpccHeader").SetParent<Header>().AddConstructor<HpccHeader>();
    return tid;
}

TypeId
HpccHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
HpccHeader::GetSerializedSize() const
{
    constexpr uint32_t sz = sizeof(m_infoBuf) + sizeof(m_intHops);
    return sz;
}

void
HpccHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU16(m_infoBuf);
    start.Write(reinterpret_cast<const uint8_t*>(m_intHops), sizeof(m_intHops));
}

uint32_t
HpccHeader::Deserialize(Buffer::Iterator start)
{
    m_infoBuf = start.ReadNtohU16();
    start.Read(reinterpret_cast<uint8_t*>(m_intHops), sizeof(m_intHops));
    return GetSerializedSize();
}

void
HpccHeader::Print(std::ostream& os) const
{
    os << "HPCC header: nHop=" << m_nHop << " pathID=" << m_pathID << std::endl;
    for (uint16_t i = 0; i < m_nHop; i++)
    {
        os << "====Hop " << i << "====" << std::endl;
        os << "lineRate=" << m_intHops[i].GetLineRate() << "timeSlot=" << m_intHops[i].GetTimeSlot()
           << "txBytes=" << m_intHops[i].GetBytes() << std::endl;
    }
}

} // namespace ns3