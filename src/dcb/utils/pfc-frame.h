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

#ifndef PFC_FRAME_H
#define PFC_FRAME_H

#include "ns3/header.h"
#include "ns3/packet.h"

namespace ns3 {

class PfcFrame : public Header
{
public:
  constexpr static const uint16_t PROT_NUMBER = 0x8808;
  constexpr static const uint32_t QUANTUM_BIT = 512; // one quantum is 512 bit

  PfcFrame ();

  /**
   * \brief Enable a Class of Service (aka. priority).
   * \param cls is the number of CoS which should be a value of 0-7.
   *        No checking, be careful.
   */
  void EnableClass (uint16_t cls);
  /**
   * \brief Disable a Class of Service (aka. priority).
   * \param cls is the number of CoS which should be a value of 0-7.
   *        No checking, be careful.
   */
  [[maybe_unused]] void DisableClass (uint16_t cls);

  void SetEnableClassField (uint8_t vec);
  uint8_t GetEnableClassField () const;

  /**
   * \brief Set quanta of a CoS (aka. priority).
   * \param cls is the number of CoS which should be a value of 0-7.
   * \param quanta the quanta to pause. One quantum is the time required to
   *        transmit 512 bits of a frame at the data rate of the MAC.
   *        No checking, be careful.
   */
  void SetQuanta (uint8_t cls, uint16_t quanta);
  /**
   * \brief Get quanta of a CoS (aka. priority).
   */
  uint16_t GetQuanta (uint8_t cls) const;
  

  static TypeId GetTypeId ();
  virtual TypeId GetInstanceTypeId (void) const override;
  virtual uint32_t GetSerializedSize () const override;
  virtual void Serialize (Buffer::Iterator start) const override;
  virtual uint32_t Deserialize (Buffer::Iterator start) override;
  virtual void Print (std::ostream &os) const override;

  static Ptr<Packet> GeneratePauseFrame (uint8_t priority, uint16_t quanta = 0xffff);
  static Ptr<Packet> GeneratePauseFrame (uint8_t enableVec, uint16_t quanta[8]);

private:
  constexpr static const uint32_t DEFAULT_OPCODE = 0x0101;

  uint16_t m_opcode;
  uint16_t m_enableVec;
  uint16_t m_quantaVec[8];
  uint8_t m_reserved[28];
  uint16_t m_frameCheck;
};

} // namespace ns3

#endif // PFC_FRAME_H
