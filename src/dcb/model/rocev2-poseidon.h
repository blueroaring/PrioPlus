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

#ifndef ROCEV2_POSEIDON_H
#define ROCEV2_POSEIDON_H

#include "rocev2-congestion-ops.h"

#include "ns3/data-rate.h"
#include "ns3/rocev2-header.h"

#include <map>

namespace ns3
{

class RoCEv2SocketState;

/**
 * The Poseidon implementation according to paper:
 * Wang, Weitao et al. “Poseidon: Efficient, Robust, and Practical Datacenter CC via Deployable
 * INT.” NSDI (2023). \url https://www.usenix.org/conference/nsdi23/presentation/wang-weitao
 */
class RoCEv2Poseidon : public RoCEv2CongestionOps
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    RoCEv2Poseidon();
    RoCEv2Poseidon(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2Poseidon() override;

    /**
     * After configuring the Poseidon, call this function.
     */
    void SetReady() override;

    /**
     * When the sender sending out a packet, recover a timeslot into the map and add an empty
     * Poseidon header into packet.
     */
    void UpdateStateSend(Ptr<Packet> packet) override;
    /**
     * When the receiver generating an ACK, move the Poseidon header from packet to ack.
     */
    void UpdateStateWithGenACK(Ptr<Packet> packet, Ptr<Packet> ack) override;

    /**
     * When the sender receiving an ACK, do what Poseidon does.
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
        std::vector<std::pair<Time, uint16_t>> vMpt; //!< Record the maximum per-hop delay target

        // Recorder function of the detailed statistics
        void RecordMpt(uint16_t mpt);
        // Collect the statistics and check if the statistics is correct
        void CollectAndCheck();

        // No getter for simplicity
    };

    inline std::shared_ptr<RoCEv2CongestionOps::Stats> GetStats() const override
    {
        return m_stats;
    }

  private:
    /**
     * Initialize the state.
     */
    void Init();

    std::shared_ptr<Stats> m_stats; //!< Statistics

    double m_m;   //!< m in paper. The “step” when updating the rate.
    uint32_t m_k; //!< k in paper. The minimum target delay in microseconds.
    uint32_t m_p; //!< p in paper. The tune parameter.

    std::map<uint32_t, Time> m_tsMap; //!< To store timeslot of each PSN.

    double m_mptOperator; // p / ln(max_rate / min_rate). To reduce computation.

    bool m_canDecrease;       //!< Whether the sender can decrease the rate.
    Timer m_canDecreaseTimer; //!< Timer to set m_canDecrease to true.

    inline void SetCanDecrease()
    {
        m_canDecrease = true;
    }

}; // class RoCEv2Poseidon

} // namespace ns3

#endif // POSEIDON_H