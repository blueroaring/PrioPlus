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
#include "rocev2-congestion-ops.h"

#include "rocev2-socket.h"

#include "ns3/rocev2-header.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2CongestionOps");
NS_OBJECT_ENSURE_REGISTERED(RoCEv2CongestionOps);

TypeId
RoCEv2CongestionOps::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::RoCEv2CongestionOps").SetParent<Object>().SetGroupName("Dcb");
    return tid;
}

RoCEv2CongestionOps::RoCEv2CongestionOps()
    : m_headerSize(12 + 8 + 20 + 14) // Rocev2+UDP+IP+Eth
{
    NS_LOG_FUNCTION(this);
}

RoCEv2CongestionOps::RoCEv2CongestionOps(Ptr<RoCEv2SocketState> sockState)
    : m_sockState(sockState),
      m_headerSize(12 + 8 + 20 + 14) // Rocev2+UDP+IP+Eth
{
    NS_LOG_FUNCTION(this);
}

RoCEv2CongestionOps::~RoCEv2CongestionOps()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2CongestionOps::SetStopTime(Time stopTime)
{
    m_stopTime = stopTime;
}

void
RoCEv2CongestionOps::SetSockState(Ptr<RoCEv2SocketState> sockState)
{
    m_sockState = sockState;
}

bool
RoCEv2CongestionOps::CheckStopCondition()
{
    return m_stopTime.GetNanoSeconds() == 0 || Simulator::Now() < m_stopTime;
}
} // namespace ns3
