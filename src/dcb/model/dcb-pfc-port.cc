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
        TypeId("ns3::DcbPfcPort")
            .SetParent<DcbFlowControlPort>()
            .SetGroupName("Dcb")
            .AddTraceSource("PfcSent",
                            "PFC pause frame sent",
                            MakeTraceSourceAccessor(&DcbPfcPort::m_tracePfcSent),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("PfcReceived",
                            "PFC pause frame received",
                            MakeTraceSourceAccessor(&DcbPfcPort::m_tracePfcReceived),
                            "ns3::Packet::TracedCallback");
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
DcbPfcPort::DoIngressProcess(Ptr<NetDevice> outDev, Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << outDev << item);
    Ptr<Packet> packet = item->GetPacket();
    Ptr<DcbNetDevice> device = DynamicCast<DcbNetDevice>(m_dev);

    CoSTag cosTag;
    packet->PeekPacketTag(cosTag);
    uint8_t priority = cosTag.GetCoS();

    if (CheckEnableVec(priority))
    {
        if (CheckShouldSendPause(priority))
        {
            DoSendPause(priority, device->GetRemote());
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
    m_tracePfcSent(GetNodeAndPortId(), priority, true);

    Ptr<DcbNetDevice> device = DynamicCast<DcbNetDevice>(m_dev);
    Ptr<DcbChannel> channel = DynamicCast<DcbChannel>(device->GetChannel());
    Time delay = channel->GetDelay();
    EventId event;
    IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    if (m_reactionType == RESEND_PAUSE)
    {
        /* XXX IT ISN'T ACTUALLY THE WAY PFC WORKS.
         * Schedule a recheck event when the pause frame up to expire
         */
        Time pauseTime =
            PauseDuration(0xffff, device->GetDataRate()); // XXX Only static quanta for now
        /* Consider the one-hop delay and a (possibly) sending packet, we schedule the
         * recheck event at pauseTime - 2 * delay (delay is often large than MTU/linerate).
         * However, the previous pause frame is also delayed by the one-hop delay...
         */
        event = Simulator::Schedule(pauseTime - 2 * delay,
                                    &DcbPfcPort::UpstreamPauseExpired,
                                    this,
                                    priority,
                                    from);
    }
    else if (m_reactionType == RESET_UPSTREAM_PAUSED)
    {
        event =
            Simulator::Schedule(3 * delay, &DcbPfcPort::SetUpstreamPaused, this, priority, false);
    }
    // Update pause event
    if (q.pauseEvent.IsRunning())
        q.pauseEvent.Cancel();
    q.pauseEvent = event;
}

void
DcbPfcPort::UpstreamPauseExpired(uint8_t priority, Address from)
{
    NS_LOG_FUNCTION(this << (uint32_t)priority << from);

    // The pervious pause frame has expired
    SetUpstreamPaused(priority, false);

    // Check the queue length again
    if (CheckEnableVec(priority))
    {
        if (CheckShouldSendPause(priority))
        {
            DoSendPause(priority, from);
        }
    }
}

void
DcbPfcPort::DoPacketOutCallbackProcess(uint32_t priority, Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this);

    if (CheckShouldSendResume(priority))
    {
        NS_LOG_DEBUG("PFC: Send resume frame from node " << Simulator::GetContext() << " port "
                                                         << m_dev->GetIfIndex());
        Ptr<Packet> pfcFrame = PfcFrame::GeneratePauseFrame(priority, (uint16_t)0);
        m_dev->Send(pfcFrame, Address(), PfcFrame::PROT_NUMBER);
        SetUpstreamPaused(priority, false);
        m_tracePfcSent(GetNodeAndPortId(), priority, false);

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

                if (m_reactionType != NEVER_EXPIRE)
                {
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
                }

                m_tracePfcReceived(GetNodeAndPortId(), priority, true);
                NS_LOG_DEBUG("PFC: node " << Simulator::GetContext() << " port " << index
                                          << " priority " << (uint32_t)priority << " is paused");
            }
            else
            {
                qDisc->SetPaused(priority, false);
                
                if (m_reactionType != NEVER_EXPIRE)
                {
                    // Cancel egress resume event
                    if (m_egressResumeEvents[priority].IsRunning())
                        m_egressResumeEvents[priority].Cancel();
                    // CancelPauseEvent(priority);
                }

                m_tracePfcReceived(GetNodeAndPortId(), priority, false);
                NS_LOG_DEBUG("PFC: node " << Simulator::GetContext() << " port " << index
                                          << " priority " << (uint32_t)priority << " is resumed");
            }
        }
    }
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

bool
DcbPfcPort::CheckShouldSendPause(uint8_t priority) const
{
    const IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    Ptr<DcbPfcMmuQueue> queueMmu =
        DynamicCast<DcbPfcMmuQueue>(m_tc->GetPorts().at(m_port.m_index).GetFCMmuQueue(priority));
    return !q.isUpstreamPaused && queueMmu->CheckShouldSendPause();
}

bool
DcbPfcPort::CheckShouldSendResume(uint8_t priority) const
{
    const IngressPortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    Ptr<DcbPfcMmuQueue> queueMmu =
        DynamicCast<DcbPfcMmuQueue>(m_tc->GetPorts().at(m_port.m_index).GetFCMmuQueue(priority));
    return q.isUpstreamPaused && queueMmu->CheckShouldSendResume();
}

inline void
DcbPfcPort::SetUpstreamPaused(uint8_t priority, bool paused)
{
    m_port.getQueue(priority).isUpstreamPaused = paused;
}

Time
DcbPfcPort::PauseDuration(uint16_t quanta, DataRate lineRate) const
{
    return NanoSeconds(1e9 * quanta * PfcFrame::QUANTUM_BIT / lineRate.GetBitRate());
}

std::pair<uint32_t, uint32_t>
DcbPfcPort::GetNodeAndPortId() const
{
    return std::make_pair(m_dev->GetNode()->GetId(), m_dev->GetIfIndex());
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
    : isUpstreamPaused(false),
      pauseEvent()
{
}

/** class DcbPfcControl::IngressPortInfo::IngressQueueInfo implementation finished. */

} // namespace ns3
