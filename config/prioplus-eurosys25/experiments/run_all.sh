#!/bin/bash
# Parse command line options
PATTERN=""  # Pattern for config file
while getopts ":p:" opt; do
  case $opt in
    p) PATTERN="$OPTARG" ;;
    \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
  esac
done
shift $((OPTIND -1))
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
MAX_CPU=95                # Maximum allowed CPU usage (%)
MIN_MEM=20                # Minimum allowed free memory (%)
CHECK_INTERVAL=5          # Resource check interval (seconds)
PARALLEL_GAP=1            # Delay between task starts (seconds)

# Record the start time
SCRIPT_START_TIME=$(date +"%Y-%m-%d %H:%M:%S")
START_TIME=$(date +%s)
echo "Script start time: $SCRIPT_START_TIME"

# Recursively find all JSON config files (including subdirectories)
echo "Scanning for JSON config files..."
declare -a FILES
while IFS= read -r -d $'\0' file; do
  if [[ -z "$PATTERN" ]] || [[ "$file" =~ $PATTERN ]]; then
    FILES+=("$file")
    echo "Found config: $file"
  fi
done < <(find "$SCRIPT_DIR" -type f -name "*.json" -print0)

total_files=${#FILES[@]}
echo "Total ${total_files} config files found"

# Resource monitoring function (enhanced)
check_resources() {
    # Get CPU usage (1-second average)
    local cpu_usage=$(top -bn1 | awk -F, '/%Cpu/ { gsub(/[^0-9.]/, "", $4); print 100 - $4 }')
    
    # Get memory metrics using free command
    local mem_info=$(free -m | awk '/Mem:/ {print $2, $7}')
    local mem_total=$(echo $mem_info | awk '{print $1}')
    local mem_avail=$(echo $mem_info | awk '{print $2}')
    local mem_remain=$(awk "BEGIN {printf \"%.1f\", $mem_avail/$mem_total*100}")

    # Output current resource status
    printf "[%s] CPU: %.1f%%, Mem: %.1f%% Free\\n" \
        "$(date +%T)" "$cpu_usage" "$mem_remain"

    # Check resource thresholds
    (( $(echo "$cpu_usage > $MAX_CPU" | bc -l) )) && return 1
    (( $(echo "$mem_remain < $MIN_MEM" | bc -l) )) && return 1
    return 0
}

# Compile the ns3 project first to avoid compilation during the run
echo "Compiling ns3 project..."
./ns3 build

# Main experiment execution loop
current_index=0
while [[ $current_index -lt $total_files ]]; do
    if check_resources; then
        config="${FILES[$current_index]}"
        echo "================================================================="
        printf "▶︎ Starting (%d/%d)\\n" $((current_index+1)) $total_files
        echo "⚙︎  Config: ${config}"
        echo "🗓  Directory: $(dirname "$config")"
        echo "⏱  Start time: $(date +'%T %F')"
        
        # Execute experiments
        # /usr/bin/time -v ./ns3 run "rdma-simulator ${config}" &
        ./ns3 run "rdma-simulator ${config}" &
        
        ((current_index++))
        sleep $PARALLEL_GAP
    else
        printf "⏳ [%s] Waiting for resources...\\n" "$(date +%T)"
        sleep $CHECK_INTERVAL
    fi
done

echo "✅ All experiments submitted!"
wait

END_TIME=$(date +%s)
SCRIPT_END_TIME=$(date +"%Y-%m-%d %H:%M:%S")
DURATION=$((END_TIME - START_TIME))
echo "🎉 All simulations completed!"
echo "================================================================"
echo "Script start time: $SCRIPT_START_TIME"
echo "Script end time:   $SCRIPT_END_TIME"
printf "Total duration:   %02d:%02d:%02d (HH:MM:SS)\n" $((DURATION/3600)) $(( (DURATION%3600)/60 )) $((DURATION%60))
echo "================================================================"