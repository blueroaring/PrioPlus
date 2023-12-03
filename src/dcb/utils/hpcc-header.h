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
 * Author: F.Y. Xue <xue.fyang@foxmail.com>
 */

#ifndef HPCC_HEADER_H
#define HPCC_HEADER_H

#include "ns3/data-rate.h"
#include "ns3/header.h"
#include "ns3/packet.h"

namespace ns3
{

class IntHop
{
  public:
    constexpr static const uint32_t TSWidth = 24;
    constexpr static const uint32_t txBytesWidth = 20;
    constexpr static const uint32_t qLenWidth = 16;
    constexpr static const uint64_t lineRateValues[8]{
        100000000000lu, // 100Gbps, which is more frequent
        25000000000lu,  // 25Gbps
        50000000000lu,  // 50Gbps
        200000000000lu, // 200Gbps
        400000000000lu  // 400Gbps
    };

    union {
        struct
        {
            uint64_t m_lineRate : 64 - TSWidth - txBytesWidth - qLenWidth;
            uint64_t m_TS : TSWidth;
            uint64_t m_txBytes : txBytesWidth;
            uint64_t m_qLen : qLenWidth;
        } __attribute__((__packed__));

        uint64_t m_buf;
    };

    constexpr static const uint32_t byteUnit = 128;
    constexpr static const uint32_t qLenUnit = 80;

    IntHop()
        : m_buf(0)
    {
    }

    IntHop(uint64_t time, uint64_t bytes, uint64_t qlen, DataRate rate)
    {
        Set(time, bytes, qlen, rate);
    }

    DataRate GetLineRate() const
    {
        uint64_t lineRateUint = lineRateValues[m_lineRate];
        return DataRate(lineRateUint);
    }

    uint64_t GetBytes() const
    {
        return (uint64_t)m_txBytes * byteUnit;
    }

    uint64_t GetQlen() const
    {
        return (uint64_t)m_qLen * qLenUnit;
    }

    uint64_t GetTimeSlot() const
    {
        return m_TS;
    }

    void Set(uint64_t time, uint64_t bytes, uint64_t qlen, DataRate rate)
    {
        m_TS = time;
        m_txBytes = bytes;
        m_qLen = qlen;
        uint64_t bitrate = rate.GetBitRate();
        uint64_t i = 0;
        for (; i < 8; i++)
        {
            if (lineRateValues[i] == bitrate)
            {
                m_lineRate = i;
                break;
            }
        }
        if (i == 8)
        {
            NS_FATAL_ERROR("[ERROR] cannot find " << rate << " in lineRateValues");
        }
    }

    uint64_t GetBytesDelta(IntHop& b) const
    {
        if (m_txBytes >= b.m_txBytes)
        {
            return (m_txBytes - b.m_txBytes) * byteUnit;
        }
        else
            /*
             * Only used for switch to substract ack's txBytes
             * from the privious one, which means the ONLY reason
             * that m_txBytes < b.m_txBytes happens when m_txBytes
             * is larger than (1 << txBytesWidth) - 1 and it overflows.
             */
            return (m_txBytes + (1 << txBytesWidth) - b.m_txBytes) * byteUnit;
    }

    uint64_t GetTimeDelta(IntHop& b) const
    {
        if (m_TS >= b.m_TS)
            return m_TS - b.m_TS;
        else
            // Same reason
            return m_TS + (1 << TSWidth) - b.m_TS;
    }
};

class HpccHeader : public Header
{
  public:
    HpccHeader();

    constexpr static const uint32_t MAX_HOP = 5;

    /**
     * \brief Push one IntHop in the header.
     */
    void PushHop(uint64_t time, uint64_t bytes, uint64_t qlen, DataRate rate);

    uint16_t GetNHop() const
    {
        return m_nHop;
    }

    uint16_t GetPathID() const
    {
        return m_pathID;
    }

    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId(void) const override;
    virtual uint32_t GetSerializedSize() const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream& os) const override;

  private:
    IntHop m_intHops[MAX_HOP];

    union {
        uint16_t m_infoBuf;

        struct
        {
            uint16_t m_pathID : 12; // TODO Now we don't considering changing path
            uint16_t m_nHop : 4;
        } __attribute__((__packed__));
    };
};

} // namespace ns3
#endif
