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

#include "dcb-pfc-port.h"

#include "dcb-channel.h"
#include "dcb-flow-control-port.h"
#include "dcb-traffic-control.h"

#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcbPfcPort");

NS_OBJECT_ENSURE_REGISTERED(DcbPfcPort);

TypeId
DcbPfcPort::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DcbPfcPort").SetParent<DcbFlowControlPort>().SetGroupName("Dcb");
    return tid;
}

DcbPfcPort::DcbPfcPort(Ptr<NetDevice> dev, Ptr<DcbTrafficControl> tc)
    : DcbFlowControlPort(dev, tc),
      m_port(dev->GetIfIndex())
{
    m_egressResumeEvents.resize(DcbTrafficControl::PRIORITY_NUMBER);
    NS_LOG_FUNCTION(this);
}

DcbPfcPort::~DcbPfcPort()
{
    NS_LOG_FUNCTION(this);
}

void
DcbPfcPort::DoIngressProcess(Ptr<const Packet> packet,
                             uint16_t protocol,
                             const Address& from,
                             const Address& to,
                             NetDevice::PacketType packetType)
{
    NS_LOG_FUNCTION(this << packet << protocol << from << to << packetType);

    CoSTag cosTag;
    packet->PeekPacketTag(cosTag);
    uint8_t priority = cosTag.GetCoS();

    if (CheckEnableVec(priority))
    {
        if (CheckShouldSendPause(priority, packet->GetSize()))
        {
            DoSendPause(priority, from);
        }
    }
}

void
DcbPfcPort::DoSendPause(uint8_t priority, const Address& from)
{
    NS_LOG_DEBUG("PFC: Send pause frame from node " << Simulator::GetContext() << " port "
                                                    << m_dev->GetIfIndex());
    Ptr<Packet> pfcFrame = PfcFrame::GeneratePauseFrame(priority);
    // pause frames are sent directly to device without queueing in egress QueueDisc
    m_dev->Send(pfcFrame, from, PfcFrame::PROT_NUMBER);
    SetUpstreamPaused(priority, true); // mark this ingress queue as paused

    // Schedule a recheck event when the pause frame up to expire
    Ptr<DcbNetDevice> device = DynamicCast<DcbNetDevice>(m_dev);
    Ptr<DcbChannel> channel = DynamicCast<DcbChannel>(device->GetChannel());
    Time delay = channel->GetDelay();
    Time pauseTime = PauseDuration(0xffff, device->GetDataRate()); // XXX Only static quanta for now
    
    /* XXX IT ISN'T ACTUALLY THE WAY PFC WORKS.
     * Consider the one-hop delay and a (possibly) sending packet, we schedule the
     * recheck event at pauseTime - 2 * delay (delay is often large than MTU/linerate).
     * However, the previous pause frame is also delayed by the one-hop delay...
     */
    EventId event = Simulator::Schedule(pauseTime - 2 * delay,
                                        &DcbPfcPort::UpstreamPauseExpired,
                                        this,
                                        priority,
                                        from);
    IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    if (q.pauseEvent.IsRunning())
        q.pauseEvent.Cancel();
    q.pauseEvent = event;
}

void
DcbPfcPort::DoPacketOutCallbackProcess(uint8_t priority, Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this);

    if (CheckShouldSendResume(priority))
    {
        NS_LOG_DEBUG("PFC: Send resume frame from node " << Simulator::GetContext() << " port "
                                                         << m_dev->GetIfIndex());
        Ptr<Packet> pfcFrame = PfcFrame::GeneratePauseFrame(priority, (uint16_t)0);
        m_dev->Send(pfcFrame, Address(), PfcFrame::PROT_NUMBER);
        SetUpstreamPaused(priority, false);

        // Cancel the pause recheck event
        IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
        if (q.pauseEvent.IsRunning())
            q.pauseEvent.Cancel();
    }
}

void
DcbPfcPort::DoEgressProcess(Ptr<Packet> packet)
{ // PFC does nothing when packet goes out throught this port
}

void
DcbPfcPort::ReceivePfc(Ptr<NetDevice> dev,
                       Ptr<const Packet> packet,
                       uint16_t protocol,
                       const Address& from,
                       const Address& to,
                       NetDevice::PacketType packetType)
{
    NS_LOG_FUNCTION(this << dev << protocol << from << to);

    Ptr<DcbNetDevice> device = DynamicCast<DcbNetDevice>(dev);
    const uint32_t index = device->GetIfIndex();
    PfcFrame pfcFrame;
    packet->PeekHeader(pfcFrame);
    uint8_t enableVec = pfcFrame.GetEnableClassField();
    for (uint8_t priority = 0; enableVec > 0; enableVec >>= 1, priority++)
    {
        if (enableVec & 1)
        {
            uint16_t quanta = pfcFrame.GetQuanta(priority);
            Ptr<PausableQueueDisc> qDisc = device->GetQueueDisc();

            if (quanta > 0)
            {
                qDisc->SetPaused(priority, true);
                Time pauseTime = PauseDuration(quanta, device->GetDataRate());
                EventId event =
                    Simulator::Schedule(pauseTime,
                                        &PausableQueueDisc::SetPaused,
                                        qDisc,
                                        priority,
                                        false); // resume the queue after the pause time.
                // Update egress resume event
                if (m_egressResumeEvents[priority].IsRunning())
                    m_egressResumeEvents[priority].Cancel();
                m_egressResumeEvents[priority] = event;
                NS_LOG_DEBUG("PFC: node " << Simulator::GetContext() << " port " << index
                                          << " priority " << (uint32_t)priority << " is paused");
            }
            else
            {
                qDisc->SetPaused(priority, false);
                // Cancel egress resume event
                if (m_egressResumeEvents[priority].IsRunning())
                    m_egressResumeEvents[priority].Cancel();
                // CancelPauseEvent(priority);
                NS_LOG_DEBUG("PFC: node " << Simulator::GetContext() << " port " << index
                                          << " priority " << (uint32_t)priority << " is resumed");
            }
        }
    }
}

void
DcbPfcPort::ConfigQueue(uint32_t priority, uint32_t reserve, uint32_t xon)
{
    NS_LOG_FUNCTION(this << priority << reserve << xon);
    IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    q.xon = xon;
    q.reserve = reserve;
}

inline bool
DcbPfcPort::CheckEnableVec(uint8_t cls)
{
    return (m_port.m_enableVec & (1 << cls)) == 1;
}

void
DcbPfcPort::SetEnableVec(uint8_t enableVec)
{
    NS_LOG_FUNCTION(this << enableVec);
    m_port.m_enableVec = enableVec;
}

inline bool
DcbPfcPort::CheckShouldSendPause(uint8_t priority, uint32_t packetSize) const
{
    // TODO: add support for dynamic threshold
    const IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    // Why compare with reserve - packetSize?
    return !q.isUpstreamPaused &&
           m_tc->CompareIngressQueueLength(m_port.m_index, priority, q.reserve - packetSize) > 0;
}

inline bool
DcbPfcPort::CheckShouldSendResume(uint8_t priority) const
{
    // TODO: add support for dynamic threshold
    const IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    return q.isUpstreamPaused &&
           m_tc->CompareIngressQueueLength(m_port.m_index, priority, q.xon) <= 0;
}

inline void
DcbPfcPort::SetUpstreamPaused(uint8_t priority, bool paused)
{
    m_port.getQueue(priority).isUpstreamPaused = paused;
}

void
DcbPfcPort::UpstreamPauseExpired(uint8_t priority, Address from)
{
    NS_LOG_FUNCTION(this << (uint32_t)priority << from);

    // The pervious pause frame has expired
    SetUpstreamPaused(priority, false);

    // Check the queue length again
    if (CheckShouldSendPause(priority, 0))
    {
        DoSendPause (priority, from);
    }
}

Time
DcbPfcPort::PauseDuration(uint16_t quanta, DataRate lineRate) const
{
    return NanoSeconds(1e9 * quanta * PfcFrame::QUANTUM_BIT / lineRate.GetBitRate());
}

/**
 * class DcbPfcControl::IngressPortInfo implementation starts.
 */

DcbPfcPort::IngressPortInfo::IngressPortInfo(uint32_t index)
    : m_index(index),
      m_enableVec(0xff)
{
    m_ingressQueues.resize(DcbTrafficControl::PRIORITY_NUMBER);
}

DcbPfcPort::IngressPortInfo::IngressPortInfo(uint32_t index, uint8_t enableVec)
    : m_index(index),
      m_enableVec(enableVec)
{
    m_ingressQueues.resize(DcbTrafficControl::PRIORITY_NUMBER);
}

inline const DcbPfcPort::IngressPortInfo::IngressQueueInfo&
DcbPfcPort::IngressPortInfo::getQueue(uint8_t priority) const
{
    return m_ingressQueues[priority];
}

inline DcbPfcPort::IngressPortInfo::IngressQueueInfo&
DcbPfcPort::IngressPortInfo::getQueue(uint8_t priority)
{
    return m_ingressQueues[priority];
}

/** class DcbPfcControl::IngressPortInfo implementation finished. */

/**
 * class DcbPfcControl::IngressPortInfo::IngressQueueInfo implementation starts.
 */

DcbPfcPort::IngressPortInfo::IngressQueueInfo::IngressQueueInfo()
    : reserve(0),
      xon(0),
      isUpstreamPaused(false),
      pauseEvent()
{
}

/** class DcbPfcControl::IngressPortInfo::IngressQueueInfo implementation finished. */

} // namespace ns3
