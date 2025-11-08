# VIEW Functionality Fix Summary

## Issues Fixed

### 1. Error 1011 - ERR_INVALID_OPERATION
**Problem**: The VIEW command was returning error 1011 (ERR_INVALID_OPERATION) because the Name Server didn't have a handler for the `OP_VIEW` operation.

**Root Cause**: 
- The `OP_VIEW` case was missing in the switch statement in `name_server/name_server.c`
- No `handle_view_files()` function was implemented

**Fix Applied**:
1. Added `handle_view_files()` function declaration in `name_server/name_server.h`
2. Implemented `handle_view_files()` in `name_server/client_handler.c`
3. Added `case OP_VIEW:` in the switch statement in `name_server/name_server.c`

### 2. Backup Replication Not Working
**Problem**: Files created on primary storage server were not being replicated to backup server.

**Root Cause**: 
- In `name_server/ss_manager.c` line 124, the Name Server was sending the **client_port** instead of the **nm_port** for backup connections
- Storage servers listen for backup replication on their **nm_port**, not client_port

**Fix Applied**:
```c
// Changed from:
backup_info.backup_port = backup_ss->client_port;

// To:
backup_info.backup_port = backup_ss->nm_port;  // Use nm_port for backup replication
```

### 3. VIEW Access Control Not Implemented
**Problem**: VIEW command showed all files regardless of user permissions. The requirements specify:
- `VIEW` - Show only files user has access to
- `VIEW -a` - Show all files on system
- `VIEW -l` - Show user-access files with details (word count, char count, owner, etc.)
- `VIEW -al` - Show all files with details

**Root Cause**:
- The original implementation didn't check access permissions
- It didn't query storage servers for detailed file information

**Fix Applied**:
Implemented comprehensive VIEW functionality with:

1. **Access Control Check**:
   - Added `user_has_access()` helper function that queries the storage server to check if user has access to a file
   - Without `-a` flag, only shows files where:
     - User is the owner, OR
     - User is in the file's access list

2. **Detailed Information**:
   - Added `get_file_details()` helper function that queries storage server for file metadata
   - With `-l` flag, displays:
     - Filename
     - Owner
     - Word count
     - Character count
     - Storage Server ID

3. **Flag Combinations**:
   - `VIEW` (no flags): Shows only user-accessible files (simple format)
   - `VIEW -a`: Shows all files on system (simple format)
   - `VIEW -l`: Shows user-accessible files with details
   - `VIEW -al`: Shows all files with details

## Files Modified

1. **name_server/name_server.h**
   - Added: `void handle_view_files(int client_socket, Message *msg);`

2. **name_server/name_server.c**
   - Added: `case OP_VIEW:` in switch statement

3. **name_server/client_handler.c**
   - Implemented: `user_has_access()` helper function
   - Implemented: `get_file_details()` helper function
   - Implemented: `handle_view_files()` with full access control and formatting

4. **name_server/ss_manager.c**
   - Fixed: Changed `backup_info.backup_port` from `client_port` to `nm_port`

## How It Works

### VIEW Command Flow:

1. **Client sends VIEW request** to Name Server with flags in `sentence_index` field
2. **Name Server processes**:
   - Iterates through all files on all storage servers
   - For each file:
     - If `-a` flag: Include all files
     - If no `-a` flag: Check if user has access (owner or in access list)
     - If `-l` flag: Query storage server for detailed metadata
   - Formats output according to flags
3. **Client receives** formatted file list and displays it

### Backup Replication Flow:

1. **Name Server assigns backup** when storage server registers
2. **Name Server sends backup info** to primary SS with correct nm_port
3. **Primary SS connects** to backup SS on nm_port
4. **Primary SS replicates** CREATE/DELETE/WRITE operations to backup
5. **Backup SS stores** replicated data in its storage directory

## Testing Instructions

### Test VIEW Functionality:
```bash
# Start Name Server
cd name_server && make clean && make && ./name_server

# Start Storage Server 1 (Primary)
cd storage_server && ./storage_server 9001 9002 storage_data1

# Start Storage Server 2 (Backup for SS1)
cd storage_server && ./storage_server 9003 9004 storage_data2 1

# Start Client
cd client && make clean && make && ./client alice

# Test VIEW commands
VIEW          # Should show only alice's files
VIEW -a       # Should show all files
VIEW -l       # Should show alice's files with details
VIEW -al      # Should show all files with details
```

### Verify Backup Replication:
```bash
# Create a file as alice
CREATE test.txt

# Check primary storage
ls storage_data1/files/test.txt

# Check backup storage
ls storage_data2/files/test.txt

# Both should exist!
```

## Implementation Notes

- Access control is enforced by querying storage servers via the `OP_INFO` operation
- This approach is more reliable than maintaining a duplicate access list in the Name Server
- The `-l` flag queries each storage server for file metadata, which may add latency for large file lists
- For better performance with many files, consider caching metadata in the Name Server (future optimization)

## Date
November 8, 2025
