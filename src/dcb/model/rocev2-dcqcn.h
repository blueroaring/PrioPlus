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

#ifndef ROCEV2_DCQCN_H
#define ROCEV2_DCQCN_H

#include "rocev2-congestion-ops.h"

namespace ns3
{

class RoCEv2SocketState;

/**
 * The DCQCN implementation according to paper:
 *   Zhu, Yibo, et al. "Congestion control for large-scale RDMA deployments." ACM SIGCOMM.
 *   \url https://dl.acm.org/doi/abs/10.1145/2829988.2787484
 */
class RoCEv2Dcqcn : public RoCEv2CongestionOps
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    RoCEv2Dcqcn();
    RoCEv2Dcqcn(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2Dcqcn();

    void SetRateAIRatio(double ratio);
    void SetRateHyperAIRatio(double ratio);

    /**
     * After configuring the DCQCN, call this function to start the timer.
     */
    void SetReady() override;

    /**
     * Update socket state when receiving a CNP.
     */
    void UpdateStateWithCNP() override;

    /**
     * When the sender sending out a packet, update the state if needed.
     */
    void UpdateStateSend(Ptr<Packet> packet) override;

    std::string GetName() const override;

    inline std::shared_ptr<RoCEv2CongestionOps::Stats> GetStats() const
    {
        // This has no Stats now
        return nullptr;
    }

  private:
    void UpdateAlpha();

    void RateTimerTriggered();

    void UpdateRate();

    void Init();

    // const Ptr<RoCEv2SocketState> m_sockState;
    double m_alpha;
    double m_g;
    double m_raiRatio;  //!< RateAI / link rate for additive increase
    double m_hraiRatio; //!< Hyper rate AI / link rate for hyper additive increase

    Timer m_alphaTimer; //!< update alpha if haven't received CNP for a configured time
    Timer m_rateTimer;
    Time m_alphaTimerDelay;
    Time m_rateTimerDelay;

    uint32_t m_bytesThreshold;
    uint32_t m_bytesCounter;
    uint32_t m_rateUpdateIter;
    uint32_t m_bytesUpdateIter;
    uint32_t m_F;

    double m_targetRateRatio;
    // Time m_stopTime;

}; // class RoCEv2Dcqcn

} // namespace ns3

#endif // DCQCN_H
