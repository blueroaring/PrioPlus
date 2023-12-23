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

#include "dcb-flow-control-port.h"

#include "dcb-traffic-control.h"

#include "ns3/ipv4-header.h"
#include "ns3/socket.h"
#include "ns3/type-id.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcbFlowControlPort");

NS_OBJECT_ENSURE_REGISTERED(DcbFlowControlPort);

TypeId
DcbFlowControlPort::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DcbFlowControlPort").SetParent<Object>().SetGroupName("Dcb");
    return tid;
}

DcbFlowControlPort::DcbFlowControlPort(Ptr<NetDevice> dev, Ptr<DcbTrafficControl> tc)
    : m_dev(dev),
      m_tc(tc),
      m_enableIngressControl(true),
      m_enableEgressControl(false)
{
    NS_LOG_FUNCTION(this);
}

DcbFlowControlPort::~DcbFlowControlPort()
{
    NS_LOG_FUNCTION(this);
}

void
DcbFlowControlPort::IngressProcess(Ptr<const Packet> packet,
                                   uint16_t protocol,
                                   const Address& from,
                                   const Address& to,
                                   NetDevice::PacketType packetType)
{
    NS_LOG_FUNCTION(this);
    if (m_enableIngressControl)
    {
        DoIngressProcess(packet, protocol, from, to, packetType);
    }
}

void
DcbFlowControlPort::PacketOutCallbackProcess(uint8_t priority, Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    if (m_enableIngressControl)
    {
        DoPacketOutCallbackProcess(priority, packet);
    }
}

void
DcbFlowControlPort::EgressProcess(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this);
    if (m_enableEgressControl)
    {
        DoEgressProcess(packet);
    }
}

void
DcbFlowControlPort::SetFcIngressEnabled(bool enable)
{
    m_enableIngressControl = enable;
}

void
DcbFlowControlPort::SetFcEgressEnabled(bool enable)
{
    m_enableEgressControl = enable;
}
} // namespace ns3
