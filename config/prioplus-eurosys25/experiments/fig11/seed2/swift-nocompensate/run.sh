./ns3 

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Get all files in this directory
for f in ${SCRIPT_DIR}/*; do
    # Check if file end with .json
    if [[ $f == *.json ]]; then
        # Run the simulation
        echo "Run $f"
        ./ns3 run "rdma-simulator $f"&
    fi
done