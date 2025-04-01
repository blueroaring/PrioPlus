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

#ifndef ROCEV2_LEDBAT_H
#define ROCEV2_LEDBAT_H

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
class LedbatHeader;

/**
 * The Ledbat implementation according to `src/internet/model/tcp-ledbat`
 * and the RFC 6817[https://datatracker.ietf.org/doc/rfc6817/]. This implementation is used for
 * testing LEDBAT's algo in RoCEv2.
 */
class RoCEv2Ledbat : public RoCEv2CongestionOps
{
  public:
    /**
     * Get the type ID.
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    RoCEv2Ledbat();
    RoCEv2Ledbat(Ptr<RoCEv2SocketState> sockState);
    ~RoCEv2Ledbat() override;

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
     * Update socket state when receiving a CNP. Since no packet loss in RoCEv2, CNP signal will be
     * adjusted as loss.
     */
    void UpdateStateWithCNP() override;

    /**
     * When the receiver generating an ACK, move the SeqTsHeade from packet to ack.
     */
    void UpdateStateWithGenACK(Ptr<Packet> packet, Ptr<Packet> ack) override;

    /**
     * When the sender receiving an ACK.
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

    /**
     * \brief slow start, cwnd +=1 per ACK
     */
    void SlowStart();

    /**
     * \brief congestion avoidance, cwnd does AI per RTT.
     * Note that since there is no packet loss in RoCEv2, it won't do MD.
     * XXX if IRN is implemented, it should do MD.
     */
    void CongestionAvoidance();

    /**
     * \brief The state of LEDBAT. If LEDBAT is not in VALID_OWD state, it falls to
     *        default congestion ops.
     */
    // XXX not used because slow start is not implemented, and LEDBAT_VALID_OWD is always true.
    //  enum State : uint32_t
    //  {
    //      LEDBAT_VALID_OWD = (1 << 1), //!< If valid timestamps are present
    //      LEDBAT_CAN_SS = (1 << 3) //!< If LEDBAT allows Slow Start
    //  };

    /**
     *\brief Buffer structure to store delays
     */
    struct OwdCircBuf
    {
        std::vector<Time> buffer; //!< Vector to store the delay
        uint32_t min;             //!< The index of minimum value
    };

    /**
     * \brief Initialise a new buffer
     *
     * \param buffer The buffer to be initialised
     */
    void InitCircBuf(OwdCircBuf& buffer);

    /// Filter function used by LEDBAT for current delay
    typedef Time (*FilterFunction)(OwdCircBuf&);

    /**
     * \brief Return the minimum delay of the buffer
     *
     * \param b The buffer
     * \return The minimum delay
     */
    static Time MinCircBuf(OwdCircBuf& b);

    /**
     * \brief Return the value of current delay
     *
     * \param filter The filter function
     * \return The current delay
     */
    Time CurrentDelay(FilterFunction filter);

    /**
     * \brief Add new delay to the buffers
     *
     * \param cb The buffer
     * \param owd The new delay
     * \param maxlen The maximum permitted length
     */
    void AddDelay(OwdCircBuf& cb, Time owd, uint32_t maxlen);

    std::shared_ptr<Stats> m_stats; //!< Statistics
    Time m_target;                  //!< Target Queue Delay
    double m_gain;                  //!< GAIN value from RFC
    uint32_t m_noiseFilterLen;      //!< Length of current delay buffer
    // Time m_lastRollover;       //!< Timestamp of last added delay
    // int32_t m_sndCwndCnt;      //!< The congestion window addition parameter
    OwdCircBuf m_noiseFilter; //!< Buffer to store the current delay
    // uint32_t m_flag;           //!< LEDBAT Flag
    uint32_t m_minCwnd;               //!< Minimum cWnd value mentioned in RFC 6817
    std::map<uint32_t, Time> m_tsMap; //!< To store timeslot of each PSN.

    bool m_doSS;        //!< If LEDBAT allows Slow Start
    bool m_canDecrease; //!< If LEDBAT can do MD
    Timer m_canDecreaseTimer;
    Time m_lastRtt; //!< The last RTT recorded

    inline void SetCanDecrease()
    {
        m_canDecrease = true;
    }

}; // class RoCEv2Ledbat

class LedbatHeader : public Header
{
    // Ledbat Header with only one Timestamp
  public:
    LedbatHeader()
        : m_ts(Simulator::Now().GetNanoSeconds())
    {
    }

    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::LedbatHeader")
                                .SetParent<Header>()
                                .SetGroupName("Dcb")
                                .AddConstructor<LedbatHeader>();
        return tid;
    }

    TypeId GetInstanceTypeId(void) const override
    {
        return GetTypeId();
    }

    inline uint32_t GetSerializedSize(void) const override
    {
        return 8;
    }

    void Serialize(Buffer::Iterator start) const override
    {
        Buffer::Iterator i = start;
        i.WriteHtonU64(m_ts);
    }

    uint32_t Deserialize(Buffer::Iterator start) override
    {
        Buffer::Iterator i = start;
        m_ts = i.ReadNtohU64();
        return GetSerializedSize();
    }

    void Print(std::ostream& os) const override
    {
        os << "time=" << NanoSeconds(m_ts).As(Time::S) << "";
    }

    Time GetTs() const
    {
        return NanoSeconds(m_ts);
    }

    uint64_t m_ts; //!< Timestamp
};

} // namespace ns3

#endif // ROCEV2_LEDBAT_H
