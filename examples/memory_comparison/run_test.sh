#!/bin/bash
#
# Script to run memory comparison between ESP-IDF and Custom Logger
# This measures actual runtime heap usage
#

echo "========================================="
echo "   Logger Memory Comparison Test"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if device is connected
if ! pio device list | grep -q "/dev/tty"; then
    echo -e "${RED}Error: No device found. Please connect your ESP32.${NC}"
    exit 1
fi

# Function to run test and capture output
run_test() {
    local env=$1
    local output_file=$2
    local description=$3
    
    echo -e "${YELLOW}=== Testing: $description ===${NC}"
    echo "Building and uploading..."
    
    # Clean build
    pio run -e $env -t clean > /dev/null 2>&1
    
    # Build
    if ! pio run -e $env > build_log.txt 2>&1; then
        echo -e "${RED}Build failed! Check build_log.txt${NC}"
        return 1
    fi
    
    # Upload
    if ! pio run -e $env -t upload > upload_log.txt 2>&1; then
        echo -e "${RED}Upload failed! Check upload_log.txt${NC}"
        return 1
    fi
    
    echo "Monitoring serial output (30 seconds)..."
    # Monitor and save output
    timeout 30s pio device monitor -b 115200 | tee $output_file
    
    echo -e "${GREEN}Test complete!${NC}"
    echo ""
    
    # Clean up logs
    rm -f build_log.txt upload_log.txt
    
    return 0
}

# Test 1: ESP-IDF Logging
if run_test "esp32dev_no_logger" "esp_idf_results.log" "ESP-IDF Logging (baseline)"; then
    ESP_IDF_TESTED=true
else
    ESP_IDF_TESTED=false
fi

# Test 2: Custom Logger
if run_test "esp32dev_with_logger" "custom_logger_results.log" "Custom Logger"; then
    CUSTOM_TESTED=true
else
    CUSTOM_TESTED=false
fi

# Analysis
echo ""
echo "========================================="
echo "           TEST ANALYSIS"
echo "========================================="

if [ "$ESP_IDF_TESTED" = true ] && [ "$CUSTOM_TESTED" = true ]; then
    echo ""
    echo "Extracting key metrics..."
    
    # Extract baseline memory
    ESP_BASELINE=$(grep -A1 "1. Baseline" esp_idf_results.log | grep "Free heap:" | awk '{print $3}')
    CUSTOM_BASELINE=$(grep -A1 "1. Baseline" custom_logger_results.log | grep "Free heap:" | awk '{print $3}')
    
    # Extract after first log
    ESP_AFTER_LOG=$(grep -A1 "2. After first log" esp_idf_results.log | grep "Free heap:" | awk '{print $3}')
    CUSTOM_AFTER_LOG=$(grep -A1 "2. After first log" custom_logger_results.log | grep "Free heap:" | awk '{print $3}')
    
    # Extract singleton cost from summary
    SINGLETON_COST=$(grep "Logger singleton creation:" custom_logger_results.log | awk '{print $4}')
    
    echo ""
    echo "ESP-IDF Logging:"
    echo "  Baseline: $ESP_BASELINE bytes"
    echo "  After first log: $ESP_AFTER_LOG bytes"
    echo "  Memory used: $((ESP_BASELINE - ESP_AFTER_LOG)) bytes"
    
    echo ""
    echo "Custom Logger:"
    echo "  Baseline: $CUSTOM_BASELINE bytes"
    echo "  After first log: $CUSTOM_AFTER_LOG bytes"
    echo "  Logger singleton: $SINGLETON_COST bytes"
    
    echo ""
    echo -e "${GREEN}CONCLUSION:${NC}"
    echo "The Custom Logger uses approximately $SINGLETON_COST bytes"
    echo "of heap memory for the singleton instance."
    
    # Calculate percentage if we have the data
    if [ ! -z "$SINGLETON_COST" ] && [ ! -z "$CUSTOM_BASELINE" ]; then
        # Remove any non-numeric characters
        SINGLETON_NUM=$(echo $SINGLETON_COST | tr -d -c 0-9)
        BASELINE_NUM=$(echo $CUSTOM_BASELINE | tr -d -c 0-9)
        
        if [ ! -z "$SINGLETON_NUM" ] && [ ! -z "$BASELINE_NUM" ] && [ "$BASELINE_NUM" -gt 0 ]; then
            PERCENTAGE=$(awk "BEGIN {printf \"%.2f\", ($SINGLETON_NUM * 100.0) / $BASELINE_NUM}")
            echo "This represents ${PERCENTAGE}% of available heap."
        fi
    fi
    
    echo ""
    echo "Full results saved to:"
    echo "  - esp_idf_results.log"
    echo "  - custom_logger_results.log"
else
    echo -e "${RED}Tests incomplete. Please check the output above.${NC}"
fi

echo ""
echo "========================================="