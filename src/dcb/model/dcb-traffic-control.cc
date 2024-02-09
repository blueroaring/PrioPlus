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

#include "dcb-traffic-control.h"

#include "dcb-flow-control-port.h"
#include "dcb-pfc-port.h"
#include "pausable-queue-disc.h"

#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/callback.h"
#include "ns3/ethernet-header.h"
#include "ns3/fatal-error.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/log-macros-enabled.h"
#include "ns3/nstime.h"
#include "ns3/pfc-frame.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/type-id.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcbTrafficControl");

NS_OBJECT_ENSURE_REGISTERED(DcbTrafficControl);

TypeId
DcbTrafficControl::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::DcbTrafficControl")
            .SetParent<TrafficControlLayer>()
            .SetGroupName("Dcb")
            .AddConstructor<DcbTrafficControl>()
            .AddTraceSource("BufferOverflow",
                            "Trace source indicating buffer overflow",
                            MakeTraceSourceAccessor(&DcbTrafficControl::m_bufferOverflowTrace),
                            "ns3::Packet::TracedCallback");
    ;
    return tid;
}

TypeId
DcbTrafficControl::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

DcbTrafficControl::DcbTrafficControl()
    : TrafficControlLayer()
{
    NS_LOG_FUNCTION(this);
}

DcbTrafficControl::~DcbTrafficControl()
{
    NS_LOG_FUNCTION(this);
}

void
DcbTrafficControl::SetRootQueueDiscOnDevice(Ptr<DcbNetDevice> device, Ptr<PausableQueueDisc> qDisc)
{
    NS_LOG_FUNCTION(this << device << qDisc);

    device->SetQueueDisc(qDisc);
    TrafficControlLayer::SetRootQueueDiscOnDevice(device, qDisc);
}

void
DcbTrafficControl::RegisterDeviceNumber(const uint32_t num)
{
    NS_LOG_FUNCTION(this << num);
    m_buffer.RegisterPortNumber(num);
}

void
DcbTrafficControl::SetBufferSize(uint32_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);

    m_buffer.SetBufferSpace(bytes);
}

void
DcbTrafficControl::Receive(Ptr<NetDevice> device,
                           Ptr<const Packet> packet,
                           uint16_t protocol,
                           const Address& from,
                           const Address& to,
                           NetDevice::PacketType packetType)
{
    NS_LOG_FUNCTION(this << device << packet << protocol << from << to << packetType);

    // Add priority to packet tag
    uint8_t priority = PeekPriorityOfPacket(packet);
    CoSTag cosTag;
    cosTag.SetCoS(priority);
    packet->AddPacketTag(cosTag); // CoSTag is removed in EgressProcess

    // Add from-index to packet tag
    uint32_t index = device->GetIfIndex();
    DeviceIndexTag tag(index);
    packet->AddPacketTag(tag); // egress will read the index from tag to decrement counter
    // update ingress queue length
    // Ipv4Header ipv4Header;
    // packet->PeekHeader(ipv4Header);
    // bool success = m_buffer.InPacketProcess(index,
    //                                         priority,
    //                                         packet->GetSize() - ipv4Header.GetSerializedSize());
    // if (!success)
    // {
    //     m_bufferOverflowTrace(packet);
    //     return;
    // }

    // const PortInfo& port = m_buffer.GetPort(index);
    // if (port.FcEnabled())
    // {
    //     // run flow control ingress process
    //     port.GetFC()->IngressProcess(packet, protocol, from, to, packetType);
    // }
    TrafficControlLayer::Receive(device, packet, protocol, from, to, packetType);
}

void
DcbTrafficControl::Send(Ptr<NetDevice> device, Ptr<QueueDiscItem> item)
{
    Ptr<Packet> pkt = item->GetPacket()->Copy();
    // Get inDev's priority and index from tag
    DeviceIndexTag devTag;
    pkt->PeekPacketTag(devTag);
    CoSTag cosTag;
    pkt->PeekPacketTag(cosTag);

    uint32_t inPortIndex = devTag.GetIndex();
    uint8_t inQueuePriority = cosTag.GetCoS();

    // Get outDev's priority and index from tag
    uint32_t outPortIndex = device->GetIfIndex();
    uint8_t outQueuePriority = inQueuePriority;

    // Check enqueue admission
    bool success = m_buffer.InPacketProcess(inPortIndex,
                                            inQueuePriority,
                                            outPortIndex,
                                            outQueuePriority,
                                            pkt->GetSize());
    if (!success)
    {
        m_bufferOverflowTrace(pkt);
        return;
    }
    const PortInfo& port = m_buffer.GetPort(inPortIndex);
    if (port.FcEnabled())
    {
        // run flow control ingress process
        port.GetFC()->IngressProcess(device, item);
    }

    TrafficControlLayer::Send(device, item);
}

void
DcbTrafficControl::EgressProcess(uint32_t outPort, uint8_t priority, Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << outPort << priority << packet);
    DeviceIndexTag devTag;
    packet->RemovePacketTag(devTag); // egress will remove the tag
    uint32_t inPortIndex = devTag.GetIndex();

    CoSTag cosTag;
    packet->RemovePacketTag(cosTag);
    uint8_t inQueuePriority = cosTag.GetCoS() & 0x0f;

    m_buffer.OutPacketProcess(inPortIndex, inQueuePriority, outPort, priority, packet->GetSize());

    // Call the packet out pipeline of the ingress port
    PortInfo& port = m_buffer.GetPort(outPort);
    port.CallFCPacketOutPipeline(inPortIndex, priority, packet);
    if (port.FcEnabled())
    {
        port.GetFC()->EgressProcess(packet);
    }
}

void
DcbTrafficControl::InstallFCToPort(uint32_t portIdx,
                                   Ptr<DcbFlowControlPort> fc,
                                   std::vector<ns3::Ptr<ns3::DcbFlowControlMmuQueue>> fcMmuQueues)
{
    NS_LOG_FUNCTION(this << portIdx);
    m_buffer.GetPort(portIdx).SetFC(fc, fcMmuQueues);
    m_buffer.SetPortFcMmuBufferCallback(portIdx);

    // Set egress callback to other ports.
    // When we enable FC on one port, it means that other ports may do something when
    // sending out the packet.
    // For example, if we config PFC on port 0, than ports other than 0 should check
    // whether port 0 has to send RESUME frame when sending out a packet.
    PortInfo::FCPacketOutCb cb = MakeCallback(&DcbFlowControlPort::PacketOutCallbackProcess, fc);
    for (auto& port : m_buffer.GetPorts())
    {
        port.AddPacketOutCallback(portIdx, cb);
    }
}

// static
uint8_t
DcbTrafficControl::PeekPriorityOfPacket(const Ptr<const Packet> packet)
{
    Ipv4Header ipv4Header;
    packet->PeekHeader(ipv4Header);
    return Socket::IpTos2Priority(ipv4Header.GetTos());
    // return ipv4Header.GetDscp () >> 3;
}

DcbTrafficControl::PortInfo::PortInfo()
    : m_fcEnabled(false),
      m_fc(nullptr)
{
}

void
DcbTrafficControl::PortInfo::AddPacketOutCallback(uint32_t fromIdx, FCPacketOutCb cb)
{
    NS_LOG_FUNCTION(this);
    m_fcPacketOutPipeline.emplace_back(fromIdx, cb);
}

void
DcbTrafficControl::PortInfo::CallFCPacketOutPipeline(uint32_t fromIdx,
                                                     uint8_t priority,
                                                     Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // Check all handler and call the one with the same ingress port
    for (const auto& handler : m_fcPacketOutPipeline)
    {
        if (handler.first == fromIdx)
        {
            const FCPacketOutCb& cb = handler.second;
            cb(priority, packet);
        }
    }
}

DcbTrafficControl::Buffer::Buffer()
    : m_totalSize(32 * 1024 * 1024),
      m_totalSharedSize(0),
      m_hasCalSharedSize(false)
{
}

void
DcbTrafficControl::Buffer::SetBufferSpace(uint32_t bytes)
{
    NS_LOG_FUNCTION(this << bytes);

    m_totalSize = bytes;
}

void
DcbTrafficControl::Buffer::RegisterPortNumber(const uint32_t num)
{
    NS_LOG_FUNCTION(this << num);

    m_ports.resize(num);
}

bool
DcbTrafficControl::Buffer::InPacketProcess(uint32_t inPortIndex,
                                           uint8_t inQueuePriority,
                                           uint32_t outPortIndex,
                                           uint8_t outQueuePriority,
                                           uint32_t packetSize)
{
    Ptr<DcbFlowControlMmuQueue> inQueue = m_ports[inPortIndex].GetFCMmuQueue(inQueuePriority);
    Ptr<DcbFlowControlMmuQueue> outQueue = m_ports[outPortIndex].GetFCMmuQueue(outQueuePriority);
    bool success =
        inQueue->CheckIngressAdmission(packetSize) & outQueue->CheckEgressAdmission(packetSize);
    if (success)
    {
        inQueue->IngressIncrement(packetSize);
        outQueue->EgressIncrement(packetSize);
        return true;
    }
    NS_LOG_DEBUG("Buffer overflow, packet drop.");
    return false; // buffer overflow
}

void
DcbTrafficControl::Buffer::OutPacketProcess(uint32_t inPortIndex,
                                            uint8_t inQueuePriority,
                                            uint32_t outPortIndex,
                                            uint8_t outQueuePriority,
                                            uint32_t packetSize)
{
    Ptr<DcbFlowControlMmuQueue> inQueue = m_ports[inPortIndex].GetFCMmuQueue(inQueuePriority);
    Ptr<DcbFlowControlMmuQueue> outQueue = m_ports[outPortIndex].GetFCMmuQueue(outQueuePriority);
    inQueue->IngressDecrement(packetSize);
    outQueue->EgressDecrement(packetSize);
}

// inline DcbTrafficControl::PortInfo&
// DcbTrafficControl::Buffer::GetPort(uint32_t portIndex)
// {
//     return m_ports[portIndex];
// }

// std::vector<DcbTrafficControl::PortInfo>&
// DcbTrafficControl::Buffer::GetPorts()
// {
//     return m_ports;
// }

uint32_t
DcbTrafficControl::Buffer::GetSharedSize()
{
    if (!m_hasCalSharedSize)
    {
        uint32_t size = m_totalSize;
        for (uint32_t i = 1; i < m_ports.size(); i++) // 0 is LoopbackNetDev
        {
            uint32_t prirority = PRIORITY_NUMBER;
            for (uint32_t j = 0; j < prirority; j++)
            {
                uint32_t exclusiveSize = m_ports[i].GetFCMmuQueue(j)->GetExclusiveBufferSize();
                NS_ASSERT_MSG(exclusiveSize <= size,
                              "Exclusive buffer size is larger than total size");
                size -= exclusiveSize;
            }
        }
        m_totalSharedSize = size;
        m_hasCalSharedSize = true;
    }
    return m_totalSharedSize;
}

uint32_t
DcbTrafficControl::Buffer::GetSharedUsed()
{
    uint32_t sum = 0;
    for (uint32_t i = 1; i < m_ports.size(); i++) // 0 is LoopbackNetDev
    {
        uint32_t prirority = PRIORITY_NUMBER;
        for (uint32_t j = 0; j < prirority; j++)
        {
            sum += m_ports[i].GetFCMmuQueue(j)->GetExclusiveSharedBufferUsed();
        }
    }
    return sum;
}

void
DcbTrafficControl::Buffer::SetPortFcMmuBufferCallback(uint32_t portIndex)
{
    const auto& port = m_ports[portIndex];
    uint32_t prirority = PRIORITY_NUMBER;
    for (uint32_t i = 0; i < prirority; i++)
    {
        Ptr<DcbFlowControlMmuQueue> queue = port.GetFCMmuQueue(i);
        queue->SetBufferCallback(MakeCallback(&DcbTrafficControl::Buffer::GetSharedSize, this),
                                 MakeCallback(&DcbTrafficControl::Buffer::GetSharedUsed, this));
    }
}

/** Tags implementation **/

DeviceIndexTag::DeviceIndexTag(uint32_t index)
    : m_index(index)
{
}

void
DeviceIndexTag::SetIndex(uint32_t index)
{
    m_index = index;
}

uint32_t
DeviceIndexTag::GetIndex() const
{
    return m_index;
}

TypeId
DeviceIndexTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DevIndexTag")
                            .SetParent<Tag>()
                            .SetGroupName("Dcb")
                            .AddConstructor<DeviceIndexTag>();
    return tid;
}

TypeId
DeviceIndexTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DeviceIndexTag::GetSerializedSize() const
{
    return sizeof(uint32_t);
}

void
DeviceIndexTag::Serialize(TagBuffer i) const
{
    i.WriteU32(m_index);
}

void
DeviceIndexTag::Deserialize(TagBuffer i)
{
    m_index = i.ReadU32();
}

void
DeviceIndexTag::Print(std::ostream& os) const
{
    os << "Device = " << m_index;
}

CoSTag::CoSTag(uint8_t cos)
    : m_cos(cos)
{
}

void
CoSTag::SetCoS(uint8_t cos)
{
    m_cos = cos;
}

uint8_t
CoSTag::GetCoS() const
{
    return m_cos;
}

TypeId
CoSTag::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::CoSTag").SetParent<Tag>().SetGroupName("Dcb").AddConstructor<CoSTag>();
    return tid;
}

TypeId
CoSTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
CoSTag::GetSerializedSize() const
{
    return sizeof(uint8_t);
}

void
CoSTag::Serialize(TagBuffer i) const
{
    i.WriteU8(m_cos);
}

void
CoSTag::Deserialize(TagBuffer i)
{
    m_cos = i.ReadU8();
}

void
CoSTag::Print(std::ostream& os) const
{
    os << "Device = " << m_cos;
}

} // namespace ns3
