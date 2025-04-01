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

#ifndef ROCEV2_D2TCP_H
#define ROCEV2_D2TCP_H

#include "rocev2-dctcp.h"

namespace ns3
{

class RoCEv2SocketState;

/**
 * The D2TCP implementation according to paper:
 *   PictureBalajee Vamanan, et al. "Deadline-aware datacenter tcp (D2TCP)."
 *   \url https://dl.acm.org/doi/10.1145/2377677.2377709
 */
class RoCEv2D2tcp : public RoCEv2Dctcp
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    RoCEv2D2tcp();
    RoCEv2D2tcp(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2D2tcp() override;

    /**
     * After configuring the Timely, call this function.
     */
    void SetReady() override;

    std::string GetName() const override;

  protected:
    virtual void ReduceWindow() override;

  private:
    /**
     * Initialize the state.
     */
    void Init();
    std::shared_ptr<Stats> m_stats; //!< Statistics
    double m_priorityFactor;        //!< Priority factor, using to calculate deadline
    Time m_deadline;                //!< Deadline
    Time m_startTime;               //!< Start time
    uint64_t m_totalSize;           //!< Total size

}; // class RoCEv2D2tcp
} // namespace ns3

#endif // ROCEV2_D2TCP_H
