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

#ifndef POSEIDON_HEADER_H
#define POSEIDON_HEADER_H

#include "ns3/header.h"
#include "ns3/packet.h"

namespace ns3
{
class PoseidonHeader : public ns3::Header
{
  public:
    PoseidonHeader();

    uint16_t m_mpd; // Maximum Per-hop Delay in microseconds. 2 bytes.
    uint16_t
        m_reserved[5]; // 10 bytes reserved. The paper says Poseidon uses 12 bytes for the header.

    static TypeId GetTypeId();
    virtual TypeId GetInstanceTypeId(void) const override;
    virtual uint32_t GetSerializedSize() const override;
    virtual void Serialize(Buffer::Iterator start) const override;
    virtual uint32_t Deserialize(Buffer::Iterator start) override;
    virtual void Print(std::ostream& os) const override;

    void UpdateMpd(uint16_t mpd);
};
} // namespace ns3
#endif // POSEIDON_HEADER_H