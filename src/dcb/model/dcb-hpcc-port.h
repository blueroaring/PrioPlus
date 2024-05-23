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

#ifndef DCB_HPCC_PORT_H
#define DCB_HPCC_PORT_H

#include "dcb-pfc-port.h"

namespace ns3
{

class DcbHpccPort : public DcbPfcPort
{
  public:
    static TypeId GetTypeId();

    DcbHpccPort();  
    DcbHpccPort(Ptr<NetDevice> dev, Ptr<DcbTrafficControl> tc);
    virtual ~DcbHpccPort();

    /**
     * \brief Egress process.
     * In HPCC switch will push the INTHop into the INT header.
     */
    virtual void DoEgressProcess(Ptr<Packet> packet) override;

  protected:
  private:
    uint64_t m_txBytes;
    uint32_t
        m_extraEgressHeaderSize; // When DoEgressProcess, packet does not include the Ethernet
                                 // header and the IP header. So we need to add the size of them.
}; // class DcbPfcPort

} // namespace ns3

#endif // DCB_HPCC_PORT_H
