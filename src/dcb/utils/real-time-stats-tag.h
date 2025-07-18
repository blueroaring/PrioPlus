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

#ifndef REAL_TIME_STATS_TAG_H
#define REAL_TIME_STATS_TAG_H

#include "ns3/tag.h"

namespace ns3
{

class RealTimeStatsTag : public Tag
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer buf) const override;
    void Deserialize(TagBuffer buf) override;
    void Print(std::ostream& os) const override;
    RealTimeStatsTag();

    /**
     * Constructs a RealTimeStatsTag with the given seq
     */
    RealTimeStatsTag(uint32_t pktSeq, uint64_t tArriveNs);
    /**
     * Gets the pkt seq for the tag
     * \returns packet seq for this tag
     */
    uint32_t GetPktSeq() const;
    /**
     * Get the arrival time of the packet
     */
    uint64_t GetTArriveNs() const;
    /**
     * Sets the arrival time of the packet
     */
    void SetTArriveNs(uint64_t tArriveNs);
    /**
     * Get the tx time of the packet
     */
    uint64_t GetTTxNs() const;
    /**
     * Sets the tx time of the packet
     */
    void SetTTxNs(uint64_t tTxNs);

  private:
    uint32_t m_pktSeq; //!< Sequence number of the packet
    uint64_t m_tArriveNs; //!< Arrival (generated by app) time of the packet
    uint64_t m_tTxNs; //!< Tx (sent out form l4) time of the packet
};

} // namespace ns3

#endif /* REAL_TIME_STATS_TAG_H */
