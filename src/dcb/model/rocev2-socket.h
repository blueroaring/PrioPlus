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
class IrnHeader;

enum RoCEv2RetxMode : uint8_t
{
    GBN, // Go Back N
    IRN  // Improved RoCE NIC, see Revisiting Network Support for RDMA by Mittal et al.
};

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

    // No default constructor, as we need the sendCb
    DcbTxBuffer(Callback<void> sendCb);

    typedef std::deque<DcbTxBufferItem>::const_iterator DcbTxBufferItemI;

    void Push(uint32_t psn,
              RoCEv2Header header,
              Ptr<Packet> payload,
              Ipv4Address daddr,
              Ptr<Ipv4Route> route);
    /**
     * \brief Get the front item of the buffer.
     */
    const DcbTxBufferItem& Front() const;
    // We do not need pop, to avoid mistakes
    /**
     * \brief Remove the item to given PSN.
     * \param psn PSN which has been acknowledged.
     */
    void AcknowledgeTo(uint32_t psn);
    /**
     * \brief Acknowledge the given PSN.
     * In this function, we just set m_acked[psn] to true.
     *
     * \param psn PSN which has been acknowledged.
     */
    void Acknowledge(uint32_t psn);
    /**
     * \brief Check if some packets can be released.
     */
    void CheckRelease();
    /**
     * \brief Get the next item to be sent.
     */
    const DcbTxBufferItem& PeekNextShouldSent();
    /**
     * \brief Remove the next item to be sent.
     */
    const DcbTxBufferItem& PopNextShouldSent();
    /**
     * \brief Return the number of packets in the buffer.
     */
    uint32_t Size() const;
    /**
     * \brief Return the number of packets should be sent.
     */
    uint32_t TotalSize() const;
    /**
     * \brief Number of packets left to be sent
     */
    uint32_t GetSizeToBeSent() const;
    /**
     * \brief Add a PSN to the txQueue.
     * Called when a packet with the PSN is desired to be retransmit.
     */
    void Retransmit(uint32_t psn);
    /**
     * \brief Retansmit all packets from the given PSN, i.e., go back N.
     * \return the number of packets to be retransmitted.
     */
    void RetransmitFrom(uint32_t psn);
    /**
     * \brief Get the PSN of the front item in the buffer.
     */
    uint32_t GetFrontPsn() const;
    /**
     * \brief Whether the buffer has gap.
     */
    bool HasGap() const;

    DcbTxBufferItemI FindPSN(uint32_t psn) const;
    DcbTxBufferItemI End() const;

  private:
    /**
     * \brief The SendPendingPacket() call back, called every time the tx buffer has new packet to
     * send.
     */
    Callback<void> m_sendCb;

    std::deque<DcbTxBufferItem> m_buffer;
    uint32_t m_frontPsn; // PSN of the front item in the buffer. Only increase when acked.
    std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>>
        m_txQueue;             // PSN of the items to be sent
    std::vector<bool> m_acked; // Whether the packet with the PSN is acked, serve as a bitmap in
                               // retx mode IRN. The index is the PSN.
    uint32_t m_maxAckedPsn;    // The max PSN which has been acknowledged, used to detect gap.

}; // class DcbTxBuffer

class DcbRxBuffer : public Object
{
  public:
    struct DcbRxBufferItem
    {
        // No default constructor
        DcbRxBufferItem(Ipv4Header ipv4H, RoCEv2Header roceH, Ptr<Packet> p);

        Ipv4Header m_ipv4H;
        RoCEv2Header m_roceH;
        Ptr<Packet> m_payload;
    }; // class DcbTxBufferItem

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    // No default constructor
    DcbRxBuffer(Callback<void, Ptr<Packet>, Ipv4Header, uint32_t, Ptr<Ipv4Interface>> forwardCb,
                Ptr<Ipv4Interface> incomingInterface,
                RoCEv2RetxMode retxMode);

    /**
     * \brief Add a packet into the buffer.
     */
    void Add(uint32_t psn, Ipv4Header ipv4, RoCEv2Header roce, Ptr<Packet> payload);

    uint32_t GetExpectedPsn() const;

  private:
    /**
     * \brief The ForwardUp() call back, called when has packet to be forwarded up.
     */
    Callback<void, Ptr<Packet>, Ipv4Header, uint32_t, Ptr<Ipv4Interface>> m_forwardCb;
    std::map<uint32_t, DcbRxBufferItem> m_buffer;
    uint32_t m_expectedPsn; // PSN of the front item in the buffer. Only increase when ForwardUp.
    Ptr<Ipv4Interface> m_forwardInterface; // Interface to forward the packet up
    RoCEv2RetxMode m_retxMode;
}; // class DcbRxBuffer

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

    // \brief Structure that keeps the IbcSendScheduler statistics
    class Stats

    {
      public:
        // constructor
        Stats();

        uint32_t nTotalSizePkts; //<! Data size sent by upper layer
        uint64_t nTotalSizeBytes;
        uint32_t nTotalSentPkts; //<! Data pkts sent to lower layer, including retransmission
        uint64_t nTotalSentBytes;
        uint32_t nTotalDeliverPkts; //<! Data pkts successfully delivered to peer upper layer
        uint64_t nTotalDeliverBytes;
        /**
         * No need to record the number of lost packets, as it can be calculated by
         * nTotalSentPkts - nTotalDeliverPkts. And it is often not accurate, as the lost packets can
         * not be detected at sender in some cases.
         */
        uint32_t nRetxCount; //<! Number of retransmission
        Time tStart;
        Time tFinish;
        Time tFct;                //<! Flow completion time
        DataRate overallFlowRate; //<! overall rate, calculate by total size / (first msg arrive -
                                  // last msg finish)

        // Detailed sender statistics, only enabled if needed
        bool bDetailedSenderStats;
        std::vector<std::pair<Time, DataRate>> vCcRate; //<! Record the rate when changed
        std::vector<std::pair<Time, uint32_t>> vCcCwnd; //<! Record the cwnd when changed
        std::vector<Time> vRecvEcn;                     //<! Record the time when received ECN
        std::vector<std::pair<Time, uint32_t>>
            vSentPkt; //<! Record the packets' send time and size XXX (only payload now)

        // Detailed statistics for retx, only enabled if needed
        bool bDetailedRetxStats;
        std::vector<std::pair<Time, uint32_t>> vSentPsn;  //<! Record the packets' send time and PSN
        std::vector<std::pair<Time, uint32_t>> vAckedPsn; //<! Record the ack recv time and PSN
        std::vector<std::pair<Time, uint32_t>> vExpectedPsn; //<! Record the ack recv time and PSN

        // Recorder function of the detailed statistics
        void RecordCcRate(DataRate rate);
        void RecordCcCwnd(uint32_t cwnd);
        void RecordRecvEcn();
        void RecordSentPkt(uint32_t size);
        void RecordSentPsn(uint32_t psn);
        void RecordAckedPsn(uint32_t psn);
        void RecordExpectedPsn(uint32_t psn);

        // Collect the statistics and check if the statistics is correct
        void CollectAndCheck();

        // No getter for simplicity
    };

    std::shared_ptr<Stats> GetStats() const;

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
        DcbRxBuffer m_rxBuffer;

        FlowInfo(uint32_t dst, DcbRxBuffer rxBuffer)
            : dstQP(dst),
              nextPSN(0),
              receivedECN(false),
              m_rxBuffer(rxBuffer)
        {
        }

        uint32_t GetExpectedPsn() const
        {
            return m_rxBuffer.GetExpectedPsn();
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

    void GoBackN(uint32_t lostPSN);
    /**
     * \brief React to the ACK packet in IRN mode.
     */
    void IrnReactToAck(uint32_t expectedPSN);
    /**
     * \brief React to the NACK packet in IRN mode.
     */
    void IrnReactToNack(uint32_t expectedPSN, IrnHeader irnH);

    void ScheduleNextCNP(std::map<FlowIdentifier, FlowInfo>::iterator flowInfoIter,
                         Ipv4Header header);
    /**
     * \brief Check whether the given priority queue disc avaliable to buffer more packet.
     *
     * Get the size of the given priority queue disc, and check if it is lower than a threshold.
     * If so, return true, otherwise return false. In this way, sockets can avoid to overwhelm
     * the queue disc. As well as avoid to send too many uncontrolled packet in the queue disc.
     *
     * This function only work with dcb dev and pausable queue disc, if not, it will always return
     * true.
     */
    bool CheckQueueDiscAvaliable(uint8_t priority) const;
    /**
     * \brief Check whether the queue disc of control priority avaliable to buffer more packet.
     *
     * Throw fatal error if it is inavaliable as control packet should always be sent.
     *
     * This function only work with dcb dev and pausable queue disc, if not, it will always return
     * true.
     */
    void CheckControlQueueDiscAvaliable() const;

    /**
     * \brief Do forward up to upper layer.
     *
     * In this function, the UdpBasedSocket::ForwardUp() is called.
     */
    void DoForwardUp(Ptr<Packet> packet,
                     Ipv4Header header,
                     uint32_t port,
                     Ptr<Ipv4Interface> incomingInterface);

    // Time CalcTxTime (uint32_t bytes);

    std::shared_ptr<Stats> m_stats;

    Ptr<DcqcnCongestionOps> m_ccOps;    //!< DCQCN congestion control
    Ptr<RoCEv2SocketState> m_sockState; //!< DCQCN socket state
    DcbTxBuffer m_txBuffer;
    DataRate m_deviceRate;
    // bool m_isSending;
    EventId m_sendEvent; //!< Event id of the next send event

    uint32_t m_senderNextPSN;
    std::map<FlowIdentifier, FlowInfo> m_receiverFlowInfo;
    uint32_t m_psnEnd; //!< the last PSN + 1, used to check if flow completes

    Time m_flowStartTime;

    RoCEv2RetxMode m_retxMode;

}; // class RoCEv2Socket

/**
 * \brief Header for IRN.
 * It will be repleaced after AETH header in the NACK packet.
 * It only has acked PSN. As the expected PSN is in the AETH header.
 */
class IrnHeader : public Header
{
  public:
    IrnHeader();
    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId(void) const override;
    virtual uint32_t GetSerializedSize() const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream& os) const override;
    void SetAckedPsn(uint32_t psn);
    uint32_t GetAckedPsn() const;

  private:
    uint32_t m_ackedPsn;
}; // class IrnHeader

} // namespace ns3

//
#endif // ROCEV2_SOCKET_H
