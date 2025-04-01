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

#ifndef ROCEV2_NOCC_H
#define ROCEV2_NOCC_H

#include "rocev2-congestion-ops.h"

namespace ns3
{

class RoCEv2SocketState;

/**
 * This class implements a congestion control algorithm that controls nothing.
 */
class RoCEv2Nocc : public RoCEv2CongestionOps
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::RoCEv2Nocc")
                                .SetParent<RoCEv2CongestionOps>()
                                .AddConstructor<RoCEv2Nocc>()
                                .SetGroupName("Dcb")
                                .AddAttribute("FairShare",
                                              "Set the rate to the fair share",
                                              BooleanValue(false),
                                              MakeBooleanAccessor(&RoCEv2Nocc::m_fairShare),
                                              MakeBooleanChecker())
                                .AddAttribute("TargetInflight",
                                              "Set the target inflight",
                                              DoubleValue(1.0),
                                              MakeDoubleAccessor(&RoCEv2Nocc::m_targetInflight),
                                              MakeDoubleChecker<double>());
        return tid;
    }

    RoCEv2Nocc()
        : RoCEv2CongestionOps(std::make_shared<Stats>()),
          m_stats(std::dynamic_pointer_cast<Stats>(RoCEv2CongestionOps::m_stats))
    {
        Init();
    }

    RoCEv2Nocc(Ptr<RoCEv2SocketState> sockState)
        : RoCEv2CongestionOps(sockState, std::make_shared<Stats>()),
          m_stats(std::dynamic_pointer_cast<Stats>(RoCEv2CongestionOps::m_stats))
    {
        Init();
    }

    std::string GetName() const override
    {
        return "NoCC";
    }

    inline std::shared_ptr<RoCEv2CongestionOps::Stats> GetStats() const
    {
        // This has no Stats now
        return nullptr;
    }

    void Init()
    {
        m_fairShare = false;
        RegisterCongestionType(GetTypeId());
    }

    virtual void SetReady() override;
    
    private:
    std::shared_ptr<Stats> m_stats;
    bool m_fairShare;
    double m_targetInflight;

}; // class RoCEv2Nocc

} // namespace ns3

#endif // NOCC_H
