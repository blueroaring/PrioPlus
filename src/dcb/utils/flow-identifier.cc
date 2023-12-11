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
 * Author: Zhaochen Zhang (zhaochen.zhang@outlook.com)
 */

#include "flow-identifier.h"

#include "ns3/log.h"

namespace ns3
{

FlowIdentifier::FlowIdentifier(Ipv4Address srcAddr,
                               Ipv4Address dstAddr,
                               uint32_t srcQP,
                               uint32_t dstQP)
    : srcAddr(srcAddr),
      dstAddr(dstAddr),
      srcPort(srcQP),
      dstPort(dstQP)
{
}

FlowIdentifier::FlowIdentifier(const FlowIdentifier& other)
    : srcAddr(other.srcAddr),
      dstAddr(other.dstAddr),
      srcPort(other.srcPort),
      dstPort(other.dstPort)
{
}

bool
FlowIdentifier::operator<(const FlowIdentifier& other) const
{
    return srcAddr < other.srcAddr || dstAddr < other.dstAddr || srcPort < other.srcPort ||
           dstPort < other.dstPort;
}

bool
FlowIdentifier::operator<=(const FlowIdentifier& other) const
{
    return srcAddr.Get() <= other.srcAddr.Get() || dstAddr.Get() <= other.dstAddr.Get() ||
           srcPort <= other.srcPort || dstPort <= other.dstPort;
}

bool
FlowIdentifier::operator==(const FlowIdentifier& other) const
{
    return srcAddr == other.srcAddr && dstAddr == other.dstAddr && srcPort == other.srcPort &&
           dstPort == other.dstPort;
}

FlowIdentifier& 
FlowIdentifier::operator=(const FlowIdentifier& other)
{
    srcAddr = other.srcAddr;
    dstAddr = other.dstAddr;
    srcPort = other.srcPort;
    dstPort = other.dstPort;
    return *this;
}

std::string
FlowIdentifier::GetSrcAddrString() const
{
    // Ipv4Address only has Print() method, which is not convenient for us to use.
    // We need to convert it to string.
    std::stringstream ss;
    srcAddr.Print(ss);
    return ss.str();
}

std::string
FlowIdentifier::GetDstAddrString() const
{
    // Ipv4Address only has Print() method, which is not convenient for us to use.
    // We need to convert it to string.
    std::stringstream ss;
    dstAddr.Print(ss);
    return ss.str();
}

} // namespace ns3
