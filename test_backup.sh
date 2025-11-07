#!/bin/bash

# Backup Replication Test Script
# This script tests the backup functionality step by step

echo "=============================================="
echo "   Backup Replication Test Script"
echo "=============================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored messages
print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Step 1: Clean up old data
print_step "Cleaning up old test data..."
cd "$(dirname "$0")"
cd storage_server

rm -rf storage_data1 storage_data2 2>/dev/null
mkdir -p storage_data1/files storage_data1/undo
mkdir -p storage_data2/files storage_data2/undo

print_success "Test directories created"
echo ""

# Step 2: Start Name Server
print_step "Starting Name Server..."
cd ../name_server

if [ ! -f "name_server" ]; then
    print_error "Name Server executable not found. Compile it first!"
    exit 1
fi

./name_server > /tmp/nm_server.log 2>&1 &
NM_PID=$!
sleep 2

if ps -p $NM_PID > /dev/null; then
    print_success "Name Server started (PID: $NM_PID)"
    print_warning "Logs: /tmp/nm_server.log"
else
    print_error "Name Server failed to start"
    exit 1
fi
echo ""

# Step 3: Start Backup Server FIRST (SS2)
print_step "Starting Backup Storage Server (SS2)..."
cd ../storage_server

./storage_server 2 9003 9004 ./storage_data2 > /tmp/ss2.log 2>&1 &
SS2_PID=$!
sleep 2

if ps -p $SS2_PID > /dev/null; then
    print_success "SS2 (Backup) started (PID: $SS2_PID)"
    print_warning "Logs: /tmp/ss2.log"
else
    print_error "SS2 failed to start"
    kill $NM_PID 2>/dev/null
    exit 1
fi
echo ""

# Step 4: Start Primary Server (SS1)
print_step "Starting Primary Storage Server (SS1)..."

./storage_server 1 9001 9002 ./storage_data1 > /tmp/ss1.log 2>&1 &
SS1_PID=$!
sleep 3

if ps -p $SS1_PID > /dev/null; then
    print_success "SS1 (Primary) started (PID: $SS1_PID)"
    print_warning "Logs: /tmp/ss1.log"
else
    print_error "SS1 failed to start"
    kill $SS2_PID $NM_PID 2>/dev/null
    exit 1
fi
echo ""

# Step 5: Check if backup connection was established
print_step "Checking if backup connection was established..."
sleep 2

if grep -q "Successfully connected to backup server" /tmp/ss1.log; then
    print_success "✓ SS1 connected to SS2"
else
    print_error "✗ SS1 did not connect to SS2"
    echo "SS1 Log:"
    tail -20 /tmp/ss1.log
fi

if grep -q "Bulk sync complete" /tmp/ss1.log || grep -q "Initial bulk sync complete" /tmp/ss1.log; then
    print_success "✓ Bulk synchronization completed"
else
    print_warning "✗ Bulk sync may not have completed"
fi
echo ""

# Step 6: Display server status
print_step "Server Status:"
echo "  Name Server: Running (PID: $NM_PID)"
echo "  SS1 (Primary): Running (PID: $SS1_PID)"
echo "  SS2 (Backup): Running (PID: $SS2_PID)"
echo ""

# Step 7: Show relevant log excerpts
print_step "Key Log Entries:"
echo ""
echo "=== Name Server Logs ==="
grep -E "SS Registration|Backup Pairing|Sent backup info" /tmp/nm_server.log | tail -10
echo ""
echo "=== SS1 (Primary) Logs ==="
grep -E "Backup|backup|BACKUP" /tmp/ss1.log | tail -15
echo ""
echo "=== SS2 (Backup) Logs ==="
grep -E "Backup|backup|BACKUP" /tmp/ss2.log | tail -10
echo ""

# Step 8: Instructions for manual testing
echo "=============================================="
echo "   Servers are running! Now test manually:"
echo "=============================================="
echo ""
echo "1. Open a new terminal and start the client:"
echo "   cd client"
echo "   ./client alice"
echo ""
echo "2. Create a file:"
echo "   osn> CREATE test.txt"
echo ""
echo "3. Watch the logs to see replication:"
echo "   Terminal 1: tail -f /tmp/ss1.log | grep -i backup"
echo "   Terminal 2: tail -f /tmp/ss2.log | grep -i backup"
echo ""
echo "4. Write to the file:"
echo "   osn> WRITE test.txt 0 0"
echo "   > Hello"
echo "   > world!"
echo "   > ETIRW"
echo ""
echo "5. Verify files are identical:"
echo "   diff storage_server/storage_data1/files/test.txt \\"
echo "        storage_server/storage_data2/files/test.txt"
echo ""
echo "=============================================="
echo ""
print_warning "Press Enter to view live logs, or Ctrl+C to stop servers..."
read

# Show live logs
echo ""
print_step "Showing live logs (Ctrl+C to stop)..."
echo ""
tail -f /tmp/ss1.log /tmp/ss2.log /tmp/nm_server.log &
TAIL_PID=$!

# Wait for user to press Ctrl+C
trap "kill $TAIL_PID $SS1_PID $SS2_PID $NM_PID 2>/dev/null; echo ''; echo 'Servers stopped'; exit 0" INT
wait $TAIL_PID
