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

#ifndef ROCEV2_HEADR_H
#define ROCEV2_HEADR_H

#include "udp-header.h"

#include "ns3/header.h"

namespace ns3
{

/**
 * Infiniband Base Transport Header (BTH)
 * The header is 12 bytes.
 */
class RoCEv2Header : public Header
{
  public:
    RoCEv2Header();

    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const override;

    virtual uint32_t GetSerializedSize(void) const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream& os) const override;

    enum Opcode
    { // part of the opcodes for now
        RC_SEND_ONLY = 0b000'00100,
        RC_ACK = 0b000'10001,
        UD_SEND_ONLY = 0b011'00100,
        CNP = 0b100'00000
    };

    Opcode GetOpcode() const;
    void SetOpcode(Opcode opcode);

    uint32_t GetDestQP() const;
    void SetDestQP(uint32_t destQP);

    uint32_t GetSrcQP() const;
    void SetSrcQP(uint32_t srcQP);

    uint32_t GetPSN() const;
    void SetPSN(uint32_t psn);

    bool GetAckQ() const;
    void SetAckQ(bool ackRequested);

  private:
    Opcode m_opcode;

    union {
        struct
        {
            uint8_t se : 1;     // solicited event
            uint8_t m : 1;      // MigReq
            uint8_t padCnt : 2; // Pad count
            uint8_t tVer : 4;   // Transport header version
        } __attribute__((__packed__));

        uint8_t u;
    } m_ua;

    uint16_t m_pKey; // Partition key

    union {
        struct
        {
            uint8_t fr : 1;       // F/Res1
            uint8_t br : 1;       // B/Res1
            uint8_t reserved : 6; // reserved
            uint32_t destQP : 24; // Destination 0QP
        } __attribute__((__packed__));

        uint32_t u;
    } m_ub;

    union {
        struct
        {
            uint8_t ackQ : 1;     // Ackknowledge request
            uint8_t reserved : 7; // reserved
            uint32_t psn : 24;    // packet sequence number
        } __attribute__((__packed__));

        uint32_t u;
    } m_uc;

}; // class RoCEv2Header

/**
 * \brief ACK extened transport header
 */
class AETHeader : public Header
{
  public:
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const override;

    AETHeader();

    virtual uint32_t GetSerializedSize(void) const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream& os) const override;

    enum SyndromeType
    {
        FC_DISABLED = 0b000, // flow control
        NACK = 0b011
    };

    SyndromeType GetSyndromeType() const;
    void SetSyndromeType(SyndromeType t);

    uint32_t GetMSN() const;
    void SetMSN(uint32_t msn);

  private:
    union {
        struct
        {
            uint8_t syndrome;
            uint32_t msn : 24;
        } __attribute__((__packed__));

        uint32_t u;
    } m_u;

}; // class AETHeader

/**
 * A combination of UDP header and RoCEv2 header.
 * This is useful when we want to peek RoCEv2 header from an IP packet.
 */
class UdpRoCEv2Header : public Header
{
  public:
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const override;

    UdpRoCEv2Header();

    virtual uint32_t GetSerializedSize(void) const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream& os) const override;

    void SetUdp(const UdpHeader& udp);
    const UdpHeader& GetUdp() const;

    void SetRoCE(const RoCEv2Header& rocev2);
    const RoCEv2Header& GetRoCE() const;

  private:
    UdpHeader m_udp;
    RoCEv2Header m_rocev2;

}; // UdpRoCEv2Header

} // namespace ns3

#endif // ROCEV2_HEADR_H
