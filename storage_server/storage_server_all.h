#ifndef STORAGE_SERVER_ALL_H
#define STORAGE_SERVER_ALL_H

// Consolidated header file for all storage server modules
// Combines: storage_server.h, file_handler_ll.h, backup_handler.h, ss_client_comm.h, ss_nm_comm.h

#include "../common/protocol.h"
#include <pthread.h>
#include <sys/socket.h>
#include <errno.h>

// ============================================================================
// FILE HANDLER (Linked List Implementation) - from file_handler_ll.h
// ============================================================================

// Word node - stores a single word
typedef struct WordNode {
    char word[256];              // The actual word
    struct WordNode *next;       // Next word in the sentence
} WordNode;

// Sentence node - stores a sentence (linked list of words)
typedef struct SentenceNode {
    WordNode *words_head;           // Head of word linked list
    char delimiter;                 // '.', '!', '?', or '\0' for last/incomplete sentence
    pthread_mutex_t sentence_lock;  // Lock for this specific sentence
    int is_locked;                  // Flag to indicate if locked
    char locked_by[MAX_USERNAME];   // Username who holds the lock
    struct SentenceNode *next;      // Next sentence
} SentenceNode;

// File structure - represents entire file in memory
typedef struct LoadedFile {
    char filename[MAX_FILENAME];
    SentenceNode *sentences_head;   // Head of sentence linked list
    int sentence_count;
    int is_loaded;                  // Flag: 1 if loaded, 0 if not
    pthread_rwlock_t file_rwlock;   // Reader-writer lock for READ vs WRITE
    
    // CRITICAL FIX: Reference counting to prevent memory leak on delete
    int refcount;                   // Number of active references to this file
    pthread_mutex_t refcount_lock;  // Protects refcount
    int marked_for_deletion;        // Flag: file deleted but still in memory due to refs
    
    struct LoadedFile *next;        // For hash table chaining or linked list
} LoadedFile;

// File metadata structure (same as before, but separate from in-memory structure)
typedef struct {
    char filename[MAX_FILENAME];
    char owner[MAX_USERNAME];
    char created_time[64];
    char modified_time[64];
    char accessed_time[64];
    long file_size;
    int word_count;
    int char_count;
    char access_list[MAX_DATA_SIZE];
} FileMetadata;

// File Handler Functions
int init_file_handler_ll(const char *storage_dir);
LoadedFile* load_file_into_memory(const char *filename);
int unload_file_from_memory(const char *filename);
LoadedFile* get_file_from_cache(const char *filename);

// CRITICAL FIX: Reference counting functions to prevent memory leak
void file_addref(LoadedFile *file);
void file_release(LoadedFile *file);

int create_file_ll(const char *filename, const char *owner);
int delete_file_ll(const char *filename);
int read_file_ll(const char *filename, char *content, int max_size);
int write_to_file_ll(const char *filename, int sentence_index, int word_index, 
                     const char *content, const char *username);
int lock_sentence_ll(const char *filename, int sentence_index, const char *username);
int unlock_sentence_ll(const char *filename, int sentence_index, const char *username);
int force_unlock_all_sentences_ll(const char *filename);
int sync_file_to_disk(const char *filename);
int file_has_locked_sentences(const char *filename);
int read_file_from_disk_ll(const char *filename, char *content, int max_size);
int undo_file_change_ll(const char *filename);
int get_file_metadata_ll(const char *filename, FileMetadata *metadata);
int update_file_metadata_ll(const char *filename, FileMetadata *metadata);
int update_file_modified_time_ll(const char *filename);
int update_file_statistics_ll(const char *filename);

// CRITICAL FIX: Consolidated metadata update (prevents race condition)
int update_file_write_stats_ll(const char *filename);

int update_file_accessed_time_ll(const char *filename, const char *username);
int has_read_access_ll(const char *filename, const char *username);
int has_write_access_ll(const char *filename, const char *username);
void get_file_path(const char *filename, char *path, size_t size);
void get_undo_path(const char *filename, char *path, size_t size);
int add_user_access_ll(const char *filename, const char *username, int access_type);
int remove_user_access_ll(const char *filename, const char *username);
int get_file_list_ll(char *file_list, int max_size);
int save_undo_backup_ll(const char *filename);
int ensure_sentence_delimiter_ll(const char *filename, int sentence_index);

// Helper functions for write operations
WordNode* create_word_node(const char *word);
SentenceNode* create_sentence_node(char delimiter);

// Global commit lock functions
void lock_commit(void);
void unlock_commit(void);

int load_metadata_ll();
int save_metadata_ll();
int add_metadata_ll(FileMetadata *metadata);
void get_timestamp(char *buffer, size_t size);
void cleanup_file_handler_ll();



// ============================================================================
// STORAGE SERVER CLIENT COMMUNICATION - from ss_client_comm.h
// ============================================================================

// Client Communication Functions
void* handle_client_connection(void *arg);
int handle_read_request(int client_sockfd, Message *msg);
int handle_write_request(int client_sockfd, Message *msg);
int handle_stream_request(int client_sockfd, Message *msg);

// ============================================================================
// STORAGE SERVER NAME MANAGER COMMUNICATION - from ss_nm_comm.h
// ============================================================================

// Name Manager Communication Functions
int register_with_nm(int nm_port, int client_port, const char *ss_ip);
void* handle_nm_connection(void *arg);
int send_ack_to_nm(int nm_sockfd, int operation, int error_code);

// ============================================================================
// STORAGE SERVER MAIN - from storage_server.h
// ============================================================================

// Global configuration
typedef struct {
    char nm_ip[MAX_IP_LEN];      // Name Server IP address
    int nm_port;           // Port for Name Server connections
    int client_port;       // Port for Client connections
    char storage_dir[MAX_PATH];  // Directory to store files
    char ss_ip[MAX_IP_LEN];      // Storage Server IP
    int nm_sockfd;         // Socket for NM connection
    int client_sockfd;     // Socket for client connections
    int ss_id;             // Storage Server ID (1, 2, 3, ...)
} SSConfig;

// Global server configuration
extern SSConfig server_config;

// Thread argument structure
typedef struct {
    int sockfd;
    char client_ip[MAX_IP_LEN];
} ThreadArg;

// Storage Server Main Functions
int init_storage_server(int nm_port, int client_port, const char *storage_dir, const char *override_ip);
int start_server();
void* handle_nm_connections(void *arg);
void* handle_client_connections(void *arg);
void shutdown_server();
void signal_handler(int signum);

#endif // STORAGE_SERVER_ALL_H
