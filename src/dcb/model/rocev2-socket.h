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

#ifndef ROCEV2_SOCKET_H
#define ROCEV2_SOCKET_H

#include "dcqcn.h"
#include "udp-based-socket.h"

#include "ns3/ipv4-address.h"
#include "ns3/rocev2-header.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class DcqcnCongestionOps;
class RoCEv2SocketState;

class DcbTxBuffer : public Object
{
  public:
    struct DcbTxBufferItem
    {
        DcbTxBufferItem(uint32_t psn,
                        RoCEv2Header header,
                        Ptr<Packet> p,
                        Ipv4Address daddr,
                        Ptr<Ipv4Route> route);

        uint32_t m_psn;
        RoCEv2Header m_header;
        Ptr<Packet> m_payload;
        Ipv4Address m_daddr;
        Ptr<Ipv4Route> m_route;

    }; // class DcbTxBufferItem

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    DcbTxBuffer();

    typedef std::deque<DcbTxBufferItem>::const_iterator DcbTxBufferItemI;

    void Push(uint32_t psn,
              RoCEv2Header header,
              Ptr<Packet> payload,
              Ipv4Address daddr,
              Ptr<Ipv4Route> route);
    const DcbTxBufferItem& Front() const;
    DcbTxBufferItem Pop();
    const DcbTxBufferItem& GetNextShouldSent();
    uint32_t Size() const;
    /**
     * Number of packets left to be sent
     */
    uint32_t GetSizeToBeSent() const;

    DcbTxBufferItemI FindPSN(uint32_t psn) const;
    DcbTxBufferItemI End() const;

  private:
    std::deque<DcbTxBufferItem> m_buffer;
    uint32_t m_sentIdx; // Idx to send now, which is according to the buffer but not PSN

}; // class DcbTxBuffer

class RoCEv2SocketState : public Object
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    RoCEv2SocketState();

    inline void SetRateRatioPercent(double ratio)
    {
        m_rateRatio = ratio;
    }

    inline double GetRateRatioPercent() const
    {
        return m_rateRatio;
    }

  private:
    /**
     * Instead of directly store sending rate here, we store a rate ratio.
     * rateRatio = target sending rate / link rate * 100.0 .
     * In this way, this class is totally decoupled with others.
     */
    double m_rateRatio;

}; // class RoCEv2SocketState

class RoCEv2Socket : public UdpBasedSocket
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    RoCEv2Socket();
    ~RoCEv2Socket();

    virtual int Bind() override;
    virtual void BindToNetDevice(Ptr<NetDevice> netdevice) override;

    int BindToLocalPort(uint32_t port);

    virtual void FinishSending() override;

    void SetStopTime(Time stopTime); // for DCQCN

    Time GetFlowStartTime() const;

  protected:
    virtual void DoSendTo(Ptr<Packet> p, Ipv4Address daddr, Ptr<Ipv4Route> route) override;

    /**
     * \brief Try to send a pending packet.
     * 
     * Try to send a pending packet. Usually called when a new packet can be sent.
     * The function will check if it is allowed to send a packet, and if so, 
     * call DoSendDataPacket to send a data packet.
     */
    void SendPendingPacket();
    
    /**
     * \brief Actually send out a data packet.
     * 
     * The packet to be sent is controled by m_buffer. This function just call m_buffer
     * and send the packet out.
     */
    void DoSendDataPacket(const DcbTxBuffer::DcbTxBufferItem& item);

    virtual void ForwardUp(Ptr<Packet> packet,
                           Ipv4Header header,
                           uint32_t port,
                           Ptr<Ipv4Interface> incomingInterface) override;

  private:
    struct FlowInfo // for receiver
    {
        uint32_t dstQP;
        uint32_t nextPSN;
        bool receivedECN;
        EventId lastCNPEvent;

        FlowInfo(uint32_t dst)
            : dstQP(dst),
              nextPSN(0),
              receivedECN(false)
        {
        }
    };

    typedef std::pair<Ipv4Address, uint32_t> FlowIdentifier;

    RoCEv2Header CreateNextProtocolHeader();
    void HandleACK(Ptr<Packet> packet, const RoCEv2Header& roce);
    void HandleDataPacket(Ptr<Packet> packet,
                          Ipv4Header header,
                          uint32_t port,
                          Ptr<Ipv4Interface> incomingInterface,
                          const RoCEv2Header& roce);
    void GoBackN(uint32_t lostPSN) const;
    void ScheduleNextCNP(std::map<FlowIdentifier, FlowInfo>::iterator flowInfoIter,
                         Ipv4Header header);

    // Time CalcTxTime (uint32_t bytes);

    Ptr<DcqcnCongestionOps> m_ccOps;    //!< DCQCN congestion control
    Ptr<RoCEv2SocketState> m_sockState; //!< DCQCN socket state
    DcbTxBuffer m_buffer;
    DataRate m_deviceRate;
    // bool m_isSending;
    EventId m_sendEvent; //!< Event id of the next send event

    uint32_t m_senderNextPSN;
    std::map<FlowIdentifier, FlowInfo> m_receiverFlowInfo;
    uint32_t m_psnEnd; //!< the last PSN + 1, used to check if flow completes

    Time m_flowStartTime;

}; // class RoCEv2Socket

} // namespace ns3

//
#endif // ROCEV2_SOCKET_H
