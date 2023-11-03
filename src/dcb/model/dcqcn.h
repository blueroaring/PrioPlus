/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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

#ifndef DCQCN_H
#define DCQCN_H

#include "ns3/data-rate.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/timer.h"

namespace ns3 {

class RoCEv2SocketState;

/**
 * The DCQCN implementation according to paper:
 *   Zhu, Yibo, et al. "Congestion control for large-scale RDMA deployments." ACM SIGCOMM.
 *   \url https://dl.acm.org/doi/abs/10.1145/2829988.2787484
 */
class DcqcnCongestionOps : public Object
{
public:

  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  DcqcnCongestionOps (Ptr<RoCEv2SocketState> sockState);
  ~DcqcnCongestionOps ();
  
  void SetRateAIRatio (double ratio);
  void SetRateHyperAIRatio (double ratio);

  /**
   * After configuring the DCQCN, call this function to start the timer.
   */
  void SetReady ();

  /**
   * Update socket state when receiving a CNP.
   */
  void UpdateStateWithCNP ();

  /**
   * When the sender sending out a packet, update the state if needed.
   */
  void UpdateStateSend (Ptr<Packet> packet);

  void SetStopTime (Time stopTime);

  Time GetCNPInterval () const;
  
private:

  void UpdateAlpha ();

  void RateTimerTriggered ();

  void UpdateRate ();

  const Ptr<RoCEv2SocketState> m_sockState;
  double m_alpha;
  const double m_g;
  double m_raiRatio; //!< RateAI / link rate for additive increase
  double m_hraiRatio; //!< Hyper rate AI / link rate for hyper additive increase
  Timer m_alphaTimer; //!< update alpha if haven't received CNP for a configured time
  Timer m_rateTimer;
  const uint32_t m_bytesThreshold;
  uint32_t m_bytesCounter;
  uint32_t m_rateUpdateIter;
  uint32_t m_bytesUpdateIter;
  const uint32_t m_F;
  double m_targetRateRatio;
  double m_curRateRatio;
  Time m_CNPInterval;
  double m_minRateRatio;
  Time m_stopTime;
  
}; // class DcqcnCongestionOps

} // namespace ns3

#endif // DCQCN_H
