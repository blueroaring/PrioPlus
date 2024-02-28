./ns3 

# Get all files in this directory
# Path: config/demos/run-all.sh
for f in config/demos/*; do
    # Check if file is a directory
    if [ $f != "config/demos/run-all.sh" ]; then
        # Run the simulation
        echo "Run $f"
        ./ns3 run "rdma-simulator $f"&
    fi
done