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
 * Author: F.Y. Xue <xue.fyang@foxmail.com>
 */

#include "rocev2-d2tcp.h"

#include "rocev2-socket.h"

#include "ns3/global-value.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2D2tcp");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2D2tcp);

TypeId
RoCEv2D2tcp::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2D2tcp")
                            .SetParent<RoCEv2Dctcp>()
                            .AddConstructor<RoCEv2D2tcp>()
                            .SetGroupName("Dcb")
                            .AddAttribute("PriorityFactor",
                                          "D2TCP's priority factor, means (Deadline / oracle FCT)",
                                          DoubleValue(2.0),
                                          MakeDoubleAccessor(&RoCEv2D2tcp::m_priorityFactor),
                                          MakeDoubleChecker<double>());
    return tid;
}

RoCEv2D2tcp::RoCEv2D2tcp()
    : RoCEv2Dctcp(),
      m_stats(std::dynamic_pointer_cast<Stats>(RoCEv2Dctcp::m_stats))
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2D2tcp::RoCEv2D2tcp(Ptr<RoCEv2SocketState> sockState)
    : RoCEv2Dctcp(sockState),
      m_stats(std::dynamic_pointer_cast<Stats>(RoCEv2Dctcp::m_stats))
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2D2tcp::~RoCEv2D2tcp()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2D2tcp::Init()
{
    m_alphaLastUpdateSeq = 0;
    m_cwndLastReduceSeq = 0;
    m_alpha = 1.0;
    m_ecnCnt = 0;
    m_allCnt = 0;
    RegisterCongestionType(GetTypeId());
}

void
RoCEv2D2tcp::SetReady()
{
    NS_LOG_FUNCTION(this);
    RoCEv2Dctcp::SetReady();

    // Calculate deadline
    // Time oracleFct = m_sockState->GetBaseRtt() +
    //                  m_sockState->GetDeviceRate()->CalculateBytesTxTime(
    //                      m_sockState->GetTxBuffer()->RemainSizeInPacket() *
    //                      m_sockState->GetMss());
    // XXX Since SetReady() os called before Socket::Send(), DcbTxBuffer::RemainSizeInPacket() can
    // not be used here. Therefore, we just record the start time of the flow
    m_startTime = Simulator::Now();
    m_deadline = Time(0);
}

void
RoCEv2D2tcp::ReduceWindow()
{
    if (m_deadline == Time(0))
    {
        // update deadline here
        Time oracleFct =
            m_sockState->GetBaseRtt() +
            m_sockState->GetDeviceRate()->CalculateBytesTxTime(m_sockState->GetFlowTotalSize());
        m_deadline = m_startTime + m_priorityFactor * oracleFct;
        // std::clog << "priorityFactor: " << m_priorityFactor << " "
        //           << "oracleFct: " << oracleFct.GetSeconds() << " "
        //           << "startTime: " << m_startTime.GetSeconds() << " "
        //           << "deadline: " << m_deadline.GetSeconds() << std::endl;
    }
    Time timeNeeded = (m_sockState->GetTxBuffer()->RemainSizeInPacket() * m_sockState->GetMss() /
                       (0.75 * m_sockState->GetCwnd())) *
                      m_sockState->GetBaseRtt(); // B/(0.75 W/RTT)
    Time timeRemain = m_deadline > Simulator::Now() ? m_deadline - Simulator::Now() : Time(0);
    double d;
    if (timeRemain == Time(0))
    {
        NS_LOG_DEBUG("Deadline is expired, d = 2");
        d = 2.0;
    }
    else
    {
        d = timeNeeded.GetSeconds() / timeRemain.GetSeconds();
    }
    // d should be in [0.5, 2]
    d = std::max(0.5, std::min(2.0, d));
    // std::clog << "priorityFactor: " << m_priorityFactor << " "
    //           << "timeNeeded: " << timeNeeded.GetSeconds() << " "
    //           << "timeRemain: " << timeRemain.GetSeconds() << " "
    //           << "d: " << d << std::endl;

    SetCwnd(m_sockState->GetCwnd() * (1 - std::pow(m_alpha, d) / 2));
}

std::string
RoCEv2D2tcp::GetName() const
{
    return "RoCEv2D2tcp";
}

} // namespace ns3