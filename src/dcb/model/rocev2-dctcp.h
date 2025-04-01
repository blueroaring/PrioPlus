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
 * Author: F.Y. Xue <xue.fyang@foxmail.com>
 */

#ifndef ROCEV2_DCTCP_H
#define ROCEV2_DCTCP_H

#include "rocev2-congestion-ops.h"

#include "ns3/data-rate.h"
#include "ns3/queue-size.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rocev2-header.h"
#include "ns3/string.h"

#include <deque>
#include <map>

namespace ns3
{

class RoCEv2SocketState;

/**
 * The DCTCP implementation according to paper:
 *   Mohammad Alizade, et al. "Data center TCP (DCTCP)."
 *   \url https://dl.acm.org/doi/10.1145/1851275.1851192
 */
class RoCEv2Dctcp : public RoCEv2CongestionOps
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    RoCEv2Dctcp();
    RoCEv2Dctcp(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2Dctcp() override;

    /**
     * After configuring the Timely, call this function.
     */
    void SetReady() override;

    /**
     * When the sender receiving an ACK.
     */
    virtual void UpdateStateWithRcvACK(Ptr<Packet> ack,
                                       const RoCEv2Header& roce,
                                       const uint32_t senderNextPSN) override;

    std::string GetName() const override;

  protected:
    /**
     * \brief slow start, cwnd +=1 per ACK
     */
    void SlowStart();

    /**
     * \brief congestion avoidance, cwnd does AI per RTT.
     * Note that since there is no packet loss in RoCEv2, it won't do MD.
     * XXX if IRN is implemented, it should do MD.
     */
    void CongestionAvoidance(const uint32_t ackSeq, const uint32_t senderNextPSN);

    uint32_t m_alphaLastUpdateSeq; //!< alphaLastUpdateSeq, to make sure that alpha is updated only
                                   //!< once per RTT.
    uint32_t m_cwndLastReduceSeq;  //!< cwndLastReduceSeq, to make sure that cwnd is reduced only
                                   //!< once per RTT.
    uint32_t m_ecnCnt;             //!< ECN count during a RTT.
    uint32_t m_allCnt;             //!< All count during a RTT.

    double m_alpha; //!< alpha in paper.

    double m_gain;     //!< g in paper. Estimation gain.
    double m_raiRatio; //!< RateAI ratio. Additive increase parameter.
    bool m_doSS;       //!< If DCTCP allows Slow Start

    enum StartPattern
    {
        SLOW_START,
        FIXED_START
    };

    uint32_t m_startPattern; //!< Start pattern. SlowStart or FullSpeed

    uint64_t m_cwndAi; //!< Calculated from RateAI ratio.

    /**
     * \brief Update DCTCP's alpha based on ECN ratio.
     */
    virtual void UpdateAlpha(double ecnRatio);

    virtual void ReduceWindow();

    std::shared_ptr<Stats> m_stats; //!< Statistics

  private:
    /**
     * Initialize the state.
     */
    void Init();

}; // class RoCEv2Dctcp
} // namespace ns3

#endif // ROCEV2_DCTCP_H
