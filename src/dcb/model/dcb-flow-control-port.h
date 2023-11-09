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

#ifndef DCB_FLOW_CONTROL_PORT_H
#define DCB_FLOW_CONTROL_PORT_H

#include "ns3/net-device.h"
#include "ns3/object.h"

namespace ns3
{

class Packet;
class Address;
class QueueDiscItem;
class DcbTrafficControl;

class DcbFlowControlPort : public Object
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    DcbFlowControlPort(Ptr<NetDevice> dev, Ptr<DcbTrafficControl> tc);
    virtual ~DcbFlowControlPort();

    /**
     * \brief Process when a packet come in.
     */
    void IngressProcess(Ptr<const Packet> packet,
                        uint16_t protocol,
                        const Address& from,
                        const Address& to,
                        NetDevice::PacketType packetType);
    /**
     * \brief Process when a packet previously came from this port is going to send
     * out through other port.
     */
    void PacketOutCallbackProcess(uint8_t priority, Ptr<Packet> packet);

    /**
     * \brief Process when a packet come out of this port.
     */
    void EgressProcess(Ptr<Packet> packet);

    void SetFcEnabled(bool enable);

  protected:
    virtual void DoIngressProcess(Ptr<const Packet> packet,
                                  uint16_t protocol,
                                  const Address& from,
                                  const Address& to,
                                  NetDevice::PacketType packetType) = 0;

    virtual void DoPacketOutCallbackProcess(uint8_t priority, Ptr<Packet> packet) = 0;
    virtual void DoEgressProcess(Ptr<Packet> packet) = 0;

  protected:
    Ptr<NetDevice> m_dev;
    Ptr<DcbTrafficControl> m_tc;

  private:
    bool m_enableIngressControl;
    bool m_enableEgressControl;

}; // class DcbFlowControlPort

} // namespace ns3

#endif // DCB_FLOW_CONTROL_PORT_H
