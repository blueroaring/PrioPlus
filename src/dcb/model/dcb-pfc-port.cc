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
            NS_LOG_DEBUG("PFC: Send pause frame from node " << Simulator::GetContext() << " port "
                                                            << m_dev->GetIfIndex());
            Ptr<Packet> pfcFrame = PfcFrame::GeneratePauseFrame(priority);
            // pause frames are sent directly to device without queueing in egress QueueDisc
            m_dev->Send(pfcFrame, from, PfcFrame::PROT_NUMBER);
            SetPaused(priority, true); // mark this ingress queue as paused
        }
    }
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
        SetPaused(priority, false);
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
                uint64_t bitRate = device->GetDataRate().GetBitRate();
                qDisc->SetPaused(priority, true);
                Time pauseTime = NanoSeconds(1e9 * quanta * PfcFrame::QUANTUM_BIT / bitRate);
                EventId event =
                    Simulator::Schedule(pauseTime,
                                        &PausableQueueDisc::SetPaused,
                                        qDisc,
                                        priority,
                                        false); // resume the queue after the pause time.
                UpdatePauseEvent(priority, event);
                NS_LOG_DEBUG("PFC: node " << Simulator::GetContext() << " port " << index
                                          << " priority " << (uint32_t)priority << " is paused");
            }
            else
            {
                qDisc->SetPaused(priority, false);
                CancelPauseEvent(priority);
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
    PortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
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
    const PortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    // Why compare with reserve - packetSize?
    return !q.isPaused &&
           m_tc->CompareIngressQueueLength(m_port.m_index, priority, q.reserve - packetSize) > 0;
}

inline bool
DcbPfcPort::CheckShouldSendResume(uint8_t priority) const
{
    // TODO: add support for dynamic threshold
    const PortInfo::IngressQueueInfo& q = m_port.getQueue(priority);
    return q.isPaused && m_tc->CompareIngressQueueLength(m_port.m_index, priority, q.xon) <= 0;
}

inline void
DcbPfcPort::SetPaused(uint8_t priority, bool paused)
{
    m_port.getQueue(priority).isPaused = paused;
}

inline void
DcbPfcPort::UpdatePauseEvent(uint8_t priority, const EventId& event)
{
    m_port.getQueue(priority).ReplacePauseEvent(event);
}

inline void
DcbPfcPort::CancelPauseEvent(uint8_t priority)
{
    m_port.getQueue(priority).CancelPauseEvent();
}

/**
 * class DcbPfcControl::PortInfo implementation starts.
 */

DcbPfcPort::PortInfo::PortInfo(uint32_t index)
    : m_index(index),
      m_enableVec(0xff)
{
    m_ingressQueues.resize(DcbTrafficControl::PRIORITY_NUMBER);
}

DcbPfcPort::PortInfo::PortInfo(uint32_t index, uint8_t enableVec)
    : m_index(index),
      m_enableVec(enableVec)
{
    m_ingressQueues.resize(DcbTrafficControl::PRIORITY_NUMBER);
}

inline const DcbPfcPort::PortInfo::IngressQueueInfo&
DcbPfcPort::PortInfo::getQueue(uint8_t priority) const
{
    return m_ingressQueues[priority];
}

inline DcbPfcPort::PortInfo::IngressQueueInfo&
DcbPfcPort::PortInfo::getQueue(uint8_t priority)
{
    return m_ingressQueues[priority];
}

/** class DcbPfcControl::PortInfo implementation finished. */

/**
 * class DcbPfcControl::PortInfo::IngressQueueInfo implementation starts.
 */

DcbPfcPort::PortInfo::IngressQueueInfo::IngressQueueInfo()
    : reserve(0),
      xon(0),
      isPaused(false),
      hasEvent(false),
      pauseEvent()
{
}

inline void
DcbPfcPort::PortInfo::IngressQueueInfo::ReplacePauseEvent(const EventId& event)
{
    if (hasEvent)
    {
        pauseEvent.Cancel();
    }
    hasEvent = true;
    pauseEvent = event;
}

inline void
DcbPfcPort::PortInfo::IngressQueueInfo::CancelPauseEvent()
{
    if (hasEvent)
    {
        hasEvent = false;
        pauseEvent.Cancel();
    }
}

/** class DcbPfcControl::PortInfo::IngressQueueInfo implementation finished. */

} // namespace ns3
