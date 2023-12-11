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

#ifndef FLOW_IDENTIFIER_H
#define FLOW_IDENTIFIER_H

#include "ns3/ipv4-address.h"

namespace ns3
{

class FlowIdentifier
{
  public:
    FlowIdentifier() = default;
    FlowIdentifier(Ipv4Address srcAddr, Ipv4Address dstAddr, uint32_t srcQP, uint32_t dstQP);
    // Copy constructor
    FlowIdentifier(const FlowIdentifier& other);

    bool operator<(const FlowIdentifier& other) const;
    bool operator==(const FlowIdentifier& other) const;
    // overload operator=
    FlowIdentifier& operator=(const FlowIdentifier& other);

    std::string GetSrcAddrString() const;
    std::string GetDstAddrString() const;

    // Only support IPv4 now
    Ipv4Address srcAddr;
    Ipv4Address dstAddr;
    uint32_t srcPort; // For RoCE, this is the QP number
    uint32_t dstPort; // For RoCE, this is the QP number
};

} // namespace ns3

#endif /* FLOW_IDENTIFIER_H */
