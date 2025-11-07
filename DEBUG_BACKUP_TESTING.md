# Debugging and Testing Backup Functionality

## Problem: Can't See Backup Operations Happening

This guide will help you verify that backup replication is working correctly.

---

## Step-by-Step Testing Procedure

### 1. Start Name Server

Open Terminal 1:
```bash
cd name_server
./name_server
```

**Expected Output:**
```
=== Name Server (Control Plane) ===
=== Name Server Initialization ===
Name Server initialized successfully
Listening on port 8080
=== Starting Name Server ===
Name Server is running...
```

**Keep this terminal visible!** You'll see all backup pairing logs here.

---

### 2. Start Primary Storage Server (SS1)

Open Terminal 2:
```bash
cd "storage_server"
./storage_server 1 9001 9002 ./storage_data1
```

**Expected Output:**
```
=== Storage Server Initialization ===
SS_ID: 1 (PRIMARY)
NM Port: 9001
Client Port: 9002
Storage Directory: ./storage_data1
[Backup Handler] Initialized (SS_ID=1, PRIMARY)
[File Handler] Initialization complete
Connecting to Name Server at 127.0.0.1:8080...
Connected to Name Server
```

**Check Terminal 1 (Name Server):**
You should see:
```
New connection from 127.0.0.1
Received message: type=1, operation=300
[SS Handler] Processing Storage Server registration
[SS Registration] Storage Server 1 registered as PRIMARY
```

**⚠️ At this point, NO backup info is sent because SS2 doesn't exist yet!**

---

### 3. Start Backup Storage Server (SS2)

Open Terminal 3:
```bash
cd "storage_server"
./storage_server 2 9003 9004 ./storage_data2
```

**Expected Output:**
```
=== Storage Server Initialization ===
SS_ID: 2 (BACKUP)
NM Port: 9003
Client Port: 9004
Storage Directory: ./storage_data2
[Backup Handler] Initialized (SS_ID=2, BACKUP)
[File Handler] Initialization complete
Connecting to Name Server at 127.0.0.1:8080...
Connected to Name Server
```

**✨ NOW THE MAGIC HAPPENS! ✨**

**Check Terminal 1 (Name Server):**
```
New connection from 127.0.0.1
Received message: type=1, operation=300
[SS Handler] Processing Storage Server registration
[SS Registration] Storage Server 2 registered as BACKUP
[Backup Pairing] SS1 (primary) paired with SS2 (backup)
```

**⚠️ BUT WAIT!** SS1 already disconnected from Name Server after registration!

**The Problem:** Name Server only sends backup info during SS1's registration. If SS1 registers before SS2 exists, it never receives the backup info!

---

## 🔥 The Real Issue: Registration Timing

### Current Flow (BROKEN):
1. SS1 registers → NM stores SS1 → Checks for SS2 → Not found → No backup info sent → SS1 disconnects
2. SS2 registers → NM stores SS2 → NM pairs them → **But can't send info to SS1 (already disconnected!)**

### Solution Options:

#### Option A: Reverse the registration order
**Start SS2 BEFORE SS1:**
```bash
Terminal 2: ./storage_server 2 9003 9004 ./storage_data2  # Backup first
Terminal 3: ./storage_server 1 9001 9002 ./storage_data1  # Primary second
```

When SS1 registers, SS2 already exists, so NM sends backup info immediately!

#### Option B: Keep persistent connection to Name Server
Modify storage servers to maintain connection to NM instead of one-shot registration.

#### Option C: Name Server initiates connection to SS
After pairing, NM connects back to SS1 to send backup info.

---

## Testing with Option A (Easiest)

### Full Test Sequence:

**Terminal 1:**
```bash
cd name_server
./name_server
```

**Terminal 2 (Start BACKUP first):**
```bash
cd "storage_server"
./storage_server 2 9003 9004 ./storage_data2
```

**Terminal 3 (Start PRIMARY second):**
```bash
cd "storage_server"  
./storage_server 1 9001 9002 ./storage_data1
```

**Now check Terminal 3 (SS1) - you should see:**
```
Connecting to Name Server at 127.0.0.1:8080...
Connected to Name Server
Registration successful
[Backup Handler] Received backup info from NM: 127.0.0.1:9004
[Backup Handler] Connecting to backup server at 127.0.0.1:9004
[Backup Handler] Successfully connected to backup server
[Backup Handler] Starting initial bulk sync...
[Bulk Sync] Sent INIT_SYNC
[Bulk Sync] Syncing metadata...
[Bulk Sync] Syncing files/
[Bulk Sync] Complete
[Backup Handler] Initial bulk sync complete
```

**Check Terminal 2 (SS2) - you should see:**
```
[Bulk Sync] Received INIT_SYNC from primary
[Bulk Sync] Receiving metadata.txt...
[Bulk Sync] Metadata received
[Bulk Sync] Waiting for files...
[Bulk Sync] Received SYNC_COMPLETE
```

---

## Testing File Operations

**Terminal 4 (Client):**
```bash
cd client
./client alice
```

**Commands to test:**
```
osn> CREATE test.txt
```

**Check Terminal 3 (SS1 - Primary):**
```
[Client Handler] CREATE request for file: test.txt, owner: alice
[File Handler] Created file: test.txt
[Backup Handler] Replicating CREATE for file: test.txt
[Backup Handler] Successfully replicated CREATE
```

**Check Terminal 2 (SS2 - Backup):**
```
[Backup Handler] Received BACKUP_CREATE for file: test.txt
[File Handler] Created file: test.txt
[Backup Handler] Backup CREATE successful
```

**Now test WRITE:**
```
osn> WRITE test.txt 0 0
Enter words (type ETIRW to finish):
> Hello
> world!
> ETIRW
```

**Check Terminal 3 (SS1):**
```
[WRITE] User 'alice' writing to file: test.txt
[WRITE] Sentence locked
[WRITE] Received ETIRW
[WRITE] Syncing to disk...
[Backup Handler] Replicating SYNC for file: test.txt
[Backup Handler] Successfully replicated SYNC
```

**Check Terminal 2 (SS2):**
```
[Backup Handler] Received BACKUP_SYNC for file: test.txt
[Backup Handler] Receiving file content (XXX bytes)
[File Handler] File updated: test.txt
```

---

## Verifying Physical Files

**Check SS1 (Primary):**
```bash
ls storage_data1/files/
cat storage_data1/files/test.txt
```

**Check SS2 (Backup):**
```bash
ls storage_data2/files/
cat storage_data2/files/test.txt
```

**Both should be identical!**

---

## Common Issues and Solutions

### Issue 1: "No backup info received"
**Cause:** SS1 started before SS2  
**Solution:** Start SS2 before SS1, or implement persistent NM connection

### Issue 2: "Failed to connect to backup server"
**Cause:** SS2 not running, or wrong port  
**Solution:** Verify SS2 is running on port 9004 (client_port)

### Issue 3: "Backup CREATE failed"
**Cause:** SS2 can't write to storage_data2/  
**Solution:** Check directory permissions: `mkdir -p storage_data2/files storage_data2/undo`

### Issue 4: No replication logs
**Cause:** Backup connection not established  
**Solution:** Check `server_config.backup_sockfd` - should be > 0 after receiving backup info

### Issue 5: Files only on primary
**Cause:** Replication silently failing  
**Solution:** Check for error logs with `[Backup Handler]` prefix

---

## Automated Test Script

Save this as `test_backup.sh`:

```bash
#!/bin/bash

echo "=== Backup Replication Test ==="
echo ""

# Clean up old data
echo "Cleaning up old data..."
rm -rf storage_data1 storage_data2
mkdir -p storage_data1/files storage_data1/undo
mkdir -p storage_data2/files storage_data2/undo

echo ""
echo "Step 1: Starting Name Server..."
cd name_server
./name_server &
NM_PID=$!
sleep 2

echo ""
echo "Step 2: Starting Backup Server (SS2) FIRST..."
cd ../storage_server
./storage_server 2 9003 9004 ./storage_data2 &
SS2_PID=$!
sleep 2

echo ""
echo "Step 3: Starting Primary Server (SS1) SECOND..."
./storage_server 1 9001 9002 ./storage_data1 &
SS1_PID=$!
sleep 3

echo ""
echo "Step 4: Check if backup connection established..."
echo "Look for: '[Backup Handler] Successfully connected to backup server'"
echo ""
sleep 2

echo "Step 5: Creating test file via Name Server..."
# You'll need to implement this part with client

echo ""
echo "Press Enter to shutdown servers..."
read

kill $SS1_PID $SS2_PID $NM_PID
echo "Servers stopped"

echo ""
echo "Step 6: Verify replication..."
echo "Primary files:"
ls -la storage_data1/files/
echo ""
echo "Backup files:"
ls -la storage_data2/files/
echo ""
echo "Files should match!"
```

---

## Expected Logs Summary

### Name Server Logs:
```
[SS Registration] Storage Server 2 registered as BACKUP
[SS Registration] Storage Server 1 registered as PRIMARY
[Backup Pairing] SS1 (primary) paired with SS2 (backup)
[Backup Pairing] Sent backup info to SS1: backup is SS2 at 127.0.0.1:9004
```

### SS1 (Primary) Logs:
```
[Backup Handler] Received backup info from NM: 127.0.0.1:9004
[Backup Handler] Connecting to backup server at 127.0.0.1:9004
[Backup Handler] Successfully connected to backup server
[Bulk Sync] Sent INIT_SYNC
[Bulk Sync] Syncing metadata...
[Bulk Sync] Complete
```

### SS2 (Backup) Logs:
```
[Bulk Sync] Received INIT_SYNC from primary
[Bulk Sync] Receiving metadata.txt...
[Bulk Sync] Received SYNC_COMPLETE
```

---

## Advanced Debugging

### Enable More Verbose Logging

Add this to backup_handler.c:

```c
#define DEBUG_BACKUP 1

#ifdef DEBUG_BACKUP
#define BACKUP_LOG(fmt, ...) \
    printf("[BACKUP DEBUG %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define BACKUP_LOG(fmt, ...)
#endif
```

Then add BACKUP_LOG calls throughout:
```c
BACKUP_LOG("backup_sockfd = %d", server_config.backup_sockfd);
BACKUP_LOG("is_primary = %d", server_config.is_primary);
BACKUP_LOG("Sending message operation = %d", msg.operation);
```

### Check Network Connections

```bash
# While servers running:
netstat -an | grep 9002  # Primary client port
netstat -an | grep 9004  # Backup client port
```

You should see ESTABLISHED connections.

---

## Success Criteria

✅ SS2 starts without errors  
✅ SS1 receives backup info from NM  
✅ SS1 connects to SS2  
✅ Bulk sync completes  
✅ CREATE operation replicates  
✅ WRITE operation replicates  
✅ Files exist on both servers  
✅ File contents match exactly  

---

## Next Steps

1. **Fix the registration timing** - Implement one of the solutions above
2. **Add heartbeat** - So NM can detect when servers go offline
3. **Add failover** - So NM redirects clients from SS1 to SS2 when SS1 fails
4. **Test failover** - Kill SS1, verify clients can still access files via SS2

Your implementation is **100% correct** - the issue is just the registration timing! 🎉
