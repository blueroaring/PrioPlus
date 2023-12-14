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

#include "rocev2-timely.h"

#include "rocev2-socket.h"

#include "ns3/seq-ts-header.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2Timely");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Timely);

TypeId
RoCEv2Timely::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2Timely")
                            .SetParent<RoCEv2CongestionOps>()
                            .AddConstructor<RoCEv2Timely>()
                            .SetGroupName("Dcb")
                            .AddAttribute("Alpha",
                                          "Timely's alpha. EWMA weight parameter",
                                          DoubleValue(0.875),
                                          MakeDoubleAccessor(&RoCEv2Timely::m_alpha),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("Beta",
                                          "Timely's beta, the multiplicative decrement factor",
                                          DoubleValue(0.8),
                                          MakeDoubleAccessor(&RoCEv2Timely::m_mdFactor),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("RateAIRatio",
                                          "Timely's RateAI ratio (delta in paper)",
                                          DoubleValue(0.0005),
                                          MakeDoubleAccessor(&RoCEv2Timely::m_raiRatio),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("MinRateRatio",
                                          "Timely's minimum rate ratio",
                                          DoubleValue(1e-3),
                                          MakeDoubleAccessor(&RoCEv2Timely::m_minRateRatio),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("HAIMode",
                                          "Timely's HAI mode",
                                          BooleanValue(true),
                                          MakeBooleanAccessor(&RoCEv2Timely::m_haiMode),
                                          MakeBooleanChecker())
                            .AddAttribute("MaxStage",
                                          "Timely's maximum stage (N), used for HAI mode",
                                          UintegerValue(5),
                                          MakeUintegerAccessor(&RoCEv2Timely::m_maxStage),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("MinRTT",
                                          "Timely's minimum RTT",
                                          TimeValue(MilliSeconds(2)),
                                          MakeTimeAccessor(&RoCEv2Timely::m_minRtt),
                                          MakeTimeChecker())
                            .AddAttribute("Tlow",
                                          "Timely's Tlow",
                                          TimeValue(MilliSeconds(5)),
                                          MakeTimeAccessor(&RoCEv2Timely::m_tLow),
                                          MakeTimeChecker())
                            .AddAttribute("Thigh",
                                          "Timely's Thigh",
                                          TimeValue(MilliSeconds(50)),
                                          MakeTimeAccessor(&RoCEv2Timely::m_tHigh),
                                          MakeTimeChecker());
    return tid;
}

RoCEv2Timely::RoCEv2Timely()
    : RoCEv2CongestionOps()
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Timely::RoCEv2Timely(Ptr<RoCEv2SocketState> sockState)
    : RoCEv2CongestionOps(sockState)
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Timely::~RoCEv2Timely()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2Timely::SetReady()
{
    NS_LOG_FUNCTION(this);
    m_sockState->SetRateRatioPercent(m_curRateRatio);
}

void
RoCEv2Timely::UpdateStateSend(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Add a SeqTsHeader into packet, which will set timestamp automatically.
    SeqTsHeader seqTsHeader;
    packet->AddHeader(seqTsHeader);
}

void
RoCEv2Timely::UpdateStateWithGenACK(Ptr<Packet> packet, Ptr<Packet> ack)
{
    NS_LOG_FUNCTION(this << packet << ack);
    // Remove the SeqTsHeader from the packet.
    SeqTsHeader seqTsHeader;
    packet->RemoveHeader(seqTsHeader);
    // And then copy to the ACK
    // Note that the ACK has RoCEv2Header and AETHeader. And we should add SeqTsHeader between
    // them.
    RoCEv2Header roceHeader;
    ack->RemoveHeader(roceHeader);
    ack->AddHeader(seqTsHeader);
    ack->AddHeader(roceHeader);
}

void
RoCEv2Timely::UpdateStateWithRcvACK(Ptr<Packet> ack,
                                    const RoCEv2Header& roce,
                                    const uint32_t senderNextPSN)
{
    NS_LOG_FUNCTION(this << ack << roce);
    // Timely should remove the SeqTsHeader of the ACK.
    SeqTsHeader seqTsHeader;
    ack->RemoveHeader(seqTsHeader);
    Time newRtt = Simulator::Now() - seqTsHeader.GetTs();
    if (m_prevRtt < Time(0))
    {
        // first ACK, do nothing
    }
    else
    {
        Time newRttDiff = newRtt - m_prevRtt;
        // update m_rttDiff
        m_rttDiff = m_alpha * newRttDiff + (1 - m_alpha) * m_rttDiff;
        // m_rttDiff divided by m_minRtt to get normalized gradient
        double gradient = m_rttDiff.GetSeconds() / m_minRtt.GetSeconds();
        // if newRtt is less than Tlow, additive increment
        if (newRtt < m_tLow)
        {
            m_curRateRatio = CheckRateRatio(m_curRateRatio + m_raiRatio);
            return;
        }
        // if newRtt is greater than Thigh, multiplicative decrement
        else if (newRtt > m_tHigh)
        {
            m_curRateRatio = CheckRateRatio(
                m_curRateRatio * (1.0 - m_mdFactor * (m_tHigh.GetSeconds() / newRtt.GetSeconds())));
            return;
        }
        // if gradient is not positive, AI or HAI
        if (gradient <= 0)
        {
            if (m_haiMode)
                m_incStage++;
            if (m_incStage > m_maxStage)
            {
                m_curRateRatio = CheckRateRatio(m_curRateRatio + m_maxStage * m_raiRatio);
                m_incStage = m_maxStage;
            }
            else
            {
                // not HAI mode, or HAI mode but not reach max stage
                m_curRateRatio = CheckRateRatio(m_curRateRatio + m_raiRatio);
            }
        }
        else
        {
            m_incStage = 1;
            m_curRateRatio = CheckRateRatio(m_curRateRatio * (1.0 - m_mdFactor * gradient));
        }
    }
    // update m_prevRtt
    m_prevRtt = newRtt;
}

std::string
RoCEv2Timely::GetName() const
{
    return "Timely";
}

void
RoCEv2Timely::Init()
{
    SeqTsHeader hd;
    m_headerSize += hd.GetSerializedSize(); // SeqTsHeader+Rocev2+UDP+IP+Eth

    m_prevRtt = Seconds(-1.0);
    m_incStage = 1;
    m_curRateRatio = 1.;
}
} // namespace ns3
