# Complete Request Flow Guide: From Connection to Response

## Overview

This guide traces the complete journey of an HTTP request through the webserver, from initial connection to final response, covering both regular file serving and CGI execution paths.

---

## 🚀 Plain-English Quick Start (Use This During Evaluation)

If the evaluator asks rapid questions, start from here. Short, direct, friendly answers. No fluff.

### 🔁 High-Level Flow (One Sentence)
Client connects → we accept → read request → parse → decide (static file or CGI) → build response → send → close.

### 🧱 Core Building Blocks (Very Simple)
| Thing | What It Is (Simple) | Why We Use It |
|-------|----------------------|---------------|
| Socket | A door for network talk | To let clients connect |
| epoll | A watcher of many sockets | To handle lots of clients fast |
| Non-blocking | "Never wait, just check" mode | So one slow client can't freeze us |
| fork | Make a copy of the server process | To safely run CGI scripts |
| execve | Replace the copy with the script | To run the script cleanly |
| pipe | One-way tube | To send data to/from CGI |
| Request parser | Breaks raw text into method, path, headers, body | To understand what client wants |
| MIME map | Extension → content-type | So browser knows how to display file |

### ⚙️ Architecture In One Breath
Single process, event loop with epoll, non-blocking sockets, per-client state, optional fork+exec for CGI, everything cleaned up when done.

### 🧪 Most Common Evaluation Questions & Model Answers

1. Q: Why epoll instead of select?  A: epoll scales to thousands of connections without scanning every socket; select must scan a fixed-size set each time.
2. Q: What does non-blocking mean?  A: Calls return immediately; if they can't do work now they say "try later" (EAGAIN), so we stay responsive.
3. Q: When do you call accept()?  A: Only when epoll tells us the listening socket is ready; then we loop accept until it returns EAGAIN.
4. Q: What happens after accept()?  A: Set the new socket non‑blocking, add it to epoll, create a client object.
5. Q: How do you parse the request?  A: Collect bytes into a buffer, find request line, then headers up to blank line, then body (Content-Length or chunked).
6. Q: How do you prevent giant requests?  A: Hard limits on total size, headers size, line length; exceed → send proper error (413/431).
7. Q: How do you detect end of headers?  A: Look for "\r\n\r\n" (or "\n\n" fallback if malformed).
8. Q: Why fork for CGI?  A: Isolation—script can't crash or corrupt server memory; child process dies independently.
9. Q: Why exec after fork?  A: Fork copies us; exec replaces the child with the script interpreter for a clean environment.
10. Q: How do you pass data to CGI?  A: Via environment variables (REQUEST_METHOD, QUERY_STRING...) and body through pipe → stdin.
11. Q: How do you read CGI output?  A: Non-blocking read on pipe (stdout of child) when epoll says it's ready.
12. Q: How do you prevent CGI hang?  A: Track start time; kill after timeout (e.g. 30s) → return 504.
13. Q: What if client disconnects while sending?  A: recv returns 0 (graceful) or error like ECONNRESET; we close and clean state.
14. Q: What does recv() returning 0 mean?  A: Peer closed the connection normally—no more data.
15. Q: What does EAGAIN mean?  A: No data (for read) or can't send now (for write); we just move on.
16. Q: How do you handle partial send?  A: Loop until all bytes sent or error; track offset.
17. Q: Why edge-triggered epoll?  A: Fewer wakeups; we drain everything once per readiness notification.
18. Q: Risk with edge-triggered?  A: If you don't read until EAGAIN you might never get another event (starvation).
19. Q: How do you avoid zombie processes?  A: waitpid() after CGI ends (or SIGCHLD handler in extended version).
20. Q: How do you block directory traversal?  A: Reject paths containing "../" before accessing filesystem.
21. Q: How do you pick MIME type?  A: Use file extension lookup; default to application/octet-stream.
22. Q: How do you build HTTP response?  A: Status line + headers (Content-Type, Length, Connection) + blank line + body.
23. Q: Why close after response?  A: Simplicity (no keep-alive) and spec default if not negotiating persistent connection.
24. Q: What if accept fails with EMFILE?  A: Log, possibly close a spare emergency FD, refuse new connection gracefully.
25. Q: Why use limits on headers?  A: Stop header abuse (slowloris / memory exhaustion).
26. Q: What's the difference between send and write?  A: send adds flags for sockets; write is generic.
27. Q: Is TCP message-based?  A: No, it's a byte stream; we define structure with delimiters (CRLF).
28. Q: How chunked transfer parsed?  A: Read chunk size line (hex) → read that many bytes → repeat until 0 chunk.
29. Q: How do you ensure no FD leaks?  A: Close unused pipe ends, epoll_ctl DEL on removal, RAII patterns where possible.
30. Q: What happens if execve fails?  A: Child logs (perror), exits with non-zero; parent detects no output or exit status → 500 error.
31. Q: How do you map internal errors to HTTP?  A: Parsing errors → 400; not found → 404; permission/config → 403/405; size issues → 413/431; CGI timeout → 504; unexpected server failure → 500.
32. Q: Why convert port with htons?  A: Network byte order standard (big-endian) ensures portability.
33. Q: How do you know if path is directory?  A: stat() then S_ISDIR; if directory and autoindex on → generate listing.
34. Q: Why environment variable names uppercase with HTTP_ prefix?  A: CGI spec expects that transformation for headers.
35. Q: Security pillars?  A: Isolation (fork/exec), input limits, path sanitization, timeout, non-blocking to resist slow clients.
36. Q: How would you add keep-alive later?  A: Parse Connection header, maintain state machine, reuse socket before closing on timeout.
37. Q: What prevents memory leaks?  A: Use std::string/std::map, delete none manually except allocated buffers, and cleanup on disconnect.
38. Q: How do you detect end of request body with Content-Length?  A: Track bytes read vs declared length; done when equal.
39. Q: What if Content-Length missing on POST?  A: Treat as bad request (400) unless chunked encoding present.
40. Q: Why not threads?  A: epoll with non-blocking I/O is simpler and avoids context-switch overhead; single loop is sufficient for our goals.

### 🧩 Plain-Language Mini Glossary
| Term | Plain Meaning |
|------|---------------|
| Backlog | Waiting room of not-yet-accepted connections |
| Non-blocking | Always return fast; never sit and wait |
| Edge-triggered | "Notify me once when it changes" style |
| Zombie process | Dead child not reaped yet |
| CGI | Run an external script to build a response |
| Header | Key:Value line giving extra request info |
| Body | Optional data after headers (e.g. form upload) |
| Timeout | Safety clock to stop stuck code |
| MIME type | Label telling browser how to treat data |
| 3-way handshake | Client SYN → Server SYN+ACK → Client ACK |

### 🛡️ Fast Security Summary
Limit size, sanitize paths, isolate scripts, time out long jobs, close everything you don't need.

### 🧠 Memory Safety Points
No manual malloc/free in hot path; C++ standard containers manage memory; close descriptors early; short-lived child process for CGI.

### 🧵 Concurrency Story
One loop, many sockets, epoll tells us which are ready, we only touch ready ones—constant overhead per event, not per connection.

---

## 🎯 **EVALUATION FOCUS AREAS**

### **Critical Implementation Points for Evaluation:**
1. **Non-blocking I/O Architecture** - How epoll prevents server blocking
2. **CGI Process Management** - Fork/exec/wait lifecycle with proper cleanup
3. **HTTP Protocol Compliance** - Request parsing, response formatting, status codes
4. **Error Handling** - Graceful handling of all error conditions
5. **Resource Management** - File descriptor cleanup, memory management
6. **Concurrency** - Handling multiple clients simultaneously
7. **Security** - Input validation, timeout handling, resource limits

### **Key Technical Decisions Explained:**
- **Why epoll over select/poll**: O(1) performance vs O(n)
- **Why non-blocking sockets**: Prevents one slow client from blocking others
- **Why fork for CGI**: Process isolation and security
- **Why pipes for CGI communication**: Secure inter-process communication
- **Why timeout handling**: Prevents resource exhaustion attacks

## Flow Diagram

```
Client Connection → Server Accept → Request Parsing → Route Decision → Response Generation → Client Response
     ↓                    ↓              ↓               ↓                    ↓                  ↓
[socket_setup.cpp]  [server.cpp]   [request.cpp]   [CGI/Handler]      [response.cpp]    [client.cpp]
```

---

## Step 1: Initial Connection Setup

### 1.1 Server Socket Creation

**File**: [Server_setup/socket_setup.cpp](Server_setup/socket_setup.cpp)
**Function**: Socket initialization and binding

```cpp
// Server creates listening socket and binds to port
int server_socket = socket(AF_INET, SOCK_STREAM, 0);
bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
listen(server_socket, SOMAXCONN);
```

#### ❓ Quick Q&A: What Are `socket()`, `bind()`, and `listen()`?
| Call | Kernel Action | Why We Need It | Simple Analogy |
|------|---------------|----------------|----------------|
| `socket()` | Allocates a protocol control block + FD | Creates an endpoint for communication | Getting a new powered-off phone |
| `bind()` | Associates FD with local (IP,port) | So clients know where to connect | Registering your phone number |
| `listen()` | Marks socket passive; allocates accept queues | Allows accepting multiple pending connections | Turning ringer on and adding a waiting room |
| `accept()` | Removes one established connection from queue | Creates per-client FD | Answering a specific call |

Concise flow: `socket()` -> set options/non-blocking -> `bind()` -> `listen()` -> event loop waits -> `accept()` per client.

#### 🔍 **EVALUATION DETAILS:**

**Socket Creation Deep Dive:**
```cpp
int server_socket = socket(AF_INET, SOCK_STREAM, 0);
```
- **AF_INET**: IPv4 address family (evaluator may ask why not AF_INET6)
- **SOCK_STREAM**: TCP protocol (reliable, connection-oriented)
- **0**: Let system choose protocol (TCP for SOCK_STREAM)
- **Return**: File descriptor number or -1 on error
- **Error Handling**: Must check return value and handle EMFILE, ENFILE, ENOBUFS

**Address Binding:**
```cpp
struct sockaddr_in server_addr;
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
server_addr.sin_port = htons(port);        // Convert to network byte order

int result = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
```
- **INADDR_ANY**: Binds to all available network interfaces (0.0.0.0)
- **htons()**: Host to Network Short - converts port to big-endian
- **Why bind()**: Associates socket with specific address/port
- **Common Errors**: EADDRINUSE (port already in use), EACCES (permission denied)

**Socket Options (Critical for Production):**
```cpp
int opt = 1;
// Allow address reuse (prevents "Address already in use" error)
setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

// Set socket to non-blocking mode
int flags = fcntl(server_socket, F_GETFL, 0);
fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
```

**Listen Queue Setup:**
```cpp
listen(server_socket, SOMAXCONN);
```
- **SOMAXCONN**: Maximum backlog queue size (typically 128)
- **Backlog Queue**: Holds pending connections before accept()
- **Why Important**: Prevents connection drops under high load

### 1.2 Epoll Setup

**File**: [Server_setup/epoll_setup.cpp](Server_setup/epoll_setup.cpp)
**Function**: Event monitoring initialization

```cpp
// Create epoll instance for I/O multiplexing
int epoll_fd = epoll_create1(0);
struct epoll_event event;
event.events = EPOLLIN;
event.data.fd = server_socket;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event);
```

#### 🔍 **EVALUATION DETAILS:**

**Why Epoll Over Alternatives:**
```cpp
// OLD WAY (select) - O(n) complexity, limited to 1024 fds
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(server_socket, &readfds);
select(max_fd + 1, &readfds, NULL, NULL, NULL);  // Scans ALL file descriptors

// NEW WAY (epoll) - O(1) complexity, unlimited fds
int epoll_fd = epoll_create1(EPOLL_CLOEXEC);  // Close on exec
```

**Epoll Creation Options:**
- **epoll_create1(0)**: Basic epoll instance
- **epoll_create1(EPOLL_CLOEXEC)**: Closes epoll fd when exec() is called
- **Why CLOEXEC**: Prevents CGI processes from inheriting epoll fd

**Event Structure Deep Dive:**
```cpp
struct epoll_event {
    uint32_t events;    // Event mask (what to watch for)
    epoll_data_t data;  // User data (usually file descriptor)
};

union epoll_data_t {
    void *ptr;     // Pointer to user data
    int fd;        // File descriptor
    uint32_t u32;  // 32-bit integer
    uint64_t u64;  // 64-bit integer
};
```

**Event Types Explained:**
```cpp
event.events = EPOLLIN;                    // Data available for reading
event.events = EPOLLIN | EPOLLOUT;         // Read OR write ready
event.events = EPOLLIN | EPOLLET;          // Edge-triggered mode
event.events = EPOLLIN | EPOLLONESHOT;     // One-shot mode
```

**Edge-Triggered vs Level-Triggered (Critical Concept):**
```cpp
// Level-Triggered (default): Event fires while condition is true
// - If data is available, epoll_wait() returns every time
// - Easier to use but potentially less efficient

// Edge-Triggered (EPOLLET): Event fires when condition changes
// - Only fires when data BECOMES available
// - Must read ALL available data in one go
// - More efficient but harder to implement correctly

// Edge-triggered example:
while (true) {
    ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        if (errno == EAGAIN) break;  // No more data
        // Handle error
    }
    // Process data
}
```

**Epoll Performance Characteristics:**
- **Time Complexity**: O(1) for add/remove/wait operations
- **Memory Usage**: O(number of watched file descriptors)
- **Scalability**: Can handle 100,000+ concurrent connections
- **System Calls**: Minimal - only when events actually occur

---

## Step 2: Connection Acceptance

### 2.1 New Client Connection

**File**: [Server_setup/server.cpp](Server_setup/server.cpp#L100-L150)
**Function**: `handle_events()` - Main event loop

```cpp
void Server::handle_events() {
    struct epoll_event events[MAX_EVENTS];
    int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    for (int i = 0; i < num_events; i++) {
        int fd = events[i].data.fd;

        if (is_server_socket(fd)) {
            // Accept new connection
            int client_fd = accept(fd, NULL, NULL);

            // Set non-blocking
            fcntl(client_fd, F_SETFL, O_NONBLOCK);

            // Add to epoll
            struct epoll_event client_event;
            client_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
            client_event.data.fd = client_fd;
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);

            // Create client object
            active_clients[client_fd] = Client(client_fd);
        }
    }
}
```

#### 🔍 **EVALUATION DETAILS - MAIN EVENT LOOP:**

**Complete Event Loop Architecture:**
```cpp
void Server::handle_events() {
    struct epoll_event events[MAX_EVENTS];  // Event array (typically 1024)
    
    while (server_running) {  // Main server loop
        // Wait for events (blocks until something happens)
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        if (num_events == -1) {
            if (errno == EINTR) continue;  // Interrupted by signal, retry
            perror("epoll_wait failed");
            break;
        }
        
        // Process each ready file descriptor
        for (int i = 0; i < num_events; i++) {
            int fd = events[i].data.fd;
            uint32_t event_mask = events[i].events;
            
            // Route to appropriate handler
            if (is_server_socket(fd)) {
                handle_new_connection(fd);
            } else if (is_client_socket(fd)) {
                handle_client_event(fd, event_mask);
            } else if (is_cgi_socket(fd)) {
                handle_cgi_event(fd, event_mask);
            } else {
                // Unknown fd - should not happen
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }
    }
}
```

**Event Routing Logic (Critical for Evaluation):**
```cpp
bool Server::is_server_socket(int fd) {
    // Check if fd matches any listening socket
    for (size_t i = 0; i < server_sockets.size(); i++) {
        if (server_sockets[i] == fd) return true;
    }
    return false;
}

bool Server::is_client_socket(int fd) {
    // Check if fd exists in active clients map
    return active_clients.find(fd) != active_clients.end();
}

bool Server::is_cgi_socket(int fd) {
    // Check if fd belongs to CGI runner
    return cgi_runner.is_cgi_fd(fd);
}
```

**Connection Acceptance Deep Dive:**
```cpp
void Server::handle_new_connection(int server_fd) {
    while (true) {  // Accept all pending connections
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // No more pending connections
            } else if (errno == EMFILE || errno == ENFILE) {
                // Too many open files - log error but continue
                std::cerr << "Warning: Too many open files, dropping connection\n";
                break;
            } else {
                perror("accept failed");
                break;
            }
        }
        
        // Set client socket to non-blocking
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags == -1 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl failed");
            close(client_fd);
            continue;
        }
        
        // Add to epoll with edge-triggered mode
        struct epoll_event client_event;
        client_event.events = EPOLLIN | EPOLLET;  // Start with read-only
        client_event.data.fd = client_fd;
        
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1) {
            perror("epoll_ctl ADD failed");
            close(client_fd);
            continue;
        }
        
        // Create client object and store
        active_clients[client_fd] = Client(client_fd);
        
        // Log connection (optional)
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "New connection from " << client_ip << ":" 
                  << ntohs(client_addr.sin_port) << " on fd " << client_fd << std::endl;
    }
}
```

**Why Edge-Triggered Mode (EPOLLET):**
1. **Performance**: Only notified when state changes, not while state persists
2. **Efficiency**: Reduces number of epoll_wait() calls
3. **Scalability**: Better for high-concurrency servers
4. **Requirement**: Must read/write ALL available data when notified

**Error Handling in Main Loop:**
- **EINTR**: Signal interrupted epoll_wait() - retry
- **EMFILE/ENFILE**: Too many open files - log and continue
- **EBADF**: Bad file descriptor - remove from epoll
- **Connection drops**: Handle gracefully without crashing server

### 🔧 Key Functions Explained:

#### `execve()` - Execute Program (Clean Reference)
```cpp
#include <unistd.h>
int execve(const char *pathname, char *const argv[], char *const envp[]);
```
Purpose: Replace current process image with a new program. On success never returns.

Key points:
- `argv` and `envp` must be NULL-terminated.
- Inherit open FDs unless marked FD_CLOEXEC (reason we prefer EPOLL_CLOEXEC / fcntl(F_SETFD)).
- PID, cwd, umask persist; memory image & signal handlers replaced.

Example:
```cpp
char *args[] = { (char*)"/usr/bin/python3", (char*)"script.py", (char*)"arg1", NULL };
char *envp[] = { (char*)"REQUEST_METHOD=GET", (char*)"QUERY_STRING=name=value", NULL };
execve("/usr/bin/python3", args, envp);
perror("execve failed");
_exit(1);
```

Common errors: E2BIG (args+env too large), EACCES (permissions), ENOENT (path/interpreter missing), ENOEXEC (bad format), ETXTBSY (binary being written), ENOMEM.

Evaluation phrase: "fork isolates by copy-on-write; execve replaces memory so CGI can't corrupt server state; only sanitized environment passed." 

struct sockaddr_in client_addr;
socklen_t client_len = sizeof(client_addr);
int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

if (client_fd >= 0) {
    // Success - we have a new connection
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    printf("New connection from %s:%d on fd %d\n", client_ip, client_port, client_fd);
} else {
    // Error occurred
    perror("accept failed");
}
```

**Error Handling**:
- `EAGAIN`/`EWOULDBLOCK`: No pending connections (non-blocking socket)
- `EBADF`: Invalid socket descriptor
- `ECONNABORTED`: Connection aborted by client
- `EINTR`: Interrupted by signal
- `EMFILE`: Too many open files (per-process limit)
- `ENFILE`: Too many open files (system-wide limit)

**Important Notes**:
- **Waiting vs Not waiting**: Normal sockets wait for someone to connect. Fast sockets return right away with `EAGAIN` if no one is waiting
- **Copy settings**: The new connection copies settings from the main socket but you can change them
- **Waiting line**: The main socket keeps a list of people waiting to connect

#### `fcntl()` - File Control Operations
```cpp
#include <fcntl.h>
int fcntl(int fd, int cmd, ...);
```

**Function Signature Deep Dive**:
- **Return Type**: `int` - Depends on command, usually 0 on success, -1 on error
- **Parameter 1**: `int fd` - File descriptor to operate on
- **Parameter 2**: `int cmd` - Command to execute
- **Parameter 3**: `...` - Additional arguments depending on command

**What it does**: Changes settings on files and sockets
**Simple explanation**: Like changing settings on your phone - you can make it work differently

**Common Commands in Our Server**:
```cpp
// Get current flags
int flags = fcntl(client_fd, F_GETFL, 0);
if (flags == -1) {
    perror("fcntl F_GETFL failed");
    return -1;
}

// Set non-blocking mode
if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL failed");
    return -1;
}

// Alternative: Set non-blocking in one call
if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1) {
    perror("fcntl failed");
    return -1;
}
```

**Detailed Flag Explanations**:
- `O_NONBLOCK`: Non-blocking I/O - operations return immediately if they would block
- `O_ASYNC`: Asynchronous I/O - generate signal when I/O is possible
- `O_APPEND`: Always append to end of file
- `O_SYNC`: Synchronous writes - wait for data to be physically written

**Why Fast Mode is Important for Servers**:
```cpp
// Slow mode (BAD for servers):
ssize_t bytes = recv(fd, buffer, size, 0);  // Waits forever if no data

// Fast mode (GOOD for servers):
ssize_t bytes = recv(fd, buffer, size, 0);
if (bytes == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No data right now, check other clients
        return;
    } else {
        // Real error happened
        perror("recv failed");
    }
}
```

**Error Handling**:
- `EBADF`: Invalid file descriptor
- `EINVAL`: Invalid command or arguments
- `EACCES`: Permission denied
- `EAGAIN`: Resource temporarily unavailable

#### `epoll_ctl()` - Control Epoll Instance

```cpp
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);
```

**What it does**: Tells our "security guard" to start watching this new door
**Simple explanation**: Adding a new door to the guard's watch list
**Operations**:

- `EPOLL_CTL_ADD`: Start watching this file descriptor
- `EPOLL_CTL_DEL`: Stop watching this file descriptor
- `EPOLL_CTL_MOD`: Change what we're watching for

````

---

## Step 3: Request Data Reception

### 3.1 Client Data Input Handling
**File**: [client/client.cpp](client/client.cpp#L50-L100)
**Function**: `handle_client_data_input()`

```cpp
void Client::handle_client_data_input(int epoll_fd, std::map<int, Client> &active_clients,
                                     ServerContext& server_config, CgiRunner& cgi_runner) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';

        // Add data to request parser
        RequestStatus status = current_request.add_new_data(buffer, bytes_received);

        if (status == EVERYTHING_IS_OK) {
            // Request is complete, process it
            process_complete_request(epoll_fd, active_clients, server_config, cgi_runner);
        }
        else if (status == NEED_MORE_DATA) {
            // Wait for more data
            return;
        }
        else {
            // Error in request
            send_error_response(400);
        }
    }
}
````

### 🔧 Key Functions Explained:

#### `recv()` - Receive Data from Client
```cpp
#include <sys/socket.h>
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

**Function Signature Deep Dive**:
- **Return Type**: `ssize_t` - Number of bytes received, 0 if peer closed connection, -1 on error
- **Parameter 1**: `int sockfd` - Socket file descriptor to read from
- **Parameter 2**: `void *buf` - Buffer to store received data
- **Parameter 3**: `size_t len` - Maximum number of bytes to receive
- **Parameter 4**: `int flags` - Flags to modify behavior

**What it does**: Gets data that someone sent to you
**Simple explanation**: Like checking your mailbox - you see if there are any letters waiting

**Detailed Behavior**:
```cpp
char buffer[4096];
ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

if (bytes_received > 0) {
    // Success - we received data
    buffer[bytes_received] = '\0';  // Null-terminate for string operations
    printf("Received %zd bytes: %s\n", bytes_received, buffer);
    
} else if (bytes_received == 0) {
    // Peer closed the connection gracefully
    printf("Client disconnected\n");
    close(client_fd);
    
} else {  // bytes_received == -1
    // Error occurred - check errno
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No data available right now (non-blocking socket)
        printf("No data available, try again later\n");
    } else if (errno == ECONNRESET) {
        // Connection reset by peer (client crashed/killed)
        printf("Connection reset by peer\n");
        close(client_fd);
    } else {
        // Other error
        perror("recv failed");
        close(client_fd);
    }
}
```

**Flag Options**:
- `0`: Normal operation (most common)
- `MSG_DONTWAIT`: Non-blocking operation (even on blocking socket)
- `MSG_PEEK`: Peek at data without removing it from queue
- `MSG_WAITALL`: Wait for full request or error
- `MSG_OOB`: Receive out-of-band data

**Advanced Usage Examples**:
```cpp
// Peek at data without consuming it
ssize_t peek_bytes = recv(fd, buffer, sizeof(buffer), MSG_PEEK);
if (peek_bytes > 0) {
    printf("Next %zd bytes would be: %.*s\n", peek_bytes, (int)peek_bytes, buffer);
    // Data is still in the socket buffer
}

// Try to receive exactly N bytes
ssize_t total_received = 0;
size_t expected_size = 1024;
while (total_received < expected_size) {
    ssize_t bytes = recv(fd, buffer + total_received, 
                        expected_size - total_received, MSG_WAITALL);
    if (bytes <= 0) break;
    total_received += bytes;
}
```

**Error Handling**:
- `EAGAIN`/`EWOULDBLOCK`: No data available (non-blocking socket)
- `EBADF`: Invalid socket descriptor
- `ECONNREFUSED`: Connection refused by peer
- `ECONNRESET`: Connection reset by peer
- `EINTR`: Interrupted by signal
- `EINVAL`: Invalid arguments
- `ENOMEM`: Insufficient memory
- `ENOTCONN`: Socket not connected
- `ENOTSOCK`: File descriptor is not a socket

**Speed Tips**:
- **Buffer Size**: Big buffers are faster but use more memory
- **Partial Reads**: `recv()` might give you less data than you asked for
- **TCP Behavior**: TCP is like a stream - no message breaks
- **Fast mode**: Important for servers so slow clients don't stop everything

**Why BUFFER_SIZE - 1**: We reserve one byte for null terminator (`\0`) to safely treat received data as a C string

````

---

## Step 4: Request Parsing and Validation

### 4.1 HTTP Request Parsing
**File**: [request/request.cpp](request/request.cpp#L1-L100)
**Function**: `add_new_data()`

#### 🔍 **EVALUATION DETAILS - HTTP PROTOCOL COMPLIANCE:**

**HTTP/1.1 Request Format (RFC 7230):**
```
Request-Line CRLF
*(header-field CRLF)
CRLF
[message-body]
```

**Complete Request Parsing Implementation:**
```cpp
RequestStatus Request::add_new_data(const char *new_data, size_t data_size) {
    // Append new data to buffer
    incoming_data.append(new_data, data_size);
    
    // Check for maximum request size (prevent DoS attacks)
    if (incoming_data.size() > MAX_REQUEST_SIZE) {
        return REQUEST_TOO_LARGE;  // 413 Payload Too Large
    }

    // Parse request line (GET /path HTTP/1.1)
    if (method.empty()) {
        size_t line_end = incoming_data.find("\r\n");
        if (line_end != std::string::npos) {
            std::string request_line = incoming_data.substr(0, line_end);
            RequestStatus status = parse_request_line(request_line);
            if (status != EVERYTHING_IS_OK) {
                return status;  // 400 Bad Request
            }
        } else if (incoming_data.size() > MAX_REQUEST_LINE_SIZE) {
            return REQUEST_LINE_TOO_LARGE;  // 414 URI Too Long
        }
    }

    // Parse headers
    if (!headers_complete && !method.empty()) {
        size_t headers_end = incoming_data.find("\r\n\r\n");
        if (headers_end != std::string::npos) {
            std::string headers_section = incoming_data.substr(
                incoming_data.find("\r\n") + 2, 
                headers_end - incoming_data.find("\r\n") - 2
            );
            
            RequestStatus status = parse_headers(headers_section);
            if (status != EVERYTHING_IS_OK) {
                return status;  // 400 Bad Request or 431 Request Header Fields Too Large
            }
            
            headers_complete = true;
            body_start_pos = headers_end + 4;
        } else if (incoming_data.size() > MAX_HEADERS_SIZE) {
            return HEADER_TOO_LARGE;  // 431 Request Header Fields Too Large
        }
    }

    // Handle request body (for POST/PUT requests)
    if (headers_complete) {
        if (method == "POST" || method == "PUT") {
            return handle_request_body();
        } else {
            return EVERYTHING_IS_OK;  // GET/DELETE don't have body
        }
    }

    return NEED_MORE_DATA;  // Need more data to complete request
}
```

**Request Line Parsing (Critical for Evaluation):**
```cpp
RequestStatus Request::parse_request_line(const std::string& line) {
    std::istringstream iss(line);
    std::string http_version;
    
    // Parse: METHOD PATH HTTP/1.1
    if (!(iss >> method >> requested_path >> http_version)) {
        return BAD_REQUEST;  // Malformed request line
    }
    
    // Validate HTTP method
    if (method != "GET" && method != "POST" && method != "DELETE") {
        return NOT_IMPLEMENTED;  // 501 Not Implemented
    }
    
    // Validate HTTP version
    if (http_version != "HTTP/1.1" && http_version != "HTTP/1.0") {
        return HTTP_VERSION_NOT_SUPPORTED;  // 505 HTTP Version Not Supported
    }
    
    // Validate and decode URL
    if (requested_path.empty() || requested_path[0] != '/') {
        return BAD_REQUEST;  // Path must start with /
    }
    
    // URL decode the path
    requested_path = url_decode(requested_path);
    
    // Check for directory traversal attacks
    if (requested_path.find("../") != std::string::npos || 
        requested_path.find("..\\") != std::string::npos) {
        return BAD_REQUEST;  // Security: prevent directory traversal
    }
    
    return EVERYTHING_IS_OK;
}
```

**Header Parsing with Validation:**
```cpp
RequestStatus Request::parse_headers(const std::string& headers_section) {
    std::istringstream stream(headers_section);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.empty()) continue;
        
        // Find header separator
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            return BAD_REQUEST;  // Malformed header
        }
        
        std::string name = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        // Trim whitespace
        name = trim(name);
        value = trim(value);
        
        // Convert header name to lowercase for case-insensitive comparison
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        // Validate header name (RFC 7230)
        if (!is_valid_header_name(name)) {
            return BAD_REQUEST;
        }
        
        // Store header
        headers[name] = value;
        
        // Special handling for important headers
        if (name == "host") {
            host_header = value;
        } else if (name == "content-length") {
            if (!is_valid_number(value)) {
                return BAD_REQUEST;
            }
            content_length = std::atoi(value.c_str());
        } else if (name == "transfer-encoding") {
            if (value == "chunked") {
                is_chunked = true;
            }
        }
    }
    
    // HTTP/1.1 requires Host header
    if (http_version == "HTTP/1.1" && host_header.empty()) {
        return BAD_REQUEST;  // 400 Bad Request - missing Host header
    }
    
    return EVERYTHING_IS_OK;
}
```

**Request Body Handling:**
```cpp
RequestStatus Request::handle_request_body() {
    if (is_chunked) {
        return parse_chunked_body();  // Handle Transfer-Encoding: chunked
    } else if (content_length > 0) {
        // Handle Content-Length body
        if (content_length > MAX_BODY_SIZE) {
            return REQUEST_TOO_LARGE;  // 413 Payload Too Large
        }
        
        size_t available_body = incoming_data.size() - body_start_pos;
        if (available_body >= content_length) {
            request_body = incoming_data.substr(body_start_pos, content_length);
            return EVERYTHING_IS_OK;
        } else {
            return NEED_MORE_DATA;
        }
    } else {
        // No body expected
        return EVERYTHING_IS_OK;
    }
}
```

**HTTP Status Codes Implemented:**
- **200 OK**: Successful request
- **201 Created**: Successful POST/PUT
- **400 Bad Request**: Malformed request
- **403 Forbidden**: Access denied
- **404 Not Found**: Resource not found
- **405 Method Not Allowed**: Unsupported method
- **413 Payload Too Large**: Request too big
- **414 URI Too Long**: URL too long
- **431 Request Header Fields Too Large**: Headers too big
- **500 Internal Server Error**: Server error
- **501 Not Implemented**: Method not supported
- **504 Gateway Timeout**: CGI timeout
- **505 HTTP Version Not Supported**: Unsupported HTTP version`

### 4.2 Location Matching

**File**: [request/request.cpp](request/request.cpp#L200-L250)
**Function**: `match_location()`

```cpp
LocationContext *Request::match_location(const std::string &requested_path) {
    // Strip query string for location matching
    std::string path_only = requested_path;
    size_t query_pos = path_only.find('?');
    if (query_pos != std::string::npos) {
        path_only = path_only.substr(0, query_pos);
    }

    // Find best matching location
    LocationContext* best_match = NULL;
    size_t best_match_length = 0;

    for (size_t i = 0; i < server_context->locations.size(); i++) {
        LocationContext& loc = server_context->locations[i];

        if (path_only.find(loc.path) == 0) {  // Path starts with location path
            if (loc.path.length() > best_match_length) {
                best_match = &loc;
                best_match_length = loc.path.length();
            }
        }
    }

    return best_match;
}
```

---

## Step 5: Route Decision Point

### 5.1 CGI Request Detection

**File**: [request/request.cpp](request/request.cpp#L300-L330)
**Function**: `is_cgi_request()`

```cpp
bool Request::is_cgi_request() const {
    if (!location || location->cgiExtension.empty() || location->cgiPath.empty()) {
        return false;
    }

    // Check if the requested path ends with the CGI extension
    std::string path = requested_path;
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }

    // Check file extension
    if (path.size() >= location->cgiExtension.size()) {
        std::string file_ext = path.substr(path.size() - location->cgiExtension.size());
        return file_ext == location->cgiExtension;
    }

    return false;
}
```

---

## Step 6A: CGI Request Processing Path

### 6A.1 CGI Process Startup

**File**: [client/client.cpp](client/client.cpp#L150-L200)
**Function**: CGI request handling block

```cpp
// Check if this is a CGI request
if (current_request.is_cgi_request()) {
    std::cout << "🔧 Detected CGI request - starting CGI process" << std::endl;
    LocationContext* location = current_request.get_location();

    if (location) {
        // Build script path
        std::string script_path = location->root + current_request.get_requested_path();
        size_t query_pos = script_path.find('?');
        if (query_pos != std::string::npos) {
            script_path = script_path.substr(0, query_pos);
        }

        // Start CGI process
        int cgi_output_fd = cgi_runner.start_cgi_process(current_request, *location,
                                                        client_fd, script_path);

        if (cgi_output_fd > 0) {
            // Add CGI output fd to epoll for monitoring
            struct epoll_event cgi_event;
            cgi_event.events = EPOLLIN;
            cgi_event.data.fd = cgi_output_fd;
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi_output_fd, &cgi_event);

            std::cout << "✅ CGI process started, monitoring fd " << cgi_output_fd << std::endl;
        }
    }
    return; // Exit early for CGI requests
}
```

### 6A.2 CGI Process Execution

**File**: [cgi/cgi_runner.cpp](cgi/cgi_runner.cpp#L50-L150)
**Function**: `start_cgi_process()`

#### 🔍 **EVALUATION DETAILS - CGI IMPLEMENTATION:**

**Complete CGI Architecture Overview:**
```cpp
class CgiRunner {
private:
    std::map<int, CgiProcess> active_cgi_processes;  // fd -> process mapping
    
public:
    int start_cgi_process(const Request& request, const LocationContext& location,
                         int client_fd, const std::string& script_path);
    bool handle_cgi_output(int fd, std::string& response_data);
    void cleanup_cgi_process(int fd);
    bool is_cgi_fd(int fd) const;
    int get_client_fd(int cgi_fd) const;
};

struct CgiProcess {
    pid_t pid;                    // Child process ID
    int input_fd;                 // Pipe to write to CGI stdin
    int output_fd;                // Pipe to read from CGI stdout
    int client_fd;                // Associated client connection
    std::string script_path;      // Path to the CGI script
    bool finished;                // Process completion status
    std::string output_buffer;    // Accumulated output
    time_t start_time;           // For timeout handling (30 seconds)
};
```

**CGI/1.1 Specification Compliance:**
1. **Environment Variables**: REQUEST_METHOD, QUERY_STRING, CONTENT_LENGTH, etc.
2. **Input/Output**: stdin for request body, stdout for response
3. **Working Directory**: Changed to script directory for relative paths
4. **Process Isolation**: Each CGI runs in separate process
5. **Timeout Handling**: 30-second limit to prevent hanging
6. **Error Handling**: Proper HTTP error responses for CGI failures

**Security Considerations (Critical for Evaluation):**
- **Process Isolation**: CGI runs in child process, can't affect server
- **Resource Limits**: Timeout prevents infinite loops
- **Path Validation**: Script path must be within allowed directories
- **Environment Sanitization**: Only safe environment variables passed
- **File Descriptor Cleanup**: All unused fds closed in child process

### 🔧 Key Functions Explained Before Code:

#### `pipe()` - Create Communication Channels
```cpp
#include <unistd.h>
int pipe(int pipefd[2]);
```

**Function Signature Deep Dive**:
- **Return Type**: `int` - 0 on success, -1 on error
- **Parameter**: `int pipefd[2]` - Array to store the two file descriptors

**What it does**: Makes a tube for sending data from one place to another
**Simple explanation**: Like making a tube where you put something in one end and it comes out the other end

**Detailed Behavior**:
```cpp
int input_pipe[2], output_pipe[2];

// Create pipes
if (pipe(input_pipe) == -1) {
    perror("input pipe creation failed");
    return -1;
}
if (pipe(output_pipe) == -1) {
    perror("output pipe creation failed");
    close(input_pipe[0]);
    close(input_pipe[1]);
    return -1;
}

// After successful pipe() calls:
// input_pipe[0] = read end of input pipe
// input_pipe[1] = write end of input pipe
// output_pipe[0] = read end of output pipe  
// output_pipe[1] = write end of output pipe

printf("Input pipe: read=%d, write=%d\n", input_pipe[0], input_pipe[1]);
printf("Output pipe: read=%d, write=%d\n", output_pipe[0], output_pipe[1]);
```

**How Pipes Work**:
- **One Way**: Data goes in one direction only
- **First In, First Out**: Like a line at the store
- **Small Writes are Safe**: Small writes don't get mixed up
- **Can Wait**: Reading waits if no data, writing waits if pipe is full
- **Limited Size**: Can only hold so much data (usually 64KB)

**Advanced Usage**:
```cpp
// Check pipe buffer size
long pipe_size = fpathconf(input_pipe[0], _PC_PIPE_BUF);
printf("Pipe buffer size: %ld bytes\n", pipe_size);

// Set pipe to non-blocking
int flags = fcntl(input_pipe[0], F_GETFL);
fcntl(input_pipe[0], F_SETFL, flags | O_NONBLOCK);
```

**Error Handling**:
- `EFAULT`: Invalid pipefd pointer
- `EMFILE`: Too many file descriptors in use by process
- `ENFILE`: System file table is full

#### `fork()` - Create New Process
```cpp
#include <unistd.h>
pid_t fork(void);
```

**Function Signature Deep Dive**:
- **Return Type**: `pid_t` - Process ID of child (in parent), 0 (in child), -1 on error
- **Parameters**: None

**What it does**: Makes a copy of your program
**Simple explanation**: Like using a copy machine - you get two programs running the same code

**Detailed Behavior**:
```cpp
printf("Before fork: PID = %d\n", getpid());

pid_t pid = fork();

if (pid == -1) {
    // Fork failed
    perror("fork failed");
    exit(1);
    
} else if (pid == 0) {
    // This is the child process
    printf("Child: PID = %d, Parent PID = %d\n", getpid(), getppid());
    
    // Child-specific code here
    execve("/usr/bin/python3", NULL, environ);  // Replace with Python
    exit(1);  // Only reached if execve fails
    
} else {
    // This is the parent process
    printf("Parent: PID = %d, Child PID = %d\n", getpid(), pid);
    
    // Parent-specific code here
    int status;
    waitpid(pid, &status, 0);  // Wait for child to finish
}
```

**What Gets Copied**:
- **Memory**: All your data and variables
- **Open Files**: All files, sockets, pipes that were open
- **Signal Setup**: How the program handles signals
- **Current Folder**: What folder you're in
- **Settings**: Environment variables

**What's Different**:
- **Process Number**: Child gets a new number
- **Parent Number**: Child knows who made it
- **Waiting Signals**: Child starts with no waiting signals
- **File Locks**: Child doesn't get file locks

**Error Handling**:
- `EAGAIN`: System limit on processes reached
- `ENOMEM`: Insufficient memory for new process
- `ENOSYS`: fork() not supported on this system

#### `dup2()` - Duplicate File Descriptor
```cpp
#include <unistd.h>
int dup2(int oldfd, int newfd);
```

**Function Signature Deep Dive**:
- **Return Type**: `int` - New file descriptor on success, -1 on error
- **Parameter 1**: `int oldfd` - Source file descriptor to duplicate
- **Parameter 2**: `int newfd` - Target file descriptor number

**What it does**: Makes one file number point to the same place as another file number
**Simple explanation**: Like changing pipes - make water flow to a different place

**Detailed Behavior**:
```cpp
// Redirect stdout to a file
int file_fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (file_fd == -1) {
    perror("open failed");
    exit(1);
}

// Save original stdout
int saved_stdout = dup(STDOUT_FILENO);

// Redirect stdout to file
if (dup2(file_fd, STDOUT_FILENO) == -1) {
    perror("dup2 failed");
    exit(1);
}

printf("This goes to the file\n");  // Written to output.txt

// Restore original stdout
dup2(saved_stdout, STDOUT_FILENO);
printf("This goes to terminal\n");  // Written to terminal

close(file_fd);
close(saved_stdout);
```

**CGI Redirection Example**:
```cpp
// In child process - redirect stdin/stdout to pipes
dup2(input_pipe[0], STDIN_FILENO);    // CGI reads from our pipe
dup2(output_pipe[1], STDOUT_FILENO);  // CGI writes to our pipe
dup2(output_pipe[1], STDERR_FILENO);  // CGI errors to our pipe

// Close original pipe ends (child doesn't need them)
close(input_pipe[0]);
close(input_pipe[1]);
close(output_pipe[0]);
close(output_pipe[1]);
```

**Error Handling**:
- `EBADF`: Inva

- `EMFILE`: Too many file descriptors open

#### `execve()` - Execute Program
``cpp
#include <unistd.h>
;
```

**F
or
- **Parameter 1**: `const char *pathname` - Path to executable
- **Parameter 2**: `char *const argv[]` - Command line arguments (NULL-terminated)
- **Parameter 3**: `char *const envp[]` - Environment variables (NULLd)

**Whatam
**Simple explanation**: Like switching the brain of the process - same body, different prg

**Detailed Behavior**:
```cpp
// Build argument array
char *args[] = {
    "

    NULL               ator
};

ent array
char *env[] = {
    "REQUEST_METHOD=GET",
    "QUERY_STRING=name=value",
",
    NULL                    // te
};

preter
execve("/usr/bin/python3", args, env)

// This line is never reached if eeds
;
exit(1);
```

**What Happens During execve()**:
1. **Process Image Replaced**: All code, data, heap, stack replaced
2. **File)
3. **Process ID**: Stays the same
fault
5. **Memory Mappings**: Unmapibraries)

**CGI Environment Building**:
```cpp
std::vector<std::strings;
env_vars.push_back("REQUEST_METHOD=" + reques
ing);
env_vars.push_back("CONTENT_LENGTH=" + content_length);

ve
std::vector<char*> env_ptrs;
for (size_t i = 0; i < env_vars.size(); i++) {
    env_ptrs.push_back(const_cast<char*>(env_vars[i].c_str()));
}
env_ptrs.push_back(NULL);  // NULL terminator

execve(cgi_path.c_str(), NULL, &env_ptrs[0]);
```

**Error Handling**:
- `EACCES`: Permission denied o
- `ENOENT`: File does not exist
- `ENOEXEC`: Invalid executable format
- `ENOMEM`: Insufficient memory
- `E2BIG`: Argument list too long

#### `write()` - Write Data to File Descript
```cpp
#include <unistd.h>

```

:
- **Return Type**: `ssize_t` n error
-o
- *
write


**Simple explanation**: Like putting a message in a pneumatigh

or**:
```cpp
const char* data = "Hello, CGI script!";
size_t data_len = strlen(data);

len);

if (bytes_written == -1) {
    perror("write failed");
} else if (bytes_written < data_len) {
    printf("Partial write: %zd/%zu bytes\n", by
 write...
} else {
    printf("Successfully wrote %zdten);
}
```

**Pipvior**:
```cpp
PIPE
signal(SIGPIPE, SIG_IGN);  // Ignore SIl

ssize_t result = write(pipe_fd, data, len);
if (result == -1 && errno == EPIPE) {
    printf("Broken pipe - no readers\n");
}
```

**Error Handling**:
- `EAGAIN`/`EWOULDBLOCK`: Would block (non-blocking I/O)
- `EBADF`ptor
- `EFointer
y signal
- `EIO`: I/O error
- `ENOSPC`: No space left on device
- `EPIPE`: Broken pipe (no readers)pted bInterru- `EINTR`: buffer palid AULT`: Inv descrialid file: InvignaE sGPIPs causes SIGreaderipe with no ting to pWri// ehaSpecific Be-es_writ\n", byt bytesndle partial    // Ha;len)en, data_tes_writtata, data_], dut_pipe[1te(inpten = wriytes_writsize_t bsavitailed BehDe**ve-throuri bank dt ac tube aet, etc.)e, sockr (file, pipptoscrie deto a fildata *: Writes s*at it doe**Wh to er of bytesumb- N` ize_t countr 3**: `sete*Param *-writea to ning datontai cuf` - Buffervoid *b: `const rameter 2***Pato write tcriptor ile desd` - F1**: `int f*Parameter  * or -1 on,ttes wriof byte- Number **p Divee Deegnaturunction Si**Ft count);*buf, size_oid t v fd, cons(intwritee_t ssizroletable not execu firecay for exo char* arrert tnv// Cory_strNG=" + queSTRI"QUERY_s.push_back(_varenv());_methodt.get_var> envfor shared lxcept (eped set to ders**: ReHandlenal 4. **Sigexecon-ked close-ess marn open (unlmaiptors**: Re Descried")ailr("execve fperrocve succexe;interhon te Pyt// Execurminatorxt/htmlTYPE=te "CONTENT_   ironmuild env// Binterm -   // argv[2]   runo ] - script targv[1         // ",   "script.py    namegram 0] - pro  // argv[on3",   pythin//usr/brunninogram  a new progrith w imagerent processs the curs**: Replacedoeit  -terminaten erruccess, -1 o s onreturns` - Never `intpe**: n Ty**Retur- **: Deep Diveature Signunctionenvp[])har *const  argv[], csthar *conpathname, cchar *t execve(consint `y signal buptederrEINTR`: Int- `criptordese lid fil
**Parameters**:

- `input_pipe[1]`: Which pipe to write to (the write end)
- `request.get_body().c_str()`: The data to send
- `request.get_body().size()`: How much data to send

#### `close()` - Close File Descriptor

```cpp
close(input_pipe[1]);
```

**What it does**: Closes a file descriptor, freeing up the resource
**Simple explanation**: Like hanging up the phone or closing a door - signals "no more data coming"
**Why important**: Tells the CGI script that we're done sending input

```cpp
int CgiRunner::start_cgi_process(const Request& request, const LocationContext& location,
                                int client_fd, const std::string& script_path) {
    // Create pipes for communication
    int input_pipe[2], output_pipe[2];
    if (pipe(input_pipe) == -1 || pipe(output_pipe) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process - execute CGI script

        // Set up pipes
        dup2(input_pipe[0], STDIN_FILENO);   // CGI reads from input_pipe
        dup2(output_pipe[1], STDOUT_FILENO); // CGI writes to output_pipe

        // Close unused pipe ends
        close(input_pipe[1]);
        close(output_pipe[0]);

        // Change to script directory
        std::string script_dir = script_path.substr(0, script_path.find_last_of('/'));
        chdir(script_dir.c_str());

        // Build environment variables
        std::vector<std::string> env_vars = build_cgi_environment(request, location);
        std::vector<char*> env_ptrs;
        for (size_t i = 0; i < env_vars.size(); i++) {
            env_ptrs.push_back(const_cast<char*>(env_vars[i].c_str()));
        }
        env_ptrs.push_back(NULL);

        // Execute CGI script
        execve(location.cgiPath.c_str(), NULL, &env_ptrs[0]);
        exit(1); // If execve fails
    }
    else if (pid > 0) {
        // Parent process - set up monitoring

        close(input_pipe[0]);  // Close read end of input pipe
        close(output_pipe[1]); // Close write end of output pipe

        // Write request body to CGI stdin if POST request
        if (request.get_method() == "POST" && !request.get_body().empty()) {
            write(input_pipe[1], request.get_body().c_str(), request.get_body().size());
        }
        close(input_pipe[1]); // Close write end after writing

        // Set output pipe to non-blocking
        fcntl(output_pipe[0], F_SETFL, O_NONBLOCK);

        // Store process information
        CgiProcess cgi_process;
        cgi_process.pid = pid;
        cgi_process.output_fd = output_pipe[0];
        cgi_process.client_fd = client_fd;
        cgi_process.script_path = script_path;
        cgi_process.finished = false;
        cgi_process.start_time = time(NULL);

        active_cgi_processes[output_pipe[0]] = cgi_process;

        return output_pipe[0]; // Return fd for epoll monitoring
    }

    return -1; // Fork failed
}
```

### 6A.3 CGI Environment Building

**File**: [cgi/cgi_runner.cpp](cgi/cgi_runner.cpp#L200-L250)
**Function**: `build_cgi_environment()`

```cpp
std::vector<std::string> CgiRunner::build_cgi_environment(const Request& request,
                                                         const LocationContext& location) {
    std::vector<std::string> env;

    // Standard CGI environment variables
    env.push_back("REQUEST_METHOD=" + request.get_method());
    env.push_back("REQUEST_URI=" + request.get_requested_path());
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");

    // Query string
    std::string query_string = "";
    size_t query_pos = request.get_requested_path().find('?');
    if (query_pos != std::string::npos) {
        query_string = request.get_requested_path().substr(query_pos + 1);
    }
    env.push_back("QUERY_STRING=" + query_string);

    // Content length for POST requests
    if (request.get_method() == "POST") {
        std::string content_length = request.get_header("content-length");
        if (!content_length.empty()) {
            env.push_back("CONTENT_LENGTH=" + content_length);
        }

        std::string content_type = request.get_header("content-type");
        if (!content_type.empty()) {
            env.push_back("CONTENT_TYPE=" + content_type);
        }
    }

    // HTTP headers as environment variables
    std::map<std::string, std::string> headers = request.get_all_headers();
    for (std::map<std::string, std::string>::iterator it = headers.begin();
         it != headers.end(); ++it) {
        std::string env_name = "HTTP_" + it->first;
        // Convert to uppercase and replace - with _
        for (size_t i = 0; i < env_name.length(); i++) {
            if (env_name[i] == '-') env_name[i] = '_';
            env_name[i] = std::toupper(env_name[i]);
        }
        env.push_back(env_name + "=" + it->second);
    }

    return env;
}
```

### 6A.4 CGI Output Monitoring

**File**: [Server_setup/server.cpp](Server_setup/server.cpp#L200-L270)
**Function**: CGI event handling in main loop

```cpp
else if (is_cgi_socket(fd)) {
    // Handle CGI process I/O
    std::cout << "⚙️ Handling CGI process I/O on fd " << fd << std::endl;

    if (events[i].events & EPOLLIN) {
        std::string response_data;
        if (cgi_runner.handle_cgi_output(fd, response_data)) {
            // CGI process finished, remove from epoll immediately
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);

            // Get the associated client and send response
            int client_fd = cgi_runner.get_client_fd(fd);
            if (client_fd >= 0 && !response_data.empty()) {
                std::map<int, Client>::iterator client_it = active_clients.find(client_fd);
                if (client_it != active_clients.end()) {
                    // Send CGI response to client
                    ssize_t sent = send(client_fd, response_data.c_str(),
                                      response_data.size(), 0);
                    if (sent > 0) {
                        std::cout << "✅ Sent " << sent << " bytes CGI response to client "
                                 << client_fd << std::endl;
                    }

                    // Close client connection
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    active_clients.erase(client_it);
                }
            }

            // Clean up CGI process
            cgi_runner.cleanup_cgi_process(fd);
        }
    }
    else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
        // CGI process closed or error occurred
        std::string response_data;
        if (cgi_runner.handle_cgi_output(fd, response_data)) {
            // Send any remaining data
            int client_fd = cgi_runner.get_client_fd(fd);
            if (client_fd >= 0 && !response_data.empty()) {
                send(client_fd, response_data.c_str(), response_data.size(), 0);
            }
        }

        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        cgi_runner.cleanup_cgi_process(fd);
    }
}
```

### 6A.5 CGI Output Processing

**File**: [cgi/cgi_runner.cpp](cgi/cgi_runner.cpp#L300-L400)
**Function**: `handle_cgi_output()`

````cpp
bool CgiRunner::handle_cgi_output(int fd, std::string& response_data) {
    std::map<int, CgiProcess>::iterator it = active_cgi_processes.find(fd);
    if (it == active_cgi_processes.end()) {
        return false;
    }

    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        it->second.output_buffer += buffer;
        return false; // More data might be coming
    }
    else if (bytes_read == 0) {
        // CGI process finished
        it->second.finished = true;

        // Wait for process to complete
        int status;
        waitpid(it->second.pid, &status, 0);

        // Format CGI output as HTTP response
        response_data = format_cgi_response(it->second.output_buffer);
        return true; // Process finished
    }
    else {
        // Error or would block
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error occurred
            it->second.finished = true;
            response_data = "HTTP/1.1 500 Internal Server Error\r\n\r\nCGI Error";
            return true;
        }
        return false; // Would block, try again later
    }
}

### 🔧 Key Functions Explained:

#### `read()` - Read Data from File Descriptor
```cpp
ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
````

**What it does**: Reads data from a file descriptor (pipe, socket, file, etc.)
**Simple explanation**: Like drinking from a straw - you suck up whatever liquid is available
**Parameters**:

- `fd`: Which file descriptor to read from (like which straw)
- `buffer`: Where to put the data (like your mouth)
- `sizeof(buffer) - 1`: Maximum amount to read (how big a sip)

**Return values**:

- `> 0`: Number of bytes successfully read
- `0`: End of file/stream (no more data will come)
- `-1`: Error or would block (check `errno`)

**Common errno values**:

- `EAGAIN`/`EWOULDBLOCK`: No data available right now, try again later
- `EBADF`: Bad file descriptor
- `EINTR`: Interrupted by signal

#### `waitpid()` - Wait for Child Process

```cpp
int status;
waitpid(it->second.pid, &status, 0);
```

**What it does**: Waits for a child process to finish and cleans up its resources
**Simple explanation**: Like waiting for your kid to finish their homework before checking their grade
**Parameters**:

- `it->second.pid`: Which child process to wait for
- `&status`: Where to store the exit status
- `0`: Options (0 means wait until process finishes)

**Why important**: Prevents "zombie processes" - dead processes that haven't been cleaned up

````

### 6A.6 CGI Response Formatting
**File**: [cgi/cgi_runner.cpp](cgi/cgi_runner.cpp#L450-L500)
**Function**: `format_cgi_response()`

```cpp
std::string CgiRunner::format_cgi_response(const std::string& cgi_output) {
    if (cgi_output.empty()) {
        return "HTTP/1.1 500 Internal Server Error\r\n\r\nEmpty CGI Response";
    }

    // Split CGI output into headers and body
    size_t header_end = cgi_output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = cgi_output.find("\n\n");
        if (header_end == std::string::npos) {
            // No headers, treat entire output as body
            return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + cgi_output;
        }
    }

    std::string cgi_headers = cgi_output.substr(0, header_end);
    std::string cgi_body = cgi_output.substr(header_end + 4);

    // Build HTTP response
    std::string http_response = "HTTP/1.1 200 OK\r\n";

    // Add CGI headers to HTTP response
    std::istringstream header_stream(cgi_headers);
    std::string line;
    while (std::getline(header_stream, line)) {
        if (!line.empty() && line[line.length()-1] == '\r') {
            line = line.substr(0, line.length()-1);
        }
        if (!line.empty()) {
            http_response += line + "\r\n";
        }
    }

    // Add content length if not present
    if (cgi_headers.find("Content-Length:") == std::string::npos) {
        std::ostringstream content_length;
        content_length << cgi_body.length();
        http_response += "Content-Length: " + content_length.str() + "\r\n";
    }

    http_response += "\r\n" + cgi_body;
    return http_response;
}
````

---

## Step 6B: Regular File Request Processing Path

### 6B.1 Method-Based Handler Selection

**File**: [client/client.cpp](client/client.cpp#L250-L300)
**Function**: Regular request processing

```cpp
// Not a CGI request, handle normally
if (current_request.get_method() == "GET") {
    GetHandler get_handler(current_request, *current_request.get_location());
    ResponseData response = get_handler.handle();
    send_response(response);
}
else if (current_request.get_method() == "POST") {
    PostHandler post_handler(current_request, *current_request.get_location());
    ResponseData response = post_handler.handle();
    send_response(response);
}
else if (current_request.get_method() == "DELETE") {
    DeleteHandler delete_handler(current_request, *current_request.get_location());
    ResponseData response = delete_handler.handle();
    send_response(response);
}
else {
    // Method not allowed
    send_error_response(405);
}
```

### 6B.2 GET Request Handling

**File**: [request/get_handler.cpp](request/get_handler.cpp#L1-L100)
**Function**: `handle()`

```cpp
ResponseData GetHandler::handle() {
    ResponseData response;

    // Build file path
    std::string file_path = location.root + request.get_requested_path();

    // Check if path is a directory
    struct stat path_stat;
    if (stat(file_path.c_str(), &path_stat) == 0) {
        if (S_ISDIR(path_stat.st_mode)) {
            // Directory request
            if (location.autoindex) {
                return generate_directory_listing(file_path);
            } else if (!location.index.empty()) {
                file_path += "/" + location.index;
            } else {
                response.status_code = 403;
                response.body = "Directory listing disabled";
                return response;
            }
        }
    }

    // Try to open file
    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        response.status_code = 404;
        response.body = "File not found";
        return response;
    }

    // Read file content
    std::ostringstream content_stream;
    content_stream << file.rdbuf();
    response.body = content_stream.str();

    // Set content type based on file extension
    response.content_type = get_mime_type(file_path);
    response.status_code = 200;

    return response;
}
```

### 6B.3 POST Request Handling

**File**: [request/post_handler.cpp](request/post_handler.cpp#L1-L150)
**Function**: `handle()`

```cpp
ResponseData PostHandler::handle() {
    ResponseData response;

    // Check if upload is allowed
    if (!location.upload_enable) {
        response.status_code = 403;
        response.body = "Upload not allowed";
        return response;
    }

    // Get content type
    std::string content_type = request.get_header("content-type");

    if (content_type.find("multipart/form-data") != std::string::npos) {
        // Handle file upload
        return handle_file_upload();
    }
    else if (content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
        // Handle form data
        return handle_form_data();
    }
    else {
        // Handle raw data
        std::string upload_path = location.upload_path + "/uploaded_file";
        std::ofstream output_file(upload_path.c_str(), std::ios::binary);

        if (output_file.is_open()) {
            output_file << request.get_body();
            output_file.close();

            response.status_code = 201;
            response.body = "File uploaded successfully";
        } else {
            response.status_code = 500;
            response.body = "Failed to save file";
        }
    }

    return response;
}
```

### 6B.4 DELETE Request Handling

**File**: [request/delete_handler.cpp](request/delete_handler.cpp#L1-L50)
**Function**: `handle()`

```cpp
ResponseData DeleteHandler::handle() {
    ResponseData response;

    // Build file path
    std::string file_path = location.root + request.get_requested_path();

    // Check if file exists
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) != 0) {
        response.status_code = 404;
        response.body = "File not found";
        return response;
    }

    // Try to delete file
    if (unlink(file_path.c_str()) == 0) {
        response.status_code = 200;
        response.body = "File deleted successfully";
    } else {
        response.status_code = 500;
        response.body = "Failed to delete file";
    }

    return response;
}
```

---

## Step 7: Response Generation and Sending

### 7.1 Response Formatting

**File**: [response/response.cpp](response/response.cpp#L1-L100)
**Function**: `format_response()`

```cpp
std::string Response::format_response(const ResponseData& response_data) {
    std::ostringstream response_stream;

    // Status line
    response_stream << "HTTP/1.1 " << response_data.status_code << " "
                   << get_status_message(response_data.status_code) << "\r\n";

    // Headers
    response_stream << "Content-Type: " << response_data.content_type << "\r\n";
    response_stream << "Content-Length: " << response_data.body.length() << "\r\n";
    response_stream << "Connection: close\r\n";

    // Additional headers
    for (std::map<std::string, std::string>::const_iterator it = response_data.headers.begin();
         it != response_data.headers.end(); ++it) {
        response_stream << it->first << ": " << it->second << "\r\n";
    }

    response_stream << "\r\n"; // End of headers
    response_stream << response_data.body;

    return response_stream.str();
}
```

### 7.2 Response Transmission

**File**: [client/client.cpp](client/client.cpp#L400-L450)
**Function**: `send_response()`

````cpp
void Client::send_response(const ResponseData& response_data) {
    std::string http_response = Response::format_response(response_data);

    ssize_t total_sent = 0;
    ssize_t response_size = http_response.length();

    while (total_sent < response_size) {
        ssize_t sent = send(client_fd, http_response.c_str() + total_sent,
                           response_size - total_sent, 0);

        if (sent > 0) {
            total_sent += sent;
        } else if (sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block, try again later
                continue;
            } else {
                // Real error
                break;
            }
        }
    }

    std::cout << "✅ Sent " << total_sent << " bytes response to client " << client_fd << std::endl;
}

### 🔧 Key Functions Explained:

#### `send()` - Send Data to Client
```cpp
#include <sys/socket.h>
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

**Function Signature Deep Dive**:
- **Return Type**: `ssize_t` - Number of bytes sent, or -1 on error
- **Parameter 1**: `int sockfd` - Socket file descriptor to send to
- **Parameter 2**: `const void *buf` - Pointer to data to send
- **Parameter 3**: `size_t len` - Number of bytes to send
- **Parameter 4**: `int flags` - Flags to modify behavior

**What it does**: Sends data to someone through the network
**Simple explanation**: Like putting a letter in the mailbox - you give it to the mail system

**Detailed Behavior**:
```cpp
const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
size_t response_len = strlen(response);
ssize_t total_sent = 0;

// Send all data (handling partial sends)
while (total_sent < response_len) {
    ssize_t sent = send(client_fd, response + total_sent, 
                       response_len - total_sent, 0);
    
    if (sent > 0) {
        total_sent += sent;
        printf("Sent %zd bytes, total: %zd/%zu\n", sent, total_sent, response_len);
        
    } else if (sent == 0) {
        // This shouldn't happen with send(), but handle it
        printf("send() returned 0 - connection closed?\n");
        break;
        
    } else {  // sent == -1
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Send buffer is full, wait and try again
            printf("Send buffer full, retrying...\n");
            usleep(1000);  // Wait 1ms
            continue;
        } else if (errno == EPIPE || errno == ECONNRESET) {
            // Client disconnected
            printf("Client disconnected during send\n");
            break;
        } else {
            // Other error
            perror("send failed");
            break;
        }
    }
}
```

**Why You Can't Send Everything at Once**:
1. **Send Buffer Full**: The system's send space is full
2. **Network Slow**: The network is busy or slow
3. **Big Data**: The system breaks big data into small pieces
4. **Got Interrupted**: Something stopped the send

**Error Handling**:
- `EAGAIN`/`EWOULDBLOCK`: Send buffer full (non-blocking socket)
- `EBADF`: Invalid socket descriptor
- `ECONNRESET`: Connection reset by peer
- `EPIPE`: Broken pipe (peer closed connection)
- `EINTR`: Interrupted by signal
- `EINVAL`: Invalid arguments
- `ENOTCONN`: Socket not connected

---

## Step 8: Connection Cleanup

### 8.1 Client Disconnection
**File**: [Server_setup/server.cpp](Server_setup/server.cpp#L350-L400)
**Function**: Connection cleanup in main loop

```cpp
// Handle client disconnection or error
if (events[i].events & (EPOLLHUP | EPOLLERR)) {
    std::cout << "Client " << fd << " disconnected" << std::endl;

    // Remove from epoll
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);

    // Close socket
    close(fd);

    // Remove from active clients
    std::map<int, Client>::iterator client_it = active_clients.find(fd);
    if (client_it != active_clients.end()) {
        active_clients.erase(client_it);
    }
}
````

---

## Error Handling Cases

### Case 1: Malformed Request

**File**: [request/request.cpp](request/request.cpp)

```cpp
// In request/request.cpp
if (method.empty() || requested_path.empty() || http_version.empty()) {
    return BAD_REQUEST;
}
```

### Case 2: File Not Found

**File**: [request/get_handler.cpp](request/get_handler.cpp)

```cpp
// In request/get_handler.cpp
std::ifstream file(file_path.c_str());
if (!file.is_open()) {
    response.status_code = 404;
    response.body = load_error_page(404);
    return response;
}
```

### Case 3: CGI Timeout

**File**: [cgi/cgi_runner.cpp](cgi/cgi_runner.cpp)

```cpp
// In cgi/cgi_runner.cpp
time_t current_time = time(NULL);
if (current_time - it->second.start_time > 30) {
    kill(it->second.pid, SIGKILL);
    waitpid(it->second.pid, NULL, 0);
    response_data = "HTTP/1.1 504 Gateway Timeout\r\n\r\nCGI Script Timeout";
    return true;
}
```

### Case 4: Method Not Allowed

**File**: [client/client.cpp](client/client.cpp)

```cpp
// In client/client.cpp
else {
    ResponseData error_response;
    error_response.status_code = 405;
    error_response.body = "Method Not Allowed";
    send_response(error_response);
}
```

### 🔧 Additional System Functions Used:

#### `kill()` - Send Signal to Process

```cpp
kill(it->second.pid, SIGKILL);
```

**What it does**: Sends a signal to a process (like SIGKILL to force termination)
**Simple explanation**: Like pressing the power button on a computer - forces it to stop
**Parameters**:

- `it->second.pid`: Which process to signal
- `SIGKILL`: What signal to send (SIGKILL = force quit, can't be ignored)

#### `time()` - Get Current Time

```cpp
time_t current_time = time(NULL);
```

**What it does**: Gets the current time in seconds since January 1, 1970
**Simple explanation**: Like looking at a clock to see what time it is
**Usage**: Used for calculating timeouts (current_time - start_time > 30 seconds)

#### `chdir()` - Change Directory

```cpp
chdir(script_dir.c_str());
```

**What it does**: Changes the current working directory of the process
**Simple explanation**: Like using "cd" command in terminal - moves to a different folder
**Why needed**: CGI scripts often expect to run from their own directory to find relative files

#### `stat()` - Get File Information

```cpp
struct stat path_stat;
if (stat(file_path.c_str(), &path_stat) == 0) {
    if (S_ISDIR(path_stat.st_mode)) {
        // It's a directory
    }
}
```

**What it does**: Gets information about a file or directory (size, type, permissions, etc.)
**Simple explanation**: Like right-clicking on a file and selecting "Properties"
**Usage**: Check if path is a file or directory, if it exists, file size, etc.

---

## Summary of All Possible Request Paths

1. **Regular GET Request**: Connection → Parsing → Location Match → GET Handler → File Read → Response
2. **Regular POST Request**: Connection → Parsing → Location Match → POST Handler → File Upload → Response
3. **Regular DELETE Request**: Connection → Parsing → Location Match → DELETE Handler → File Delete → Response
4. **CGI GET Request**: Connection → Parsing → CGI Detection → CGI Process → CGI Output → Response
5. **CGI POST Request**: Connection → Parsing → CGI Detection → CGI Process (with body) → CGI Output → Response
6. **Error Cases**: Connection → Parsing → Error Detection → Error Response
7. **Directory Listing**: Connection → Parsing → Directory Detection → Autoindex → Response
8. **File Not Found**: Connection → Parsing → File Access → 404 Response

## Each path maintains the non-blocking, event-driven architecture using epoll for efficient I/O multiplexing.

## 🔧 Complete System Functions Reference

### Network Functions

| Function   | Purpose                    | Simple Explanation                              |
| ---------- | -------------------------- | ----------------------------------------------- |
| `socket()` | Create network endpoint    | Like getting a phone number                     |
| `bind()`   | Assign address to socket   | Like connecting your phone to a specific number |
| `listen()` | Wait for connections       | Like turning on your phone to receive calls     |
| `accept()` | Accept incoming connection | Like answering the phone when it rings          |
| `recv()`   | Receive data from network  | Like listening to what the caller says          |
| `send()`   | Send data over network     | Like talking back to the caller                 |

### Process Management Functions

| Function    | Purpose                          | Simple Explanation                                     |
| ----------- | -------------------------------- | ------------------------------------------------------ |
| `fork()`    | Create new process               | Like making a copy of yourself                         |
| `execve()`  | Replace process with new program | Like switching brains with someone else                |
| `waitpid()` | Wait for child process to finish | Like waiting for your kid to come home                 |
| `kill()`    | Send signal to process           | Like tapping someone on the shoulder (or hitting them) |
| `pipe()`    | Create communication channel     | Like building a tube between two rooms                 |

### File/IO Functions

| Function  | Purpose                            | Simple Explanation                     |
| --------- | ---------------------------------- | -------------------------------------- |
| `read()`  | Read data from file descriptor     | Like drinking from a straw             |
| `write()` | Write data to file descriptor      | Like blowing bubbles through a straw   |
| `close()` | Close file descriptor              | Like hanging up the phone              |
| `dup2()`  | Redirect input/output              | Like changing the plumbing connections |
| `fcntl()` | Control file descriptor properties | Like adjusting settings on your phone  |

### Event Monitoring Functions (epoll)

| Function          | Purpose                      | Simple Explanation                           |
| ----------------- | ---------------------------- | -------------------------------------------- |
| `epoll_create1()` | Create event monitor         | Like hiring a security guard                 |
| `epoll_ctl()`     | Add/remove/modify monitoring | Like telling the guard which doors to watch  |
| `epoll_wait()`    | Wait for events              | Like asking the guard "did anything happen?" |

### File System Functions

| Function   | Purpose                  | Simple Explanation                |
| ---------- | ------------------------ | --------------------------------- |
| `stat()`   | Get file information     | Like checking file properties     |
| `chdir()`  | Change current directory | Like using "cd" command           |
| `unlink()` | Delete file              | Like throwing a file in the trash |

### Error Handling

Most functions return `-1` on error and set `errno` to indicate what went wrong:

- `EAGAIN`/`EWOULDBLOCK`: Try again later (non-blocking operation would block)
- `EINTR`: Interrupted by signal
- `EBADF`: Bad file descriptor
- `EPIPE`: Broken pipe (other end closed)
- `ECONNRESET`: Connection reset by peer

### Why Non-Blocking is Important

```cpp
// Blocking (bad for servers):
recv(fd, buffer, size, 0);  // Waits forever if no data

// Non-blocking (good for servers):
fcntl(fd, F_SETFL, O_NONBLOCK);
int result = recv(fd, buffer, size, 0);
if (result == -1 && errno == EAGAIN) {
    // No data available, check other clients
}
```

**Simple explanation**: Blocking is like waiting in line at one checkout counter even if it's slow. Non-blocking is like checking multiple counters and serving whoever is ready first.

This architecture allows the server to handle thousands of clients efficiently by never waiting for slow operations.

---

## 📚 Simple Summary

### What Happens When Someone Visits Your Website:

1. **Someone Connects**: A person types your website in their browser
2. **Server Accepts**: Your server says "OK, I'll talk to you"
3. **They Send Request**: Browser sends "GET /page.html HTTP/1.1"
4. **Server Reads**: Your server reads what they want
5. **Server Decides**: Is this a normal file or a script?
6. **Server Responds**: 
   - **Normal file**: Read file, send it back
   - **Script (CGI)**: Run the script, send what it makes
7. **Connection Closes**: Done talking, hang up

### Key Functions in Simple Words:

| Function | What It Does | Like... |
|----------|--------------|---------|
| `epoll_wait()` | Wait for something to happen | Guard watching many doors |
| `accept()` | Answer when someone calls | Picking up the phone |
| `recv()` | Get data from someone | Reading a letter |
| `send()` | Send data to someone | Mailing a letter |
| `fork()` | Make a copy of your program | Using a copy machine |
| `pipe()` | Make a tube for data | Building a tunnel |
| `execve()` | Change to a different program | Switching brains |

### Why This is Fast:

- **No Waiting**: Server doesn't wait for slow clients
- **Many at Once**: Can talk to thousands of people at the same time
- **Smart Watching**: Only checks things when they're ready
- **Quick Decisions**: Knows right away if it's a file or script

### Common Problems and Fixes:

- **Client Disconnects**: Check if `recv()` returns 0
- **Send Buffer Full**: Check for `EAGAIN` and try again later
- **Script Takes Too Long**: Kill it after 30 seconds
- **File Not Found**: Send 404 error page
- **Bad Request**: Send 400 error page

This server can handle many people at once because it never waits around doing nothing!
---

## 📘 Appendix: System Call & Concept Reference (Deep Dive)

Central reference for all primitives used; format: What | How | Why | Pitfalls | Evaluation highlight | Analogy.

### socket()
Creates a network endpoint (FD). Kernel allocates protocol control block (TCP state machine initially CLOSED). Needed to begin any network I/O. Pitfalls: ignoring errors (EMFILE/ENFILE). Evaluation: differentiate AF_INET vs AF_INET6, SOCK_STREAM semantics. Analogy: getting a new phone.

### bind()
Associates local IP:port with socket; inserts into kernel lookup tables so SYN packets map to this listener. Pitfall: forgetting SO_REUSEADDR causes restart delays. Analogy: registering your number.

### listen()
Marks socket passive; establishes SYN backlog + accept queue. Pitfall: too small backlog under burst load. Evaluation: mention two queues (SYN, accept). Analogy: enabling reception desk with chairs.

### accept()
Returns new FD per client; must loop until EAGAIN under EPOLLET. Pitfall: only accepting once; leads to remaining pending connections never processed. Analogy: answering each ringing line.

### fcntl(O_NONBLOCK)
Switches descriptor to non-blocking, enabling event-driven design. Pitfall: overwriting flags instead of OR-ing. Analogy: telling a clerk to report status immediately instead of waiting.

### epoll (create1/ctl/wait)
Kernel maintains ready list; epoll_wait returns only when transitions occur (ET) or condition stands (LT). Pitfall: not draining reads/writes. Evaluation: O(1) scaling statement.

### pipe()
Unidirectional in-kernel circular buffer used for CGI stdin/stdout bridging. Pitfall: not closing unused ends -> child waits forever for EOF.

### fork()
Copy-on-write clone; child inherits FDs/environment. Pitfall: forgetting _exit() on failure after failed exec path.

### dup2()
Atomically reassigns a descriptor number (e.g., STDIN_FILENO). Pitfall: leaving original pipe descriptors open causing unexpected blocking.

### execve()
Replaces process image; see cleaned section above. Emphasize environment sanitation + FD leakage prevention (FD_CLOEXEC).

### waitpid()
Reaps child; prevents zombie accumulation. Pitfall: ignoring non-blocking variant for periodic cleanup.

### recv()/send()
Operate on TCP stream (no message boundaries). Partial operations normal. Must loop + handle EAGAIN. Evaluation: correct treatment of 0 from recv (peer closed gracefully).

### read()/write()
Generic I/O; used for pipes. Partial writes require loops for large buffers.

### stat()/unlink()
File metadata & deletion. Security: check path traversal earlier to avoid exposing filesystem.

### chdir()
Used only in CGI child so server cwd remains stable.

### MIME Mapping
Extension -> Content-Type; fallback application/octet-stream.

### Limits (MAX_REQUEST_SIZE etc.)
Protect against memory exhaustion & header bombing; map to 413/431 responses.

### Timeout Handling
Track CGI start_time; kill overrun -> 504. Evaluation: resource exhaustion mitigation.

Use these concise points during oral defense for fast recall.



## 🎯 **COMPREHENSIVE EVALUATION CHECKLIST**

### **1. Non-blocking I/O Implementation**
- ✅ **epoll() usage**: O(1) event notification system
- ✅ **Edge-triggered mode**: EPOLLET for maximum efficiency
- ✅ **Non-blocking sockets**: fcntl(fd, F_SETFL, O_NONBLOCK)
- ✅ **EAGAIN handling**: Proper handling of would-block conditions
- ✅ **Multiple client support**: Concurrent connection handling

### **2. HTTP Protocol Compliance**
- ✅ **HTTP/1.1 support**: Request/response format compliance
- ✅ **Method support**: GET, POST, DELETE implementation
- ✅ **Status codes**: Comprehensive HTTP status code coverage
- ✅ **Header parsing**: Case-insensitive, RFC-compliant parsing
- ✅ **Content-Length**: Proper body size handling
- ✅ **Host header**: Required for HTTP/1.1 compliance

### **3. CGI Implementation**
- ✅ **Process isolation**: fork() for security
- ✅ **Environment variables**: CGI/1.1 specification compliance
- ✅ **Input/Output redirection**: stdin/stdout pipe handling
- ✅ **Working directory**: chdir() to script directory
- ✅ **Timeout handling**: 30-second limit with SIGKILL
- ✅ **Process cleanup**: waitpid() to prevent zombies

### **4. Error Handling**
- ✅ **System call errors**: errno checking for all system calls
- ✅ **Resource limits**: File descriptor and memory limits
- ✅ **Malformed requests**: Proper HTTP error responses
- ✅ **Connection drops**: Graceful handling of client disconnects
- ✅ **CGI failures**: Proper error responses for script failures

### **5. Resource Management**
- ✅ **File descriptor cleanup**: close() all unused descriptors
- ✅ **Memory management**: No memory leaks in C++98
- ✅ **Process cleanup**: Kill and wait for CGI processes
- ✅ **Connection limits**: Handle EMFILE/ENFILE gracefully
- ✅ **Buffer management**: Prevent buffer overflow attacks

### **6. Security Considerations**
- ✅ **Directory traversal**: Block ../ path attacks
- ✅ **Request size limits**: Prevent DoS attacks
- ✅ **Process isolation**: CGI can't affect server
- ✅ **Input validation**: Sanitize all user input
- ✅ **Resource limits**: Timeout and size restrictions

---

## 🔧 **TECHNICAL IMPLEMENTATION DETAILS**

### **File Descriptor Management:**
```cpp
class FdManager {
private:
    std::set<int> open_fds;
    
public:
    void track_fd(int fd) { open_fds.insert(fd); }
    void untrack_fd(int fd) { open_fds.erase(fd); }
    
    ~FdManager() {
        // Cleanup all tracked file descriptors
        for (std::set<int>::iterator it = open_fds.begin(); 
             it != open_fds.end(); ++it) {
            close(*it);
        }
    }
};
```

### **Memory Management (C++98 Compliant):**
```cpp
// RAII pattern for automatic cleanup
class ClientConnection {
private:
    int client_fd;
    char* buffer;
    
public:
    ClientConnection(int fd) : client_fd(fd), buffer(new char[BUFFER_SIZE]) {}
    
    ~ClientConnection() {
        if (client_fd >= 0) close(client_fd);
        delete[] buffer;
    }
    
    // Prevent copying (C++98 style)
private:
    ClientConnection(const ClientConnection&);
    ClientConnection& operator=(const ClientConnection&);
};
```

### **Signal Handling for CGI:**
```cpp
void setup_signal_handlers() {
    // Ignore SIGPIPE (broken pipe)
    signal(SIGPIPE, SIG_IGN);
    
    // Handle SIGCHLD for zombie prevention
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

void sigchld_handler(int sig) {
    // Reap zombie processes
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Log process termination
        std::cout << "CGI process " << pid << " terminated\n";
    }
}
```

### **Configuration File Parsing:**
```cpp
// NGINX-style configuration support
server {
    listen 8080;
    server_name localhost;
    
    location / {
        root /var/www/html;
        index index.html;
        autoindex on;
    }
    
    location /cgi-bin/ {
        root /var/www;
        cgi_extension .py;
        cgi_path /usr/bin/python3;
    }
}
```

### **Performance Optimizations:**
1. **Connection Pooling**: Reuse client objects
2. **Buffer Reuse**: Avoid frequent allocations
3. **Efficient String Operations**: Minimize string copying
4. **Lazy Evaluation**: Parse only when needed
5. **Cache Headers**: Store parsed headers efficiently

### **Testing Scenarios for Evaluation:**
1. **Concurrent Connections**: Test with multiple simultaneous clients
2. **Large Files**: Upload/download large files
3. **CGI Scripts**: Various Python scripts with different outputs
4. **Error Conditions**: Malformed requests, timeouts, disconnects
5. **Resource Limits**: Test behavior under resource pressure
6. **Security Tests**: Directory traversal, oversized requests

### **Common Evaluation Questions & Answers:**

**Q: Why use epoll instead of select?**
A: epoll is O(1) vs select's O(n), supports unlimited file descriptors, and is more efficient for high-concurrency servers.

**Q: How do you prevent zombie processes?**
A: Use waitpid() to reap child processes and SIGCHLD handler for automatic cleanup.

**Q: How do you handle partial reads/writes?**
A: Check return values and loop until all data is processed, handling EAGAIN for non-blocking I/O.

**Q: What happens if CGI script hangs?**
A: 30-second timeout with SIGKILL, proper cleanup, and HTTP 504 Gateway Timeout response.

**Q: How do you prevent DoS attacks?**
A: Request size limits, connection limits, timeouts, and input validation.

**Q: Why change working directory for CGI?**
A: Allows CGI scripts to access relative files and matches CGI/1.1 specification behavior.

This implementation provides a production-ready, secure, and efficient HTTP server with full CGI support while maintaining C++98 compatibility.