./ns3
configs=("conext-paper/eval/multi-prio/seed9/swift-compensate/swift-applicationConfig::0::applicationConfig::...ongestionConfig::Application::PartofCdfPrio::end-0.json" "conext-paper/eval/multi-prio/seed9/swift-compensate/swift-applicationConfig::0::applicationConfig::...ongestionConfig::Application::PartofCdfPrio::end-1.json")
for((i=0;i<2;i+=1)) do 
{
    ./ns3 run "rdma-simulator config/${configs[$i]}" 
}& 
done