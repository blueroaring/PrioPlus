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

#ifndef ROCEV2_L4_PROTOCOL_H
#define ROCEV2_L4_PROTOCOL_H

#include "udp-based-l4-protocol.h"
#include <map>

namespace ns3 {

class RoCEv2L4Protocol : public UdpBasedL4Protocol
{

public:
  constexpr inline static const uint16_t PROT_NUMBER = 4791;
  constexpr inline static const uint32_t DEFAULT_DST_QP = 100;

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  RoCEv2L4Protocol ();
  virtual ~RoCEv2L4Protocol ();

  // virtual InnerEndPoint *Allocate () override;
  using UdpBasedL4Protocol::Allocate;
  virtual InnerEndPoint *Allocate (uint32_t dstPort) override;
  InnerEndPoint *Allocate (uint32_t srcPort, uint32_t dstPort);

  virtual uint16_t GetProtocolNumber (void) const override;

  virtual uint32_t GetInnerProtocolHeaderSize () const override;

  virtual uint32_t GetHeaderSize () const override;

  virtual uint32_t GetDefaultServicePort () const override;
  static uint32_t DefaultServicePort ();

  virtual Ptr<Socket> CreateSocket () override;

  static Ptr<Packet> GenerateCNP (uint32_t srcQP, uint32_t dstQP);
  static Ptr<Packet> GenerateACK (uint32_t srcQP, uint32_t dstQP, uint32_t expectedPSN);
  static Ptr<Packet> GenerateNACK (uint32_t srcQP, uint32_t dstQP, uint32_t expectedPSN);

  // protected:
  // void ServerReceive (Ptr<Packet> packet, Ipv4Header header, uint32_t port,
  //                     Ptr<Ipv4Interface> incommingInterface);

private:

  virtual void FinishSetup (Ipv4EndPoint *const udpEndPoint) override;

  // virtual void PreSend (Ptr<Packet> packet, Ipv4Address saddr, Ipv4Address daddr, uint32_t srcQP,
  //                       uint32_t destQP, Ptr<Ipv4Route> route) override;

  /**
   * \brief Get the dst QP from RoCEv2 header.
   * \return The dst QP number.
   */
  virtual uint32_t ParseInnerPort (Ptr<Packet> packet, Ipv4Header header, uint16_t port,
                                   Ptr<Ipv4Interface> incomingIntf) override;

  std::map<uint32_t, uint32_t> m_qpMapper; //!< map destQP to stcQP

}; // class RoCEv2L4Protocol

} // namespace ns3

#endif // ROCEV2_L4_PROTOCOL_H
