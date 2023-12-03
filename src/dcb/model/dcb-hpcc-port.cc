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

#include "dcb-hpcc-port.h"

#include "ns3/hpcc-header.h"
#include "ns3/rocev2-header.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcbHpccPort");

NS_OBJECT_ENSURE_REGISTERED(DcbHpccPort);

TypeId
DcbHpccPort::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DcbHpccPort").SetParent<DcbPfcPort>().SetGroupName("Dcb");
    return tid;
}

DcbHpccPort::DcbHpccPort(Ptr<NetDevice> dev, Ptr<DcbTrafficControl> tc)
    : DcbPfcPort(dev, tc),
      m_txBytes(0)
{
    NS_LOG_FUNCTION(this);
}

DcbHpccPort::~DcbHpccPort()
{
    NS_LOG_FUNCTION(this);
}

void
DcbHpccPort::DoEgressProcess(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);

    // DcbPfcPort::DoEgressProcess(packet);

    // Add m_txBytes
    m_txBytes += packet->GetSize();

    // Remove packet's IPv4Header
    Ipv4Header ipv4Header;
    packet->RemoveHeader(ipv4Header);

    // Check and Remove packet's RoCEv2 Header
    UdpHeader udpHeader;
    packet->PeekHeader(udpHeader);
    if (udpHeader.GetSourcePort() == 4791) // RoCEv2L4Protocol::PROT_NUMBER
    {                                      // RoCEv2
        UdpRoCEv2Header udpRoCEheader;
        packet->RemoveHeader(udpRoCEheader);

        // Check if RoCEv2 Header is not ACK
        if (udpRoCEheader.GetRoCE().GetOpcode() != RoCEv2Header::Opcode::RC_ACK)
        {
            // Remove and Modify packet's HPCC header
            HpccHeader hpccHeader;
            packet->RemoveHeader(hpccHeader);

            // XXX since HPCC doesn't consider multi-priority queue, we simply calculate queue
            // length and txBytes by adding all queues together.
            Ptr<DcbNetDevice> device = DynamicCast<DcbNetDevice>(m_dev);
            uint64_t qLen = 0;
            size_t nQueue = device->GetQueueDisc()->GetNQueueDiscClasses();

            // Sum queue length of each priority queue
            for (size_t i = 0; i < nQueue; i++)
            {
                qLen += device->GetQueueDisc()->GetQueueDiscClass(i)->GetQueueDisc()->GetNBytes();
            }

            // Push into HpccHeader
            hpccHeader.PushHop(Simulator::Now().GetTimeStep(),
                               m_txBytes,
                               qLen,
                               device->GetDataRate());
        }

        // Rebuild packet's UdpRocev2Header
        packet->AddHeader(udpRoCEheader);
    }
    // Rebuild packet's Ipv4Header
    packet->AddHeader(ipv4Header);
}

} // namespace ns3
