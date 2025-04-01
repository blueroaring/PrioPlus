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

#include "rocev2-dctcp.h"

#include "rocev2-socket.h"

#include "ns3/global-value.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2Dctcp");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Dctcp);

TypeId
RoCEv2Dctcp::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2Dctcp")
                            .SetParent<RoCEv2CongestionOps>()
                            .AddConstructor<RoCEv2Dctcp>()
                            .SetGroupName("Dcb")
                            .AddAttribute("Gain",
                                          "DCTCP's gain",
                                          DoubleValue(1.0),
                                          MakeDoubleAccessor(&RoCEv2Dctcp::m_gain),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("RateAIRatio",
                                          "DCTCP's RateAI ratio.",
                                          DoubleValue(0.0005),
                                          MakeDoubleAccessor(&RoCEv2Dctcp::m_raiRatio),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("StartPattern",
                                          "The start pattern. SLOW_START or FIXED_START",
                                          EnumValue(RoCEv2Dctcp::StartPattern::SLOW_START),
                                          MakeEnumAccessor(&RoCEv2Dctcp::m_startPattern),
                                          MakeEnumChecker(RoCEv2Dctcp::StartPattern::SLOW_START,
                                                          "SlowStart",
                                                          RoCEv2Dctcp::StartPattern::FIXED_START,
                                                          "FixedStart"));
    return tid;
}

RoCEv2Dctcp::RoCEv2Dctcp()
    : RoCEv2CongestionOps(std::make_shared<Stats>()),
      m_stats(std::dynamic_pointer_cast<Stats>(RoCEv2CongestionOps::m_stats))
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Dctcp::RoCEv2Dctcp(Ptr<RoCEv2SocketState> sockState)
    : RoCEv2CongestionOps(sockState, std::make_shared<Stats>()),
      m_stats(std::dynamic_pointer_cast<Stats>(RoCEv2CongestionOps::m_stats))
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Dctcp::~RoCEv2Dctcp()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2Dctcp::Init()
{
    m_alphaLastUpdateSeq = 0;
    m_cwndLastReduceSeq = 0;
    m_alpha = 1.0;
    m_ecnCnt = 0;
    m_allCnt = 0;
    RegisterCongestionType(GetTypeId());
}

void
RoCEv2Dctcp::SetReady()
{
    NS_LOG_FUNCTION(this);
    if (m_startPattern == StartPattern::SLOW_START)
    {
        SetCwnd(m_sockState->GetPacketSize());
        m_doSS = true;
    }
    else
    {
        SetCwnd(m_sockState->GetBaseBdp() * m_startRateRatio);
        m_doSS = false;
    }
    m_cwndAi = m_sockState->GetBaseBdp() * m_raiRatio; // unit: bytes
}

void
RoCEv2Dctcp::UpdateStateWithRcvACK(Ptr<Packet> ack,
                                   const RoCEv2Header& roce,
                                   const uint32_t senderNextPSN)
{
    m_ecnCnt += m_sockState->m_receivedEcn ? 1 : 0;
    m_allCnt += 1;

    if (m_doSS)
    {
        // if received ECN or cwnd is BDP, quit slow start
        if (m_sockState->m_receivedEcn || m_sockState->GetCwnd() >= m_sockState->GetBaseBdp())
        {
            m_doSS = false;
            CongestionAvoidance(roce.GetPSN(), senderNextPSN);
            return;
        }
        SlowStart();
    }
    else
    {
        CongestionAvoidance(roce.GetPSN(), senderNextPSN);
    }
}

void
RoCEv2Dctcp::SlowStart()
{
    NS_LOG_FUNCTION(this);
    SetCwnd(m_sockState->GetCwnd() + m_sockState->GetPacketSize());
}

void
RoCEv2Dctcp::CongestionAvoidance(const uint32_t ackSeq, const uint32_t senderNextPSN)
{
    NS_LOG_FUNCTION(this);
    if (ackSeq > m_alphaLastUpdateSeq)
    {
        if (m_alphaLastUpdateSeq != 0)
        {
            double ecnRatio = m_ecnCnt / m_allCnt;
            UpdateAlpha(ecnRatio);
            m_ecnCnt = 0;
            m_allCnt = 0;
        }
        m_alphaLastUpdateSeq = senderNextPSN;
    }

    if (ackSeq > m_cwndLastReduceSeq)
    {
        if (m_sockState->m_receivedEcn)
        {
            ReduceWindow();
            m_cwndLastReduceSeq =
                senderNextPSN; // Only after about 1 RTT, the cwnd will be changed.
        }
        else
        {
            // Do AI, cwndPackets is cwnd in packets
            uint64_t totalPacketSize = m_sockState->GetPacketSize();
            uint64_t cwndPackets =
                ((static_cast<double>(m_sockState->GetCwnd()) + totalPacketSize - 1.0) /
                 totalPacketSize);
            SetCwnd(m_sockState->GetCwnd() + (m_cwndAi + cwndPackets - 1) / cwndPackets);
        }
    }
}

void
RoCEv2Dctcp::UpdateAlpha(double ecnRatio)
{
    m_alpha = m_alpha * (1.0 - m_gain) + m_gain * ecnRatio;
}

void
RoCEv2Dctcp::ReduceWindow()
{
    SetCwnd(m_sockState->GetCwnd() * (1 - m_alpha / 2));
}

std::string
RoCEv2Dctcp::GetName() const
{
    return "RoCEv2Dctcp";
}

} // namespace ns3