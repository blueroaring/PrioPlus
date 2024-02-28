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

#ifndef TRACER_EXTENSION_H
#define TRACER_EXTENSION_H

#include "ns3/dcb-net-device.h"
#include "ns3/dcb-traffic-gen-application.h"
#include "ns3/dcb-traffic-control.h"
#include "ns3/rocev2-header.h"
#include "ns3/rocev2-socket.h"

#include <fstream>

namespace ns3
{

namespace tracer_extension
{

typedef void (*FlowTracedCallback)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, Time, Time);

enum Protocol
{
    None,
    RoCEv2,
};

void ConfigOutputDirectory(std::string dirName);
void ConfigStopTime(Time stopTime);

void ConfigTraceFCT(Protocol protocol, std::string fileName);

void RegisterTraceFCT(Ptr<DcbTrafficGenApplication> app);

/**
 * Capture packet at device and output the pcap file with prefix fileNamePrefix.
 */
void EnableDevicePcap(Ptr<NetDevice> device, std::string fileNamePrefix);
/**
 * Capture packets at all IPv4 interfaces of a switch and output the pcap file
 * with prefix fileNamePrefix.
 */
void EnableSwitchIpv4Pcap(Ptr<Node> sw, std::string fileNamePrefix);

void EnableDeviceRateTrace(Ptr<NetDevice> device, std::string context, Time interval);

void EnablePortQueueLengthTrace(Ptr<NetDevice> device, std::string context, Time interval);

void EnableBufferoverflowTrace(Ptr<Node> sw, std::string context);

void CleanTracers();

} // namespace tracer_extension

} // namespace ns3

#endif // TRACER_EXTENSION_H
