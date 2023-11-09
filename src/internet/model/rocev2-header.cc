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

#include "rocev2-header.h"

#include "udp-header.h"

#include "ns3/fatal-error.h"
#include "ns3/type-id.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Header);

TypeId
RoCEv2Header::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::RoCEv2Header")
                            .SetParent<Header>()
                            .SetGroupName("Dcb")
                            .AddConstructor<RoCEv2Header>();
    return tid;
}

TypeId
RoCEv2Header::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

RoCEv2Header::RoCEv2Header()
    : m_opcode(RoCEv2Header::Opcode::RC_SEND_ONLY),
      m_pKey(0)
{
    m_ua.u = 0;
    m_ub.u = 0;
    m_uc.u = 0;
}

uint32_t
RoCEv2Header::GetSerializedSize(void) const
{
    return 12;
}

void
RoCEv2Header::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_opcode);
    i.WriteU8((m_ua.se << 7) | (m_ua.m << 6) | (m_ua.padCnt << 4) | (m_ua.tVer));
    i.WriteHtonU16(m_pKey);
    uint8_t h = (m_ub.fr << 7) | (m_ub.br << 6) | (m_ub.reserved);
    i.WriteHtonU32((h << 24) | m_ub.destQP);
    h = (m_uc.ackQ << 7) | (m_uc.reserved);
    i.WriteHtonU32((h << 24) | m_uc.psn);
}

uint32_t
RoCEv2Header::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_opcode = static_cast<RoCEv2Header::Opcode>(i.ReadU8()); // be careful here

    uint8_t b = i.ReadU8();
    m_ua.se = b >> 7;
    m_ua.m = (b >> 6) & 0b1;
    m_ua.padCnt = (b >> 4) & 0b11;
    m_ua.tVer = b & 0b1111;

    m_pKey = i.ReadNtohU16();

    uint32_t u = i.ReadNtohU32();
    uint8_t h = u >> 24;
    m_ub.fr = h >> 7;
    m_ub.br = (h >> 6) & 0b1;
    m_ub.reserved = h & 0b11'1111;
    m_ub.destQP = u & 0xffffff;

    u = i.ReadNtohU32();
    h = u >> 24;
    m_uc.ackQ = h >> 7;
    m_uc.reserved = h & 0b111'1111;
    m_uc.psn = u & 0xffffff;

    return GetSerializedSize();
}

void
RoCEv2Header::Print(std::ostream& os) const
{
    std::string opcodeStr;
    switch (m_opcode)
    {
    case Opcode::RC_SEND_ONLY:
        opcodeStr = "RC_SEND_ONLY";
        break;
    case Opcode::RC_ACK:
        opcodeStr = "RC_ACK";
        break;
    case Opcode::UD_SEND_ONLY:
        opcodeStr = "UD_SEND_ONLY";
        break;
    case Opcode::CNP:
        opcodeStr = "CNP";
        break;
    }
    os << "[RoCEv2 opcode='" << opcodeStr << "'  qp: " << GetSrcQP() << " -> " << GetDestQP()
       << " ]";
}

RoCEv2Header::Opcode
RoCEv2Header::GetOpcode() const
{
    return m_opcode;
}

void
RoCEv2Header::SetOpcode(RoCEv2Header::Opcode opcode)
{
    m_opcode = opcode;
}

uint32_t
RoCEv2Header::GetDestQP() const
{
    return m_ub.destQP & 0xfff;
}

void
RoCEv2Header::SetDestQP(uint32_t destQP)
{
    m_ub.destQP &= 0xfff000;
    m_ub.destQP |= destQP;
}

uint32_t
RoCEv2Header::GetSrcQP() const
{
    return m_ub.destQP >> 12;
}

void
RoCEv2Header::SetSrcQP(uint32_t srcQP)
{
    m_ub.destQP &= 0x000fff;
    m_ub.destQP |= srcQP << 12;
}

uint32_t
RoCEv2Header::GetPSN() const
{
    return m_uc.psn;
}

void
RoCEv2Header::SetPSN(uint32_t psn)
{
    m_uc.psn = psn & 0xffffff;
}

bool
RoCEv2Header::GetAckQ() const
{
    return m_uc.ackQ;
}

void
RoCEv2Header::SetAckQ(bool ackRequested)
{
    m_uc.ackQ = ackRequested;
}

TypeId
AETHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::AETHeader")
                            .SetParent<Header>()
                            .SetGroupName("Dcb")
                            .AddConstructor<AETHeader>();
    return tid;
}

AETHeader::AETHeader()
{
    m_u.u = 0;
}

TypeId
AETHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

uint32_t
AETHeader::GetSerializedSize(void) const
{
    return 4;
}

void
AETHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU32(m_u.u);
}

uint32_t
AETHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_u.u = i.ReadU32();
    return GetSerializedSize();
}

void
AETHeader::Print(std::ostream& os) const
{
}

AETHeader::SyndromeType
AETHeader::GetSyndromeType() const
{
    switch (m_u.syndrome >> 5)
    {
    case 0:
        return SyndromeType::FC_DISABLED;
    case 0b11:
        return SyndromeType::NACK;
    default:
        NS_FATAL_ERROR("Unknown syndrome type");
    }
}

void
AETHeader::SetSyndromeType(AETHeader::SyndromeType t)
{
    m_u.syndrome |= (static_cast<uint8_t>(t) << 5);
}

uint32_t
AETHeader::GetMSN() const
{
    return m_u.msn;
}

void
AETHeader::SetMSN(uint32_t msn)
{
    m_u.msn = msn & 0xffffff;
}

TypeId
UdpRoCEv2Header::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::UdpRoCEv2Header")
                            .SetGroupName("Dcb")
                            .SetParent<Header>()
                            .AddConstructor<UdpRoCEv2Header>();
    return tid;
}

TypeId
UdpRoCEv2Header::GetInstanceTypeId() const
{
    return GetTypeId();
}

UdpRoCEv2Header::UdpRoCEv2Header()
{
}

uint32_t
UdpRoCEv2Header::GetSerializedSize() const
{
    return m_udp.GetSerializedSize() + m_rocev2.GetSerializedSize();
}

void
UdpRoCEv2Header::Serialize(Buffer::Iterator start) const
{
    m_udp.Serialize(start);
    start.Next(m_udp.GetSerializedSize());
    m_rocev2.Serialize(start);
}

uint32_t
UdpRoCEv2Header::Deserialize(Buffer::Iterator start)
{
    m_udp.Deserialize(start);
    start.Next(m_udp.GetSerializedSize());
    m_rocev2.Deserialize(start);
    return GetSerializedSize();
}

void
UdpRoCEv2Header::Print(std::ostream& os) const
{
    m_udp.Print(os);
    os << " | ";
    m_rocev2.Print(os);
}

void
UdpRoCEv2Header::SetUdp(const UdpHeader& udp)
{
    m_udp = udp;
}

const UdpHeader&
UdpRoCEv2Header::GetUdp() const
{
    return m_udp;
}

void
UdpRoCEv2Header::SetRoCE(const RoCEv2Header& rocev2)
{
    m_rocev2 = rocev2;
}

const RoCEv2Header&
UdpRoCEv2Header::GetRoCE() const
{
    return m_rocev2;
}

} // namespace ns3
