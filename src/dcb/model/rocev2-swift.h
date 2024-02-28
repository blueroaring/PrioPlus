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

#ifndef ROCEV2_SWIFT_H
#define ROCEV2_SWIFT_H

#include "rocev2-congestion-ops.h"

#include "ns3/data-rate.h"
#include "ns3/rocev2-header.h"

#include <map>

namespace ns3
{

class RoCEv2SocketState;

/**
 * The Swift implementation according to paper:
 *   Radhika Mittal, et al. "SWIFT: Delay is Simple and Effective for Congestion Control in the
 * Datacenter." ACM SIGCOMM. \url https://dl.acm.org/doi/10.1145/3387514.34065910
 */
class RoCEv2Swift : public RoCEv2CongestionOps
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    RoCEv2Swift();
    RoCEv2Swift(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2Swift();

    // void SetRateAIRatio(double ratio);
    // void SetRateHyperAIRatio(double ratio);

    /**
     * After configuring the Swift, call this function.
     */
    void SetReady() override;

    /**
     * When the sender sending out a packet, recover a timeslot into the map.
     */
    void UpdateStateSend(Ptr<Packet> packet) override;

    /**
     * When the sender receiving an ACK, do what Swift does.
     */
    void UpdateStateWithRcvACK(Ptr<Packet> ack,
                               const RoCEv2Header& roce,
                               const uint32_t senderNextPSN) override;

    std::string GetName() const override;

    class Stats : public RoCEv2CongestionOps::Stats
    {
      public:
        // constructor
        Stats();

        // Detailed statistics, only enabled if needed
        bool bDetailedSenderStats;
        std::vector<std::pair<Time, bool>>
            vCcRateChange; //!< Record the time when the rate changes, true for increase, false for
                            //!< decrease

        // Recorder function of the detailed statistics
        void RecordCcRateChange(bool increase);
        // Collect the statistics and check if the statistics is correct
        void CollectAndCheck();

        // No getter for simplicity
    };

    inline std::shared_ptr<RoCEv2CongestionOps::Stats> GetStats() const
    {
        return m_stats;
    }

  private:
    /**
     * Initialize the state.
     */
    void Init();

    Time GetTargetDelay();

    std::shared_ptr<Stats> m_stats; //!< Statistics

    Time m_baseTarget; //!< Base target delay. Default to 25us.

    double m_mdFactor;    //!< β in paper. Multiplicative Decrement factor
    double m_maxMdFactor; //!< max_mdf in paper. Maximum Multiplicative Decrement factor

    double m_raiRatio; //!< Rate additive increase ratio.

    double m_range; //!< fs_range in paper. Default to 4.0.
    double m_gamma; //!< Used in GetTargetDelay() in order to simplify the formula.

    bool m_canDecrease; //!< Whether the sender can decrease the rate.

    std::map<uint32_t, Time> m_tsMap; //!< To store timeslot of each PSN.

    Timer m_canDecreaseTimer; //!< Timer to set m_canDecrease to true.

    inline void SetCanDecrease()
    {
        m_canDecrease = true;
    }

}; // class RoCEv2Swift

/**
 * The Swift Header. We don't use it in the simulation, but add it to the packet for accuracy.
 */
class SwiftHeader : public Header
{
  public:
    SwiftHeader()
        : m_ts(0),
          m_hopCount(0)

    {
    }

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("ns3::SwiftHeader")
                                .SetParent<Header>()
                                .SetGroupName("Dcb")
                                .AddConstructor<SwiftHeader>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override
    {
        return GetTypeId();
    }

    void Print(std::ostream& os) const override
    {
        os << "(hopCount=" << m_hopCount << " ts=" << m_ts << ")";
    }

    inline uint32_t GetSerializedSize() const override
    {
        return 9;
    }

    void Serialize(Buffer::Iterator start) const override
    {
        Buffer::Iterator i = start;
        i.WriteHtonU64(m_ts);
        i.WriteU8(m_hopCount);
    }

    uint32_t Deserialize(Buffer::Iterator start) override
    {
        Buffer::Iterator i = start;
        m_ts = i.ReadNtohU64();
        m_hopCount = i.ReadU8();
        return GetSerializedSize();
    }

  private:
    uint64_t m_ts;      //!< Timestamp
    uint8_t m_hopCount; //!< Hop count
};

} // namespace ns3

#endif // SWIFT_H
