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

#include "rocev2-swift.h"

#include "rocev2-socket.h"

#include "ns3/global-value.h"
#include "ns3/seq-ts-header.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2Swift");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Swift);

TypeId
RoCEv2Swift::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RoCEv2Swift")
            .SetParent<RoCEv2CongestionOps>()
            .AddConstructor<RoCEv2Swift>()
            .SetGroupName("Dcb")
            .AddAttribute("RateAIRatio",
                          "Swift's RateAI ratio (delta in paper)",
                          DoubleValue(0.005),
                          MakeDoubleAccessor(&RoCEv2Swift::m_raiRatio),
                          MakeDoubleChecker<double>())
            .AddAttribute("Beta",
                          "Swift's beta, the multiplicative decrement factor",
                          DoubleValue(0.8),
                          MakeDoubleAccessor(&RoCEv2Swift::m_mdFactor),
                          MakeDoubleChecker<double>())
            .AddAttribute("MaxMdf",
                          "Swift's max md factor.",
                          DoubleValue(0.4),
                          MakeDoubleAccessor(&RoCEv2Swift::m_maxMdFactor),
                          MakeDoubleChecker<double>())
            .AddAttribute("Range",
                          "Swift's fs_range, according to paper's Figure 5, it should be 4.0",
                          DoubleValue(4.0),
                          MakeDoubleAccessor(&RoCEv2Swift::m_range),
                          MakeDoubleChecker<double>())
            .AddAttribute("BaseTarget",
                          "Swift's base target delay",
                          TimeValue(MicroSeconds(25)),
                          MakeTimeAccessor(&RoCEv2Swift::m_baseTarget),
                          MakeTimeChecker());
    return tid;
}

RoCEv2Swift::RoCEv2Swift()
    : RoCEv2CongestionOps()
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Swift::RoCEv2Swift(Ptr<RoCEv2SocketState> sockState)
    : RoCEv2CongestionOps(sockState)
{
    NS_LOG_FUNCTION(this);
    Init();
}

RoCEv2Swift::~RoCEv2Swift()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2Swift::SetReady()
{
    NS_LOG_FUNCTION(this);
    // Reload any config before starting
    SetRateRatio(1.);
    m_gamma =
        1. / (1. / std::sqrt(m_sockState->GetMinRateRatio()) - 1.); // Used in GetTargetDelay()
}

void
RoCEv2Swift::UpdateStateSend(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Get packet's PSN from roceheader.
    RoCEv2Header roceHeader;
    packet->PeekHeader(roceHeader);
    // Add current time to the map.
    m_tsMap[roceHeader.GetPSN()] = Simulator::Now();
}

void
RoCEv2Swift::UpdateStateWithRcvACK(Ptr<Packet> ack,
                                   const RoCEv2Header& roce,
                                   const uint32_t senderNextPSN)
{
    NS_LOG_FUNCTION(this << ack << roce);

    // Read the ACK's PSN and get the corresponding timeslot.
    Time delay = Simulator::Now() - m_tsMap[roce.GetPSN() - 1];
    Time targetDelay = GetTargetDelay();

    uint64_t totalPacketSize = m_sockState->GetPacketSize();
    if (delay < targetDelay)
    {
        // Do AI, cwndPackets is cwnd in packets

        double cwndPackets;
        if (m_isLimiting)
        {
            cwndPackets = ((static_cast<double>(m_sockState->GetCwnd()) + totalPacketSize - 1.0) /
                           totalPacketSize);
        }
        else
        {
            cwndPackets = (m_sockState->GetRateRatioPercent() * m_sockState->GetBaseBdp() +
                           totalPacketSize - 1.0) /
                          totalPacketSize;
        }
        if (cwndPackets < 1)
        {
            cwndPackets = 1.0;
        }
        SetRateRatio(m_sockState->GetRateRatioPercent() + m_raiRatio / cwndPackets);

        m_stats->RecordCcRateChange(true);
    }
    else if (m_canDecrease)
    {
        // Do MD
        double factor =
            std::min(m_mdFactor * (delay.GetNanoSeconds() - targetDelay.GetNanoSeconds()) /
                         delay.GetNanoSeconds(),
                     m_maxMdFactor);
        SetRateRatio(m_sockState->GetRateRatioPercent() * (1.0 - factor));
        // Schedule a timer to set m_canDecrease to true.
        m_canDecrease = false;
        m_canDecreaseTimer.SetDelay(delay);
        m_canDecreaseTimer.Schedule();

        m_stats->RecordCcRateChange(false);
    }
}

Time
RoCEv2Swift::GetTargetDelay()
{
    /**
     * The formula in paper's **Overall Scaling** section can be simplified as:
     * t = base_target + base_rtt + max(0, min(range, range * x))
     * where x = (1/√cur - 1/√max) / (1/√min - 1/√max).
     * ($ \frac{\frac{1}{\sqrt{cur}} - \frac{1}{\sqrt{max}}}
     * {\frac{1}{\sqrt{min}} - \frac{1}{\sqrt{max}}} $)
     *
     * We use γ = 1/(1/√min - 1/√max) to simplify the formula as x = γ * (1/√cur - 1/√max).
     */
    Time t = m_baseTarget + m_sockState->GetBaseRtt();
    double range = std::max(
        0.0,
        std::min(m_range,
                 m_range * m_gamma * (1.0 / std::sqrt(m_sockState->GetRateRatioPercent()) - 1.0)));
    t += range * m_baseTarget;
    // LOG TargetDelay and RateRatio and range
    NS_LOG_DEBUG("TargetDelay: " << t << ", RateRatio: " << m_sockState->GetRateRatioPercent()
                                 << ", range: " << range);

    return t;
}

std::string
RoCEv2Swift::GetName() const
{
    return "Swift";
}

void
RoCEv2Swift::Init()
{
    NS_LOG_FUNCTION(this);
    // Set Timer
    m_canDecreaseTimer = Timer();
    m_canDecreaseTimer.SetFunction(&RoCEv2Swift::SetCanDecrease, this);
    m_canDecrease = true;
    SetLimiting(true);

    RegisterCongestionType(GetTypeId());

    m_stats = std::make_shared<Stats>();
}

RoCEv2Swift::Stats::Stats()
{
    NS_LOG_FUNCTION(this);
    BooleanValue bv;
    if (GlobalValue::GetValueByNameFailSafe("detailedSenderStats", bv))
        bDetailedSenderStats = bv.Get();
    else
        bDetailedSenderStats = false;
}

void
RoCEv2Swift::Stats::RecordCcRateChange(bool increase)
{
    if (bDetailedSenderStats)
    {
        vCcRateChange.push_back(std::make_pair(Simulator::Now(), increase));
    }
}
} // namespace ns3
