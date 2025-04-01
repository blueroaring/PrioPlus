./ns3
configs=("conext-paper/eval/multi-prio/seed9/swift-compensate/swift-applicationConfig::0::applicationConfig::...ngestionConfig::Application::PartofCdfPrio::end-31.json" "conext-paper/eval/multi-prio/seed9/swift-compensate/swift-applicationConfig::0::applicationConfig::...ngestionConfig::Application::PartofCdfPrio::end-33.json" "conext-paper/eval/multi-prio/seed9/swift-compensate/swift-applicationConfig::0::applicationConfig::...ngestionConfig::Application::PartofCdfPrio::end-35.json" "conext-paper/eval/multi-prio/seed9/swift-compensate/swift-applicationConfig::0::applicationConfig::...ngestionConfig::Application::PartofCdfPrio::end-37.json" "conext-paper/eval/multi-prio/seed9/swift-compensate/swift-applicationConfig::0::applicationConfig::...ngestionConfig::Application::PartofCdfPrio::end-39.json")
for((i=0;i<5;i+=1)) do 
{
    ./ns3 run "rdma-simulator config/${configs[$i]}" 
}& 
done