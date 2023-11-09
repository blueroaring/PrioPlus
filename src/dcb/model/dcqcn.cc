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
 * Author: Pavinberg <pavin0702@gmail.com>
 */

#include "dcqcn.h"

#include "rocev2-socket.h"

#include "ns3/rocev2-header.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcqcnCongestionOps");

NS_OBJECT_ENSURE_REGISTERED(DcqcnCongestionOps);

TypeId
DcqcnCongestionOps::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DcqcnCongestionOps").SetParent<Object>().SetGroupName("Dcb");
    return tid;
}

DcqcnCongestionOps::DcqcnCongestionOps(Ptr<RoCEv2SocketState> sockState)
    : m_sockState(sockState),
      m_alpha(1.),     // larger alpha means more aggressive rate reduction
      m_g(0.00390625), // 1 / 16.
      m_raiRatio(.5),
      m_hraiRatio(1.),
      m_alphaTimer(Timer::CANCEL_ON_DESTROY),
      m_bytesThreshold(150 * 1024),
      m_bytesCounter(0),
      m_rateUpdateIter(0),
      m_bytesUpdateIter(0),
      m_F(5),
      m_targetRateRatio(100.),
      m_curRateRatio(100.),
      m_CNPInterval(MicroSeconds(4)),
      m_minRateRatio(1e-3)
{
    NS_LOG_FUNCTION(this);
    m_alphaTimer.SetFunction(&DcqcnCongestionOps::UpdateAlpha, this);
    m_alphaTimer.SetDelay(MicroSeconds(1)); // 55
    m_rateTimer.SetFunction(&DcqcnCongestionOps::RateTimerTriggered, this);
    m_rateTimer.SetDelay(MicroSeconds(20)); // 1500
}

DcqcnCongestionOps::~DcqcnCongestionOps()
{
    NS_LOG_FUNCTION(this);
}

void
DcqcnCongestionOps::SetReady()
{
    m_rateTimer.Schedule();
}

void
DcqcnCongestionOps::UpdateStateSend(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    m_bytesCounter += packet->GetSize();
    if (m_bytesCounter >= m_bytesThreshold)
    {
        m_bytesCounter = 0;
        m_bytesUpdateIter++;
        UpdateRate();
    }
}

void
DcqcnCongestionOps::UpdateStateWithCNP()
{
    NS_LOG_FUNCTION(this);

    m_targetRateRatio = m_curRateRatio;
    m_curRateRatio *= 1 - m_alpha / 2;
    m_curRateRatio = std::max(m_curRateRatio, m_minRateRatio);
    m_sockState->SetRateRatioPercent(m_curRateRatio);
    m_alpha = (1 - m_g) * m_alpha + m_g;

    m_alphaTimer.Cancel(); // re-schedule timer
    if (m_stopTime.GetNanoSeconds() == 0 || Simulator::Now() < m_stopTime)
    {
        m_alphaTimer.Schedule();
        m_rateTimer.Cancel(); // re-schedule timer
        m_rateTimer.Schedule();
    }
    m_bytesCounter = 0;
    m_rateUpdateIter = 0;
    m_bytesUpdateIter = 0;
}

void
DcqcnCongestionOps::UpdateAlpha()
{
    NS_LOG_FUNCTION(this);

    m_alpha *= 1 - m_g;
    if (m_stopTime.GetNanoSeconds() == 0 || Simulator::Now() < m_stopTime)
    {
        m_alphaTimer.Schedule();
    }
}

void
DcqcnCongestionOps::RateTimerTriggered()
{
    NS_LOG_FUNCTION(this);

    m_rateUpdateIter++;
    UpdateRate();
    if (m_stopTime.GetNanoSeconds() == 0 || Simulator::Now() < m_stopTime)
    {
        m_rateTimer.Schedule();
    }
}

void
DcqcnCongestionOps::UpdateRate()
{
    NS_LOG_FUNCTION(this);

    double old = m_curRateRatio;
    if (m_rateUpdateIter > m_F && m_bytesUpdateIter > m_F)
    { // Hyper increase
        uint32_t i = std::min(m_rateUpdateIter, m_bytesUpdateIter) - m_F + 1;
        m_targetRateRatio = std::min(m_targetRateRatio + i * m_hraiRatio, 100.);
    }
    else if (m_rateUpdateIter > m_F || m_bytesUpdateIter > m_F)
    { // Additive increase
        m_targetRateRatio = std::min(m_targetRateRatio + m_raiRatio, 100.);
    }
    // else m_rateUpdateIter < m_F && m_bytesUpdateIter < m_F
    // Fast recovery: don't need to update target rate

    m_curRateRatio = (m_targetRateRatio + m_curRateRatio) / 2;
    m_sockState->SetRateRatioPercent(m_curRateRatio);
    if (old < 100.)
    {
        NS_LOG_DEBUG("DCQCN: Rate update from " << old << "% to " << m_curRateRatio << "% at time "
                                                << Simulator::Now().GetMicroSeconds() << "us");
    }
}

void
DcqcnCongestionOps::SetRateAIRatio(double ratio)
{
    m_raiRatio = ratio * 100.;
}

void
DcqcnCongestionOps::SetRateHyperAIRatio(double ratio)
{
    m_hraiRatio = ratio * 100.;
}

void
DcqcnCongestionOps::SetStopTime(Time stopTime)
{
    m_stopTime = stopTime;
}

Time
DcqcnCongestionOps::GetCNPInterval() const
{
    return m_CNPInterval;
}

} // namespace ns3
