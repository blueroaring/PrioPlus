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

#include "rocev2-ledbat.h"

#include "rocev2-socket.h"

#include "ns3/global-value.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2Ledbat");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Ledbat);

TypeId
RoCEv2Ledbat::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2Ledbat")
                            .SetParent<RoCEv2CongestionOps>()
                            .AddConstructor<RoCEv2Ledbat>()
                            .SetGroupName("Dcb")
                            .AddAttribute("TargetDelay",
                                          "Targeted Queue Delay",
                                          TimeValue(MilliSeconds(100)),
                                          MakeTimeAccessor(&RoCEv2Ledbat::m_target),
                                          MakeTimeChecker())
                            .AddAttribute("NoiseFilterLen",
                                          "Number of Current delay samples",
                                          UintegerValue(4),
                                          MakeUintegerAccessor(&RoCEv2Ledbat::m_noiseFilterLen),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("Gain",
                                          "Offset Gain",
                                          DoubleValue(1.0),
                                          MakeDoubleAccessor(&RoCEv2Ledbat::m_gain),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("MinCwnd",
                                          "Minimum cWnd for Ledbat",
                                          UintegerValue(2),
                                          MakeUintegerAccessor(&RoCEv2Ledbat::m_minCwnd),
                                          MakeUintegerChecker<uint32_t>());
    return tid;
}

RoCEv2Ledbat::RoCEv2Ledbat()
    : RoCEv2CongestionOps()
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Ledbat::RoCEv2Ledbat(Ptr<RoCEv2SocketState> sockState)
    : RoCEv2CongestionOps(sockState)
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Ledbat::~RoCEv2Ledbat()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2Ledbat::Init()
{
    // m_lastRollover = Time(0);
    // m_flag = LEDBAT_CAN_SS;
    m_doSS = true;
    m_canDecrease = true;
    m_canDecreaseTimer = Timer();
    m_canDecreaseTimer.SetFunction(&RoCEv2Ledbat::SetCanDecrease, this);

    InitCircBuf(m_noiseFilter);

    // m_stats = std::make_shared<Stats>();
    RegisterCongestionType(GetTypeId());
}

std::shared_ptr<RoCEv2CongestionOps::Stats>
RoCEv2Ledbat::GetStats() const
{
    return m_stats;
}

void
RoCEv2Ledbat::SetReady()
{
    NS_LOG_FUNCTION(this);
    // SetCwnd(m_sockState->GetBaseBdp() * m_startRateRatio);
    SetCwnd(m_sockState->GetBaseBdp());
}

void
RoCEv2Ledbat::UpdateStateSend(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    // Get packet's PSN from roceheader.
    RoCEv2Header roceHeader;
    packet->PeekHeader(roceHeader);
    // Add current time to the map.
    m_tsMap[roceHeader.GetPSN()] = Simulator::Now();
}

void
RoCEv2Ledbat::UpdateStateWithCNP()
{
    // if (m_canDecrease)
    // {
    //     NS_LOG_FUNCTION(this);
    //     m_canDecrease = false;
    //     m_canDecreaseTimer.SetDelay(m_lastRtt);
    //     m_canDecreaseTimer.Schedule();
    //     SetCwnd(std::max(m_sockState->GetCwnd() / 2,
    //                      static_cast<uint64_t>(m_minCwnd * m_sockState->GetPacketSize())));
    // }
}

void
RoCEv2Ledbat::UpdateStateWithGenACK(Ptr<Packet> packet, Ptr<Packet> ack)
{
    NS_LOG_FUNCTION(this << packet << ack);
    // Add a LedbatHeader into ack, which record the ts of recv the data packet.
    // Note that the ACK has RoCEv2Header and AETHeader. And we should add HarvestHeader between
    // them.
    RoCEv2Header roceHeader;
    LedbatHeader ledbatHeader;
    ack->RemoveHeader(roceHeader);
    ack->AddHeader(ledbatHeader);
    ack->AddHeader(roceHeader);
}

void
RoCEv2Ledbat::UpdateStateWithRcvACK(Ptr<Packet> ack,
                                    const RoCEv2Header& roce,
                                    const uint32_t senderNextPSN)
{
    // Remove the LedbatHeader from ack
    LedbatHeader ledbatHeader;
    ack->RemoveHeader(ledbatHeader);
    // Calculate the delay from the ACK
    Time delay = ledbatHeader.GetTs() - m_tsMap[roce.GetPSN() - 1];
    m_lastRtt = Simulator::Now() - m_tsMap[roce.GetPSN() - 1];
    AddDelay(m_noiseFilter, delay, m_noiseFilterLen);

    if (m_doSS && delay < m_sockState->GetBaseOneWayDelay() + Time("1us"))
    {
        SlowStart();
    }
    else
    {
        m_doSS = false;
        CongestionAvoidance();
    }
}

void
RoCEv2Ledbat::SlowStart()
{
    CongestionAvoidance();
    // SetCwnd(m_sockState->GetCwnd() + m_sockState->GetPacketSize());
}

void
RoCEv2Ledbat::CongestionAvoidance()
{
    Time currentDelay = CurrentDelay(&RoCEv2Ledbat::MinCircBuf);
    Time queueDelay = currentDelay - m_sockState->GetBaseOneWayDelay();
    double offset = (m_target - queueDelay).GetSeconds() * m_gain / m_target.GetSeconds();

    // Do AI
    uint64_t totalPacketSize = m_sockState->GetPacketSize();
    double cwnd = m_sockState->GetCwnd(); // Bytes
    double cwndPackets = (cwnd + totalPacketSize - 1.0) / totalPacketSize;
    if (cwndPackets < 1)
    {
        cwndPackets = 1.0;
    }
    cwnd += offset * totalPacketSize / cwndPackets;
    cwnd = std::max(cwnd, static_cast<double>(m_minCwnd * totalPacketSize));
    SetCwnd(static_cast<uint64_t>(cwnd));
}

void
RoCEv2Ledbat::InitCircBuf(OwdCircBuf& buffer)
{
    NS_LOG_FUNCTION(this);
    buffer.buffer.clear();
    buffer.min = 0;
}

Time
RoCEv2Ledbat::MinCircBuf(OwdCircBuf& b)
{
    NS_LOG_FUNCTION_NOARGS();
    if (b.buffer.empty())
    {
        return Time(UINT64_MAX);
    }
    else
    {
        return b.buffer[b.min];
    }
}

Time
RoCEv2Ledbat::CurrentDelay(FilterFunction filter)
{
    NS_LOG_FUNCTION(this);
    return filter(m_noiseFilter);
}

void
RoCEv2Ledbat::AddDelay(OwdCircBuf& cb, Time owd, uint32_t maxlen)
{
    NS_LOG_FUNCTION(this << owd << maxlen << cb.buffer.size());
    if (cb.buffer.empty())
    {
        NS_LOG_LOGIC("First Value for queue");
        cb.buffer.push_back(owd);
        cb.min = 0;
        return;
    }
    cb.buffer.push_back(owd);
    if (cb.buffer[cb.min] > owd)
    {
        cb.min = static_cast<uint32_t>(cb.buffer.size() - 1);
    }
    if (cb.buffer.size() >= maxlen)
    {
        NS_LOG_LOGIC("Queue full" << maxlen);
        cb.buffer.erase(cb.buffer.begin());
        cb.min = 0;
        NS_LOG_LOGIC("Current min element" << cb.buffer[cb.min]);
        for (uint32_t i = 1; i < maxlen - 1; i++)
        {
            if (cb.buffer[i] < cb.buffer[cb.min])
            {
                cb.min = i;
            }
        }
    }
}

std::string
RoCEv2Ledbat::GetName() const
{
    return "RoCEv2Ledbat";
}

} // namespace ns3