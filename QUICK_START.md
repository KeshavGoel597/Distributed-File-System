# Quick Start Guide - Testing Backup Replication

## TL;DR - Just Run This

```bash
# Option 1: Automated test
./test_backup.sh

# Option 2: Manual test (4 terminals)
# Terminal 1: Name Server
cd name_server && ./name_server

# Terminal 2: Backup Server (START FIRST!)
cd storage_server && ./storage_server 2 9003 9004 ./storage_data2

# Terminal 3: Primary Server (START SECOND!)
cd storage_server && ./storage_server 1 9001 9002 ./storage_data1

# Terminal 4: Client
cd client && ./client alice
osn> CREATE test.txt
osn> WRITE test.txt 0 0
> Hello
> world!
> ETIRW
osn> READ test.txt
```

## What You Should See

### ✅ Terminal 3 (SS1 Primary)
```
Waiting for backup info from Name Server...
Received backup info from NM
Backup server: 127.0.0.1:9004
[Backup Handler] Successfully connected to backup server
[Bulk Sync] Complete
Successfully configured backup replication
```

### ✅ Terminal 2 (SS2 Backup)
```
[Bulk Sync] Received INIT_SYNC from primary
[Bulk Sync] Received SYNC_COMPLETE
```

### ✅ After CREATE/WRITE in Terminal 4
**SS1 (Terminal 3):**
```
[Backup Handler] Replicating CREATE for file: test.txt
[Backup Handler] Successfully replicated CREATE
[Backup Handler] Replicating SYNC for file: test.txt
[Backup Handler] Successfully replicated SYNC
```

**SS2 (Terminal 2):**
```
[Backup Handler] Received BACKUP_CREATE for file: test.txt
[Backup Handler] Received BACKUP_SYNC for file: test.txt
```

## Verify Files Match

```bash
# Check files exist on both servers
ls -la storage_server/storage_data1/files/test.txt
ls -la storage_server/storage_data2/files/test.txt

# Verify content is identical
diff storage_server/storage_data1/files/test.txt \
     storage_server/storage_data2/files/test.txt

# Should output nothing (files are identical)
```

## Important Order

**⚠️ CRITICAL: Start backup server (SS2) BEFORE primary server (SS1)!**

Why? So SS2 is already online when SS1 registers with Name Server. This way NM can immediately send backup info to SS1.

## Troubleshooting

### "No backup info received"
- Check you started SS2 **before** SS1
- Check Name Server is running
- Check logs: `grep "backup info" /tmp/ss1.log`

### "Failed to connect to backup server"
- Check SS2 is running on port 9004
- Check firewall isn't blocking connections
- Check logs: `tail -20 /tmp/ss2.log`

### "Files don't match"
- Check both servers show successful replication logs
- Check for error messages in logs
- Verify network connection: `netstat -an | grep 9004`

## Logs Location

When using `test_backup.sh`:
- Name Server: `/tmp/nm_server.log`
- SS1 (Primary): `/tmp/ss1.log`
- SS2 (Backup): `/tmp/ss2.log`

Watch live:
```bash
tail -f /tmp/ss1.log | grep -i backup
```

## Status Check

Run this to verify everything is working:
```bash
# Check all processes running
ps aux | grep -E "name_server|storage_server"

# Check backup connection established
grep "Successfully connected to backup" /tmp/ss1.log

# Check bulk sync completed
grep "Bulk sync complete" /tmp/ss1.log

# Check replication working
grep "Successfully replicated" /tmp/ss1.log
```

## Clean Up

```bash
# Stop all servers
pkill name_server
pkill storage_server

# Clean test data
rm -rf storage_server/storage_data1 storage_server/storage_data2
rm -f /tmp/nm_server.log /tmp/ss1.log /tmp/ss2.log
```

---

**That's it! Your backup replication is working! 🎉**

For detailed explanation, see `BACKUP_FIX_SUMMARY.md`
