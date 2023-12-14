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
#ifndef ROCEV2_CONGESTION_OPS_H
#define ROCEV2_CONGESTION_OPS_H

#include "ns3/data-rate.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/rocev2-header.h"
#include "ns3/timer.h"
#include "ns3/traced-value.h"

namespace ns3
{

class RoCEv2SocketState;

class RoCEv2CongestionOps : public Object
{
  public:
    /**
     * \brief Get the type ID.
     *
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    RoCEv2CongestionOps();
    RoCEv2CongestionOps(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2CongestionOps();

    void SetStopTime(Time stopTime);

    void SetSockState(Ptr<RoCEv2SocketState> sockState);

    /**
     ********** VIRTUAL FUNCTIONS**********
     * implemented by subclasses.
     */

    /**
     * \brief Get the name of the congestion control algorithm. All subclasses
     * must implement this function.
     *
     * \return A string identifying the name
     */
    virtual std::string GetName() const = 0;

    /**
     * \brief When the sender sending out a packet, update the state if needed.
     *
     * Do nothing in this class. And implementions in subclasses is not necessary.
     */
    virtual void UpdateStateSend(Ptr<Packet> packet)
    {
    }

    /**
     * \brief When receiving a CNP, update the state if needed.
     *
     * Do nothing in this class. And implementions in subclasses is not necessary.
     */
    virtual void UpdateStateWithCNP()
    {
    }

    /**
     * \brief When receiving an ACK, update the state if needed.
     *
     * Do nothing in this class. And implementions in subclasses is not necessary.
     *
     * Note: This function has the pointer of ACK packet as parameter, which is used after calling
     * this function in RoCEv2Socket::ForwardUp. So, the ACK packet should be carefully modified in
     * this.
     */
    virtual void UpdateStateWithRcvACK(Ptr<Packet> ack,
                                       const RoCEv2Header& roce,
                                       const uint32_t senderNextPSN)
    {
    }

    /**
     * \brief When Generating an ACK, update the state if needed.
     *
     * Do nothing in this class. And implementions in subclasses is not necessary.
     *
     * Note: This function has the pointer of packet as parameter, which is used after calling
     * this function in RoCEv2Socket::HandleDataPacket. So, the packet should be carefully modified
     * in this.
     *
     * \param packet The packet received.
     * \param ack The ACK packet to be sent.
     */
    virtual void UpdateStateWithGenACK(Ptr<Packet> packet, Ptr<Packet> ack)
    {
    }

    /**
     * \brief When RoCEv2Socket is binded to netdevice, start some initialization if needed.
     *
     * Do nothing in this class. And implementions in subclasses is not necessary.
     *
     * Note: In subclass's constructor, the sockState is not set yet. So, any initialization with
     * sockState should be done in this function.
     */
    virtual void SetReady()
    {
    }

    /**
     * \brief Get headersize.
     */
    virtual uint32_t GetHeaderSize()
    {
        return m_headerSize;
    }

  protected:
    /**
     * \return true if current time is not over stopTime.
     */
    bool CheckStopCondition();
    Ptr<RoCEv2SocketState> m_sockState;
    Time m_stopTime;
    uint32_t m_headerSize;

    /**
     ********** RATE-BASED **********
     */

    /**
     * \brief Check if rateRatio is less than 1. and greater than m_minRateRatio.
     * if not, correct it.
     * \return corrected rateRatio.
     */
    double CheckRateRatio(double rateRatio);
    double m_curRateRatio; //!< current rate
    double m_minRateRatio; //!< minimum rate

    // TODO add window-based
};
} // namespace ns3

#endif
