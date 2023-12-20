/*
 * Copyright (c) 2023
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
 * Author: Zhaochen Zhang (zhaochen.zhang@outlook.com)
 */

#include "real-time-stats-tag.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RealTimeStatsTag");

NS_OBJECT_ENSURE_REGISTERED(RealTimeStatsTag);

TypeId
RealTimeStatsTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RealTimeStatsTag")
                            .SetParent<Tag>()
                            .SetGroupName("Dcb")
                            .AddConstructor<RealTimeStatsTag>();
    return tid;
}

TypeId
RealTimeStatsTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
RealTimeStatsTag::GetSerializedSize() const
{
    NS_LOG_FUNCTION(this);
    return 4 + 8 + 8;
}

void
RealTimeStatsTag::Serialize(TagBuffer buf) const
{
    NS_LOG_FUNCTION(this << &buf);
    buf.WriteU32(m_pktSeq);
    buf.WriteU64(m_tArriveNs);
    buf.WriteU64(m_tTxNs);
}

void
RealTimeStatsTag::Deserialize(TagBuffer buf)
{
    NS_LOG_FUNCTION(this << &buf);
    m_pktSeq = buf.ReadU32();
    m_tArriveNs = buf.ReadU64();
    m_tTxNs = buf.ReadU64();
}

void
RealTimeStatsTag::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    os << "PktSeq=" << m_pktSeq << ", TArriveNs=" << m_tArriveNs << ", TTxNs=" << m_tTxNs;
}

RealTimeStatsTag::RealTimeStatsTag()
    : Tag()
{
    NS_LOG_FUNCTION(this);
}

RealTimeStatsTag::RealTimeStatsTag(uint32_t pktSeq, uint64_t tArriveNs)
    : Tag(),
      m_pktSeq(pktSeq),
      m_tArriveNs(tArriveNs)
{
    NS_LOG_FUNCTION(this << pktSeq << tArriveNs);
}

uint32_t
RealTimeStatsTag::GetPktSeq() const
{
    NS_LOG_FUNCTION(this);
    return m_pktSeq;
}

void
RealTimeStatsTag::SetTArriveNs(uint64_t tArriveNs)
{
    NS_LOG_FUNCTION(this << tArriveNs);
    m_tArriveNs = tArriveNs;
}

uint64_t
RealTimeStatsTag::GetTArriveNs() const
{
    NS_LOG_FUNCTION(this);
    return m_tArriveNs;
}

void
RealTimeStatsTag::SetTTxNs(uint64_t tTxNs)
{
    NS_LOG_FUNCTION(this << tTxNs);
    m_tTxNs = tTxNs;
}

uint64_t
RealTimeStatsTag::GetTTxNs() const
{
    NS_LOG_FUNCTION(this);
    return m_tTxNs;
}

} // namespace ns3
