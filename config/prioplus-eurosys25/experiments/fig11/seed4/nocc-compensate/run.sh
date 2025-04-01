./ns3
configs=("conext-paper/eval/multi-prio/seed4/nocc-compensate/nocc-applicationConfig::0::applicationConfig::P...ongestionConfig::Application::PartofCdfPrio::end-0.json" "conext-paper/eval/multi-prio/seed4/nocc-compensate/nocc-applicationConfig::0::applicationConfig::P...ongestionConfig::Application::PartofCdfPrio::end-1.json")
for((i=0;i<2;i+=1)) do 
{
    ./ns3 run "rdma-simulator config/${configs[$i]}" 
}& 
done