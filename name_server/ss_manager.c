#define _GNU_SOURCE
#include "name_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// HASH TABLE IMPLEMENTATION FOR EFFICIENT FILE SEARCH (O(1))
// ============================================================================

// Initialize hash table
void init_file_hash_table() {
    memset(&nm_state->file_index.buckets, 0, sizeof(nm_state->file_index.buckets));
    pthread_mutex_init(&nm_state->file_index.hash_mutex, NULL);
    printf("[Hash Table] Initialized with %d buckets\n", FILE_HASH_TABLE_SIZE);
}

// Hash function using djb2 algorithm (Dan Bernstein)
unsigned int hash_filename(const char *filename) {
    unsigned long hash = 5381;
    int c;
    
    while ((c = *filename++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % FILE_HASH_TABLE_SIZE;
}

// Insert file into hash table
void hash_insert_file(FileInfo *file) {
    pthread_mutex_lock(&nm_state->file_index.hash_mutex);
    
    unsigned int index = hash_filename(file->filename);
    
    // Create new node
    FileHashNode *node = (FileHashNode *)malloc(sizeof(FileHashNode));
    if (node == NULL) {
        fprintf(stderr, "[Hash Table] Failed to allocate memory for hash node\n");
        pthread_mutex_unlock(&nm_state->file_index.hash_mutex);
        return;
    }
    
    strncpy(node->filename, file->filename, MAX_FILENAME - 1);
    node->filename[MAX_FILENAME - 1] = '\0';
    node->file_ptr = file;
    node->next = nm_state->file_index.buckets[index];
    
    nm_state->file_index.buckets[index] = node;
    
    pthread_mutex_unlock(&nm_state->file_index.hash_mutex);
    
    printf("[Hash Table] Inserted file '%s' at bucket %u\n", file->filename, index);
}

// Remove file from hash table
void hash_remove_file(const char *filename) {
    pthread_mutex_lock(&nm_state->file_index.hash_mutex);
    
    unsigned int index = hash_filename(filename);
    FileHashNode *current = nm_state->file_index.buckets[index];
    FileHashNode *prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            if (prev == NULL) {
                nm_state->file_index.buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            printf("[Hash Table] Removed file '%s' from bucket %u\n", filename, index);
            pthread_mutex_unlock(&nm_state->file_index.hash_mutex);
            return;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&nm_state->file_index.hash_mutex);
}

// Find file in hash table - O(1) average case
FileInfo* hash_find_file(const char *filename) {
    pthread_mutex_lock(&nm_state->file_index.hash_mutex);
    
    unsigned int index = hash_filename(filename);
    FileHashNode *current = nm_state->file_index.buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            FileInfo *file = current->file_ptr;
            pthread_mutex_unlock(&nm_state->file_index.hash_mutex);
            return file;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&nm_state->file_index.hash_mutex);
    return NULL;
}

// ============================================================================
// FILE LOCATION CACHE IMPLEMENTATION (LRU Cache for Recent Searches)
// ============================================================================

// Initialize the cache
void init_file_cache() {
    memset(&nm_state->search_cache.entries, 0, sizeof(nm_state->search_cache.entries));
    nm_state->search_cache.next_evict_index = 0;
    pthread_mutex_init(&nm_state->search_cache.cache_mutex, NULL);
    printf("[Cache] Initialized LRU cache with %d entries (TTL: %d seconds)\n", 
           CACHE_SIZE, CACHE_TTL);
}

// Hash function for cache indexing (djb2 algorithm)
static unsigned int cache_hash(const char *filename) {
    unsigned long hash = 5381;
    int c;
    while ((c = *filename++)) {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash % CACHE_SIZE;
}

// Insert or update cache entry - O(1) hash-based
void cache_insert(const char *filename, int ss_id) {
    pthread_mutex_lock(&nm_state->search_cache.cache_mutex);
    
    // Calculate hash index for O(1) lookup
    unsigned int hash_index = cache_hash(filename);
    
    // Check if entry at hash index matches (cache hit on same hash slot)
    if (nm_state->search_cache.entries[hash_index].valid &&
        strcmp(nm_state->search_cache.entries[hash_index].filename, filename) == 0) {
        // Update existing entry at hash position
        nm_state->search_cache.entries[hash_index].ss_id = ss_id;
        nm_state->search_cache.entries[hash_index].timestamp = time(NULL);
        pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
        printf("[Cache] Updated entry for '%s' at hash index %u (SS%d)\n", 
               filename, hash_index, ss_id);
        return;
    }
    
    // If hash slot is empty or has different file, use it (hash collision = evict old entry)
    // This is acceptable for a cache - we trade collision handling for O(1) speed
    strncpy(nm_state->search_cache.entries[hash_index].filename, filename, MAX_FILENAME - 1);
    nm_state->search_cache.entries[hash_index].filename[MAX_FILENAME - 1] = '\0';
    nm_state->search_cache.entries[hash_index].ss_id = ss_id;
    nm_state->search_cache.entries[hash_index].timestamp = time(NULL);
    nm_state->search_cache.entries[hash_index].valid = 1;
    
    pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
    printf("[Cache] Inserted entry for '%s' at hash index %u (SS%d)\n", 
           filename, hash_index, ss_id);
}

// Lookup cache entry - O(1) hash-based lookup
int cache_lookup(const char *filename, int *ss_id) {
    pthread_mutex_lock(&nm_state->search_cache.cache_mutex);
    
    // Calculate hash index for O(1) lookup
    unsigned int hash_index = cache_hash(filename);
    time_t now = time(NULL);
    
    // Direct hash-based lookup - O(1)
    if (nm_state->search_cache.entries[hash_index].valid &&
        strcmp(nm_state->search_cache.entries[hash_index].filename, filename) == 0) {
        
        // Check if entry is still valid (not expired)
        if ((now - nm_state->search_cache.entries[hash_index].timestamp) < CACHE_TTL) {
            *ss_id = nm_state->search_cache.entries[hash_index].ss_id;
            pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
            printf("[Cache] HIT for '%s' at hash index %u (SS%d)\n", 
                   filename, hash_index, *ss_id);
            return 1;  // Cache hit
        } else {
            // Entry expired, invalidate it
            nm_state->search_cache.entries[hash_index].valid = 0;
            pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
            printf("[Cache] MISS for '%s' at hash index %u (expired entry)\n", 
                   filename, hash_index);
            return 0;  // Cache miss (expired)
        }
    }
    
    pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
    printf("[Cache] MISS for '%s' at hash index %u (not found or hash collision)\n", 
           filename, hash_index);
    return 0;  // Cache miss (not found or different file at this hash slot)
}

// Invalidate specific cache entry - O(1) hash-based
void cache_invalidate(const char *filename) {
    pthread_mutex_lock(&nm_state->search_cache.cache_mutex);
    
    // Calculate hash index for O(1) lookup
    unsigned int hash_index = cache_hash(filename);
    
    // Direct hash-based invalidation - O(1)
    if (nm_state->search_cache.entries[hash_index].valid &&
        strcmp(nm_state->search_cache.entries[hash_index].filename, filename) == 0) {
        nm_state->search_cache.entries[hash_index].valid = 0;
        pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
        printf("[Cache] Invalidated entry for '%s' at hash index %u\n", filename, hash_index);
        return;
    }
    
    pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
    printf("[Cache] Entry for '%s' not found in cache (no invalidation needed)\n", filename);
}

// Clear entire cache (e.g., when SS status changes)
void cache_clear() {
    pthread_mutex_lock(&nm_state->search_cache.cache_mutex);
    memset(&nm_state->search_cache.entries, 0, sizeof(nm_state->search_cache.entries));
    nm_state->search_cache.next_evict_index = 0;
    pthread_mutex_unlock(&nm_state->search_cache.cache_mutex);
    printf("[Cache] Cleared all entries\n");
}

// ============================================================================
// STORAGE SERVER MANAGEMENT
// ============================================================================

int register_storage_server(Message *msg) {
    pthread_mutex_lock(&nm_state->ss_list_mutex);
    
    // Check if SS already exists
    int existing_index = find_storage_server(msg->ss_id);
    
    if (existing_index >= 0) {
        // Server reconnecting - update info
        StorageServerInfo *ss = &nm_state->storage_servers[existing_index];
        
        pthread_mutex_lock(&ss->ss_mutex);
        strcpy(ss->ip, msg->ip);
        ss->nm_port = msg->port1;
        ss->client_port = msg->port2;
        ss->status = SS_STATUS_ONLINE;
        pthread_mutex_unlock(&ss->ss_mutex);
        
        printf("[SS Registration] Storage Server %d reconnected\n", msg->ss_id);
        log_operation("SS_RECONNECT", msg->data);
    } else {
        // New server registration
        if (nm_state->ss_count >= MAX_STORAGE_SERVERS) {
            pthread_mutex_unlock(&nm_state->ss_list_mutex);
            printf("[SS Registration] Error: Maximum storage servers reached\n");
            return ERR_SERVER_ERROR;
        }
        
        StorageServerInfo *ss = &nm_state->storage_servers[nm_state->ss_count];
        
        pthread_mutex_lock(&ss->ss_mutex);
        ss->ss_id = msg->ss_id;
        strcpy(ss->ip, msg->ip);
        ss->nm_port = msg->port1;
        ss->client_port = msg->port2;
        ss->status = SS_STATUS_ONLINE;
        ss->file_count = 0;
        pthread_mutex_unlock(&ss->ss_mutex);
        
        nm_state->ss_count++;
        
        printf("[SS Registration] Storage Server %d registered\n", msg->ss_id);
        
        // Parse file list from data field
        if (strlen(msg->data) > 0) {
            char *file_list = strdup(msg->data);
            char *filename = strtok(file_list, ",");
            
            while (filename != NULL && ss->file_count < MAX_FILES_PER_SERVER) {
                // Parse filename:owner format
                char *colon = strchr(filename, ':');
                char owner[MAX_USERNAME] = "system";  // Default owner
                
                if (colon != NULL) {
                    *colon = '\0';  // Split at colon
                    strncpy(owner, colon + 1, MAX_USERNAME - 1);
                    owner[MAX_USERNAME - 1] = '\0';
                }
                
                strcpy(ss->files[ss->file_count].filename, filename);
                strcpy(ss->files[ss->file_count].owner, owner);
                ss->files[ss->file_count].ss_id = msg->ss_id;
                ss->files[ss->file_count].created_time = time(NULL);
                ss->files[ss->file_count].modified_time = time(NULL);
                
                // Add file to hash table for O(1) lookup
                hash_insert_file(&ss->files[ss->file_count]);
                
                ss->file_count++;
                
                printf("[SS Registration] Registered file: %s (owner: %s)\n", filename, owner);
                filename = strtok(NULL, ",");
            }
            
            free(file_list);
        }
        
        log_operation("SS_REGISTER", msg->data);
    }
    
    pthread_mutex_unlock(&nm_state->ss_list_mutex);
    
    print_server_status();
    return ERR_SUCCESS;
}

void handle_storage_server_registration(int socket, Message *msg) {
    printf("[SS Handler] Processing Storage Server registration\n");
    
    int result = register_storage_server(msg);
    
    Message response = {0};
    response.msg_type = MSG_ACK;
    response.operation = OP_SS_REGISTER;
    response.error_code = result;
    
    if (result == ERR_SUCCESS) {
        strcpy(response.data, "Registration successful");
    } else {
        strcpy(response.data, "Registration failed");
    }
    
    send_message(socket, &response);
}

int find_storage_server(int ss_id) {
    for (int i = 0; i < nm_state->ss_count; i++) {
        if (nm_state->storage_servers[i].ss_id == ss_id) {
            return i;
        }
    }
    return -1;  // Not found
}

int get_available_storage_server() {
    pthread_mutex_lock(&nm_state->assignment_mutex);
    
    // CRITICAL FIX: Iterate through actual registered servers, not guessed IDs
    // This handles sparse IDs, large IDs, and is much more efficient
    
    if (nm_state->ss_count == 0) {
        pthread_mutex_unlock(&nm_state->assignment_mutex);
        printf("[Load Balancing] Error: No storage servers registered\n");
        return -1;
    }
    
    // Round-robin assignment starting from last assigned index
    int start_index = nm_state->next_ss_index;
    int selected_ss = -1;
    
    // Try each registered server, starting from next_ss_index
    for (int i = 0; i < nm_state->ss_count; i++) {
        int current_index = (start_index + i) % nm_state->ss_count;
        StorageServerInfo *ss = &nm_state->storage_servers[current_index];
        
        pthread_mutex_lock(&ss->ss_mutex);
        
        // Check if this server is online
        if (ss->status == SS_STATUS_ONLINE) {
            selected_ss = ss->ss_id;
            
            // Update next_ss_index to the next index for true round-robin
            nm_state->next_ss_index = (current_index + 1) % nm_state->ss_count;
            
            pthread_mutex_unlock(&ss->ss_mutex);
            break;
        }
        
        pthread_mutex_unlock(&ss->ss_mutex);
    }
    
    pthread_mutex_unlock(&nm_state->assignment_mutex);
    
    if (selected_ss > 0) {
        printf("[Load Balancing] Assigned storage server: SS%d\n", selected_ss);
    } else {
        printf("[Load Balancing] Error: No available storage servers\n");
    }
    
    return selected_ss;
}

int add_file_to_server(int ss_id, const char *filename, const char *owner) {
    int ss_index = find_storage_server(ss_id);
    if (ss_index < 0) {
        return ERR_SERVER_ERROR;
    }
    
    StorageServerInfo *ss = &nm_state->storage_servers[ss_index];
    
    pthread_mutex_lock(&ss->ss_mutex);
    
    if (ss->file_count >= MAX_FILES_PER_SERVER) {
        pthread_mutex_unlock(&ss->ss_mutex);
        return ERR_SERVER_ERROR;
    }
    
    // Check if file already exists
    for (int i = 0; i < ss->file_count; i++) {
        if (strcmp(ss->files[i].filename, filename) == 0) {
            pthread_mutex_unlock(&ss->ss_mutex);
            return ERR_FILE_EXISTS;
        }
    }
    
    // Add new file
    FileInfo *file = &ss->files[ss->file_count];
    strcpy(file->filename, filename);
    strcpy(file->owner, owner);
    file->ss_id = ss_id;
    file->created_time = time(NULL);
    file->modified_time = time(NULL);
    
    // Add to hash table for O(1) lookup
    hash_insert_file(file);
    
    ss->file_count++;
    
    pthread_mutex_unlock(&ss->ss_mutex);
    
    printf("[File Management] Added file '%s' to SS%d (owner: %s)\n", filename, ss_id, owner);
    return ERR_SUCCESS;
}

int remove_file_from_server(int ss_id, const char *filename) {
    int ss_index = find_storage_server(ss_id);
    if (ss_index < 0) {
        return ERR_SERVER_ERROR;
    }
    
    StorageServerInfo *ss = &nm_state->storage_servers[ss_index];
    
    pthread_mutex_lock(&ss->ss_mutex);
    
    // Find and remove file
    for (int i = 0; i < ss->file_count; i++) {
        if (strcmp(ss->files[i].filename, filename) == 0) {
            // Remove from hash table first
            hash_remove_file(filename);
            
            // Shift remaining files
            for (int j = i; j < ss->file_count - 1; j++) {
                ss->files[j] = ss->files[j + 1];
            }
            ss->file_count--;
            
            pthread_mutex_unlock(&ss->ss_mutex);
            printf("[File Management] Removed file '%s' from SS%d\n", filename, ss_id);
            return ERR_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&ss->ss_mutex);
    return ERR_FILE_NOT_FOUND;
}

FileInfo* find_file(const char *filename) {
    // Use hash table for O(1) lookup instead of O(N*M) linear search
    return hash_find_file(filename);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void print_server_status() {
    printf("\n=== Name Server Status ===\n");
    printf("Storage Servers: %d\n", nm_state->ss_count);
    printf("Clients: %d\n", nm_state->client_count);
    
    for (int i = 0; i < nm_state->ss_count; i++) {
        StorageServerInfo *ss = &nm_state->storage_servers[i];
        pthread_mutex_lock(&ss->ss_mutex);
        printf("  SS%d: %s:%d (status: %s, files: %d)\n",
               ss->ss_id, ss->ip, ss->client_port,
               ss->status == SS_STATUS_ONLINE ? "ONLINE" : "OFFLINE",
               ss->file_count);
        pthread_mutex_unlock(&ss->ss_mutex);
    }
    printf("========================\n\n");
}

void log_operation(const char *operation, const char *details) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[LOG] %s - %s: %s\n", timestamp, operation, details);
}
