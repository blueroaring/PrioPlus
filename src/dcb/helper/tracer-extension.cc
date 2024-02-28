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

#include "tracer-extension.h"

#include "csv-writer.h"
#include "dcb-net-device-helper.h"
#include "dcb-switch-stack-helper.h"

#include "ns3/dcb-net-device.h"
#include "ns3/dcb-traffic-control.h"

#include <fstream>

namespace ns3
{

namespace tracer_extension
{

/******************************************
 ** Declarations
 ******************************************
 */
static std::string outputDirectory;
static Time stopTime;

// ----------------------------------------
template <class T>
static void ClearTracersList(std::list<T>& tracers);
static std::string GetRealFileName(std::string fileName);

// ----------------------------------------
namespace fct_unit
{
Protocol protocol = Protocol::None; // TODO: not supporting multiple protocols
std::ofstream fctFileStream;
void FlowCompletionTracer(uint32_t srcNode,
                          uint32_t dstNode,
                          uint32_t srcPort,
                          uint32_t dstPort,
                          uint32_t flowSize,
                          Time startTime,
                          Time finishTime);
} // namespace fct_unit

// ----------------------------------------
class RateTracer
{
  public:
    RateTracer(Time interval, std::string context);
    ~RateTracer();
    void Trace(Ptr<const Packet> packet);
    static std::list<RateTracer*> tracers;

  private:
    void LogRate();
    uint64_t m_bytes;
    Timer m_timer;
    std::string m_context;
    std::ofstream m_ofstream;
}; // class RateTracer

std::list<RateTracer*> RateTracer::tracers;

// ----------------------------------------
class QueueLengthTracer
{
  public:
    QueueLengthTracer(std::string context, Ptr<PausableQueueDisc> queueDisc, Time interval);
    ~QueueLengthTracer();
    void Trace();
    static std::list<QueueLengthTracer*> tracers;

  private:
    Ptr<PausableQueueDisc> m_queueDisc;
    Timer m_timer;
    std::ofstream m_ofstream;
}; // class QueueLengthTracer

std::list<QueueLengthTracer*> QueueLengthTracer::tracers;

// ----------------------------------------
class BufferOverflowTracer
{
  public:
    BufferOverflowTracer(std::string context, Ptr<DcbTrafficControl> tc);
    ~BufferOverflowTracer();
    void Trace(Ptr<const Packet> packet);
    static std::list<BufferOverflowTracer*> tracers;

  private:
    std::ofstream m_ofstream;
}; // class BufferoverflowTracer

std::list<BufferOverflowTracer*> BufferOverflowTracer::tracers;

/******************************************
 ** tracer_extension functions definition
 ******************************************
 */
void
ConfigOutputDirectory(std::string dirName)
{
    // NOTICIE: no checking
    outputDirectory = dirName;
}

void
ConfigStopTime(Time t)
{
    stopTime = t;
}

void
ConfigTraceFCT(Protocol protocol, std::string fileName)
{
    fct_unit::protocol = protocol;
    std::string fctFile = GetRealFileName(fileName);
    fct_unit::fctFileStream.open(fctFile);
    if (!fct_unit::fctFileStream.good())
    {
        std::cerr << "Error: Cannot open file \"" << fctFile << "\"" << std::endl;
    }
}

void
RegisterTraceFCT(Ptr<DcbTrafficGenApplication> app)
{
    switch (fct_unit::protocol)
    {
    case Protocol::RoCEv2:
        app->TraceConnectWithoutContext("FlowComplete",
                                        MakeCallback(&fct_unit::FlowCompletionTracer));
        break;
    default: // do nothing
        ;
    }
}

void
EnableDevicePcap(Ptr<NetDevice> device, std::string fileNamePrefix)
{
    DcbNetDeviceHelper devHelper;
    devHelper.EnablePcap(GetRealFileName(fileNamePrefix), device);
}

void
EnableSwitchIpv4Pcap(Ptr<Node> sw, std::string fileNamePrefix)
{
    Ptr<Ipv4> ipv4 = sw->GetObject<Ipv4>();
    if (!ipv4)
    {
        NS_FATAL_ERROR("Ipv4 is not bound to the switch " << sw);
    }
    const uint32_t nintf = ipv4->GetNInterfaces();
    DcbSwitchStackHelper switchStack;
    for (uint32_t i = 1; i < nintf; i++) // interface 0 is loopback so we skip it.
    {
        switchStack.EnablePcapIpv4(GetRealFileName(fileNamePrefix), ipv4, i, false);
    }
}

void
EnableDeviceRateTrace(Ptr<NetDevice> device, std::string context, Time interval)
{
    RateTracer* tracer = new RateTracer(interval, context);
    device->TraceConnectWithoutContext("MacTx", MakeCallback(&RateTracer::Trace, tracer));
    RateTracer::tracers.push_back(tracer);
}

void
EnablePortQueueLengthTrace(Ptr<NetDevice> device, std::string context, Time interval)
{
    Ptr<DcbNetDevice> dcbDev = DynamicCast<DcbNetDevice>(device);
    if (dcbDev)
    {
        QueueLengthTracer* tracer =
            new QueueLengthTracer(context, dcbDev->GetQueueDisc(), interval);
        QueueLengthTracer::tracers.push_back(tracer);
    }
    else
    {
        NS_FATAL_ERROR("Cannot cast NetDevice to DcbNetDevice");
    }
}

void
EnableBufferoverflowTrace(Ptr<Node> sw, std::string context)
{
    Ptr<DcbTrafficControl> tc = sw->GetObject<DcbTrafficControl>();
    BufferOverflowTracer* tracer = new BufferOverflowTracer(context, tc);
    BufferOverflowTracer::tracers.push_back(tracer);
}

void
CleanTracers()
{
    if (fct_unit::fctFileStream.is_open())
    {
        fct_unit::fctFileStream.close();
    }
    ClearTracersList(RateTracer::tracers);
    ClearTracersList(QueueLengthTracer::tracers);
    ClearTracersList(BufferOverflowTracer::tracers);
}

/************************************************
 ** Internal classes and functions definition
 ************************************************
 */

template <class T>
static void
ClearTracersList(std::list<T>& tracers)
{
    for (auto tracer : tracers)
    {
        delete tracer;
    }
    tracers.clear();
}

static std::string
GetRealFileName(std::string fileName)
{
    return outputDirectory + "/" + fileName;
}

///////////////////////
/// FCT
///////////////////////
namespace fct_unit
{

void
FlowCompletionTracer(uint32_t srcNode,
                     uint32_t dstNode,
                     uint32_t srcPort,
                     uint32_t dstPort,
                     uint32_t flowSize,
                     Time startTime,
                     Time finishTime)
{
    // TODO: add mutex lock for concurrency
    Time fct = finishTime - startTime;
    CsvWriter writer(&fct_unit::fctFileStream, 8);
    writer.WriteNextValue(srcNode); // src node
    writer.WriteNextValue(dstNode); // dest node
    writer.WriteNextValue(srcPort); // src port/qp
    writer.WriteNextValue(dstPort); // dest port/qp
    writer.WriteNextValue(flowSize);
    writer.WriteNextValue(startTime.GetNanoSeconds());
    writer.WriteNextValue(finishTime.GetNanoSeconds());
    writer.WriteNextValue(fct.GetNanoSeconds());

    // NS_LOG_UNCOND ("Node " << Simulator::GetContext () << " flow of RoCEv2 " << roce.GetDestQP ()
    //                        << "->" << roce.GetSrcQP () << " start at " << startTime << " finish
    //                        at "
    //                        << finishTime << " with FCT=" << fct);
}

} // namespace fct_unit

///////////////////////
/// RateTracer
///////////////////////

RateTracer::RateTracer(Time interval, std::string context)
    : m_bytes(0),
      m_context(context)
{
    m_timer.SetFunction(&RateTracer::LogRate, this);
    m_timer.SetDelay(interval);
    m_timer.Schedule();

    std::string filename = GetRealFileName("rate-tx-" + context + ".csv");
    m_ofstream.open(filename);
    if (!m_ofstream.good())
    {
        std::cerr << "Error: Cannot open file \"" << filename << "\"" << std::endl;
    }
}

RateTracer::~RateTracer()
{
    m_ofstream.close();
}

void
RateTracer::Trace(Ptr<const Packet> packet)
{
    m_bytes += packet->GetSize();
}

void
RateTracer::LogRate()
{
    double rate = static_cast<double>(m_bytes) * 8 / m_timer.GetDelay().GetMicroSeconds();
    m_ofstream << Simulator::Now().GetMicroSeconds() << "," << rate << std::endl;
    // NS_LOG_UNCOND ("rate of device " <<  m_context << " is " << rate << "Mbps");
    m_bytes = 0;
    if (Simulator::Now() < stopTime)
    {
        m_timer.Schedule();
    }
}

///////////////////////
/// QueueLengthTracer
///////////////////////

QueueLengthTracer::QueueLengthTracer(std::string context,
                                     Ptr<PausableQueueDisc> queueDisc,
                                     Time interval)
    : m_queueDisc(queueDisc)
{
    m_timer.SetFunction(&QueueLengthTracer::Trace, this);
    m_timer.SetDelay(interval);
    m_timer.Schedule();

    std::string filename = GetRealFileName("queue-" + context + ".csv");
    m_ofstream.open(filename);
    if (!m_ofstream.good())
    {
        std::cerr << "Error: Cannot open file \"" << filename << "\"" << std::endl;
    }
}

QueueLengthTracer::~QueueLengthTracer()
{
    m_ofstream.close();
}

void
QueueLengthTracer::Trace()
{
    size_t nQueue = m_queueDisc->GetNQueueDiscClasses();
    m_ofstream << Simulator::Now().GetMicroSeconds();
    for (size_t i = 0; i < nQueue; i++) // Get queue length of each priority
    {
        uint32_t bytes = m_queueDisc->GetQueueDiscClass(i)->GetQueueDisc()->GetNBytes();
        m_ofstream << "," << bytes;
    }
    m_ofstream << std::endl;
    if (Simulator::Now() < stopTime)
    {
        m_timer.Schedule();
    }
}

///////////////////////////
/// BufferOverflowTracer
///////////////////////////

BufferOverflowTracer::BufferOverflowTracer(std::string context, Ptr<DcbTrafficControl> tc)
{
    std::string filename = GetRealFileName("bufferoverflow-" + context + ".csv");
    m_ofstream.open(filename);
    tc->TraceConnectWithoutContext("BufferOverflow",
                                   MakeCallback(&BufferOverflowTracer::Trace, this));
}

BufferOverflowTracer::~BufferOverflowTracer()
{
    m_ofstream.close();
}

void
BufferOverflowTracer::Trace(Ptr<const Packet> packet)
{
    Ptr<Packet> p = packet->Copy();
    Ipv4Header ipHeader;
    UdpRoCEv2Header header;
    p->RemoveHeader(ipHeader);
    p->PeekHeader(header);
    m_ofstream << ipHeader.GetSource() << "," << ipHeader.GetDestination() << ","
               << header.GetRoCE().GetSrcQP() << "," << header.GetRoCE().GetDestQP() << ","
               << header.GetRoCE().GetPSN();
}

} // namespace tracer_extension

} // namespace ns3
