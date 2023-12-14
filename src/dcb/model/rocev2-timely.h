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

#ifndef ROCEV2_TIMELY_H
#define ROCEV2_TIMELY_H

#include "rocev2-congestion-ops.h"

#include "ns3/data-rate.h"
#include "ns3/rocev2-header.h"

namespace ns3
{

class RoCEv2SocketState;

/**
 * The DCQCN implementation according to paper:
 *   Zhu, Yibo, et al. "Congestion control for large-scale RDMA deployments." ACM SIGCOMM.
 *   \url https://dl.acm.org/doi/abs/10.1145/2829988.2787484
 */
class RoCEv2Timely : public RoCEv2CongestionOps
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    RoCEv2Timely();
    RoCEv2Timely(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2Timely();

    // void SetRateAIRatio(double ratio);
    // void SetRateHyperAIRatio(double ratio);

    /**
     * After configuring the Timely, call this function.
     */
    void SetReady() override;

    /**
     * When the sender sending out a packet, add a SeqTsHeader into packet, in order to store
     * timestamp.
     */
    void UpdateStateSend(Ptr<Packet> packet) override;

    /**
     * When the receiver generating an ACK, move the SeqTsHeade from packet to ack.
     */
    void UpdateStateWithGenACK(Ptr<Packet> packet, Ptr<Packet> ack) override;

    /**
     * When the sender receiving an ACK, do what Timely does.
     */
    void UpdateStateWithRcvACK(Ptr<Packet> ack,
                               const RoCEv2Header& roce,
                               const uint32_t senderNextPSN) override;

    std::string GetName() const override;

  private:
    /**
     * Initialize the state.
     */
    void Init();

    double m_alpha;    //!< α in paper. EWMA weight parameter
    double m_mdFactor; //!< β in paper. Multiplicative Decrement factor

    double m_raiRatio; //!< RateAI / link rate for additive increase

    uint32_t m_incStage; //!< To count how many times that gradient<0

    bool m_haiMode;      //!< whether in hyperactive-increase mode
    uint32_t m_maxStage; //!< N in paper. Default to 5.

    Time m_prevRtt; //!< prev_rtt in paper.
    Time m_rttDiff; //!< rtt_diff in paper.

    Time m_minRtt; //!< minRTT in paper.
    Time m_tLow;   //!< Tlow in paper.
    Time m_tHigh;  //!< Thigh in paper.

}; // class RoCEv2Timely

} // namespace ns3

#endif // TIMELY_H
