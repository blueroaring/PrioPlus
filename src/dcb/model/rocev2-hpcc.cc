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

#include "rocev2-hpcc.h"

#include "rocev2-socket.h"

#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2Hpcc");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Hpcc);

TypeId
RoCEv2Hpcc::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2Hpcc")
                            .SetParent<RoCEv2CongestionOps>()
                            .AddConstructor<RoCEv2Hpcc>()
                            .SetGroupName("Dcb")
                            .AddAttribute("TargetUtil",
                                          "HPCC's target utilization",
                                          DoubleValue(0.95),
                                          MakeDoubleAccessor(&RoCEv2Hpcc::m_targetUtil),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("MinRateRatio",
                                          "HPCC's minimum rate ratio",
                                          DoubleValue(1e-3),
                                          MakeDoubleAccessor(&RoCEv2Hpcc::m_minRateRatio),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("MaxStage",
                                          "HPCC's maximum stage",
                                          UintegerValue(5),
                                          MakeUintegerAccessor(&RoCEv2Hpcc::m_maxStage),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("RateAIRatio",
                                          "HPCC's RateAI ratio",
                                          DoubleValue(0.0005),
                                          MakeDoubleAccessor(&RoCEv2Hpcc::m_raiRatio),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("BaseRTT",
                                          "HPCC's base RTT",
                                          TimeValue(MilliSeconds(100)),
                                          MakeTimeAccessor(&RoCEv2Hpcc::m_baseRtt),
                                          MakeTimeChecker());
    return tid;
}

RoCEv2Hpcc::RoCEv2Hpcc()
    : RoCEv2CongestionOps()
//   m_alphaTimer(Timer::CANCEL_ON_DESTROY)
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Hpcc::RoCEv2Hpcc(Ptr<RoCEv2SocketState> sockState)
    : RoCEv2CongestionOps(sockState)
//   m_alphaTimer(Timer::CANCEL_ON_DESTROY)
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Hpcc::~RoCEv2Hpcc()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2Hpcc::SetReady()
{
    NS_LOG_FUNCTION(this);
    m_sockState->SetRateRatioPercent(m_curRateRatio);
}

void
RoCEv2Hpcc::UpdateStateSend(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Add an empty HPCC header into packet
    HpccHeader hpccHeader;
    packet->AddHeader(hpccHeader);
}

void
RoCEv2Hpcc::UpdateStateWithGenACK(Ptr<Packet> packet, Ptr<Packet> ack)
{
    NS_LOG_FUNCTION(this << packet << ack);
    // HPCC should remove the HPCC header of the packet.
    HpccHeader hpccHeader;
    packet->RemoveHeader(hpccHeader);
    // And then copy to the ACK.
    // Note that the ACK has RoCEv2Header and AETHeader. And we should add HPCC header between them.
    RoCEv2Header roceHeader;
    ack->RemoveHeader(roceHeader);
    ack->AddHeader(hpccHeader);
    ack->AddHeader(roceHeader);
}

void
RoCEv2Hpcc::UpdateStateWithRcvACK(Ptr<Packet> ack,
                                  const RoCEv2Header& roce,
                                  const uint32_t senderNextPSN)
{
    NS_LOG_FUNCTION(this << ack << roce);
    // HPCC should remove the HPCC header of the ACK.
    HpccHeader hpccHeader;
    ack->RemoveHeader(hpccHeader);
    const IntHop* inthop = hpccHeader.m_intHops;
    uint32_t nhop = hpccHeader.m_nHop;

    uint32_t ackSeq = roce.GetPSN();

    if (m_lastUpdateSeq != 0)
    {
        // not first ACK
        MeasureInflight(hpccHeader);
        bool updateCurrent = ackSeq > m_lastUpdateSeq;
        UpdateRate(updateCurrent);
        if (updateCurrent)
        {
            m_lastUpdateSeq = senderNextPSN;
        }
    }
    else
    {
        // first ACK
        m_lastUpdateSeq = senderNextPSN;
    }
    CopyIntHop(inthop, nhop);
}

void
RoCEv2Hpcc::MeasureInflight(const HpccHeader& hpccHeader)
{
    double u = 0;
    Time tau;
    for (uint32_t i = 0; i < hpccHeader.m_nHop; i++) // Algorithm Line 3
    {
        Time tauPrime = Time(hpccHeader.m_intHops[i].GetTimeDelta(m_hops[i]));
        double txRate = (hpccHeader.m_intHops[i].GetBytesDelta(m_hops[i]) * 8.0) /
                        tauPrime.GetSeconds(); // Algorithm Line 4
        double uPrime = txRate / hpccHeader.m_intHops[i].GetLineRate().GetBitRate();
        uPrime += std::min(hpccHeader.m_intHops[i].GetQlen(), m_hops[i].GetQlen()) * 8.0 /
                  m_baseRtt.GetSeconds() /
                  hpccHeader.m_intHops[i].GetLineRate().GetBitRate(); // Algorithm Line 5

        if (uPrime > u) // Algorithm Line 6
        {
            u = uPrime;
            tau = tauPrime; // Algorithm Line 7
        }
    }
    if (tau > m_baseRtt) // Algorithm Line 8
    {
        tau = m_baseRtt;
    }
    double frab = tau.GetSeconds() / m_baseRtt.GetSeconds();
    m_u = m_u * (1.0 - frab) + u * frab; // Algorithm Line 9
}

void
RoCEv2Hpcc::UpdateRate(bool updateCurrent)
{
    double uNormal = m_u / m_targetUtil;
    double newRateRatio;
    uint32_t newIncStage;
    if (uNormal >= 1.0 || m_incStage >= m_maxStage) // Algorithm Line 12
    {
        newRateRatio = m_curRateRatio / uNormal + m_raiRatio; // Algorithm Line 13
        newIncStage = 0;
    }
    else
    {
        newRateRatio = m_curRateRatio + m_raiRatio; // Algorithm Line 17
        newIncStage = m_incStage + 1;
    }
    // newRateRatio should between m_minRateRatio and 100%
    newRateRatio = CheckRateRatio(newRateRatio);
    // Update current rate and incStage if needed
    if (updateCurrent)
    {
        m_curRateRatio = newRateRatio;
        m_incStage = newIncStage;
    }
    // Update the rate ratio of the socket state
    m_sockState->SetRateRatioPercent(newRateRatio);
    // TODO： update window size
}

void
RoCEv2Hpcc::CopyIntHop(const IntHop* src, uint32_t nhop)
{
    for (uint32_t i = 0; i < nhop; i++)
    {
        m_hops[i] = src[i];
    }
}

std::string
RoCEv2Hpcc::GetName() const
{
    return "HPCC";
}

void
RoCEv2Hpcc::Init()
{
    HpccHeader hd;
    m_headerSize += hd.GetSerializedSize(); // HPCC+Rocev2+UDP+IP+Eth

    m_lastUpdateSeq = 0;
    m_incStage = 0;
    m_curRateRatio = 1.;
    m_u = 1.;
}
} // namespace ns3
