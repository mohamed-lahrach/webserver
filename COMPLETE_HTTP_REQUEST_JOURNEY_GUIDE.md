# Complete HTTP Request Journey Guide: From recv() to Response

## Overview

This guide traces the complete journey of an HTTP request through the webserver, from the moment data is received via `recv()` to the final response being sent back to the client. We'll cover both regular file serving and CGI execution paths, explaining every function, system call, and decision point along the way.

## 🎯 **CRITICAL EVALUATION POINTS**

### **Key Technical Concepts to Understand:**

1. **Non-blocking I/O with epoll**: How the server handles multiple clients without blocking
2. **HTTP Protocol Compliance**: Request parsing, header validation, response formatting
3. **CGI Implementation**: Process management, environment variables, pipe communication
4. **Error Handling**: Graceful handling of all error conditions
5. **Resource Management**: File descriptor cleanup, memory management, process cleanup
6. **Security**: Input validation, timeout handling, directory traversal prevention

---

## 🚀 **STEP 1: Data Reception - The recv() Call**

### Location: [`client/client.cpp:67-72`](client/client.cpp#L67-L72) - `handle_client_data_input()`

```cpp
void Client::handle_client_data_input(int epoll_fd, std::map<int, Client> &active_clients,
                                     ServerContext &server_config, CgiRunner& cgi_runner)
{
    char buffer[7000000] = {0};  // Large buffer for request data
    ssize_t bytes_received;

    bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
```

### 🔧 **Function Deep Dive: recv() - Complete Technical Analysis**

```cpp
#include <sys/socket.h>
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

#### **What recv() Actually Does at the Kernel Level**

**System Call Journey:**
```
User Space Application (your webserver)
        ↓ (system call trap)
Kernel Space (privileged mode)
        ↓
Socket subsystem validation
        ↓
TCP/IP stack processing
        ↓
Network buffer management
        ↓
Data copy to user space
        ↓
Return to application
```

**Internal Kernel Process:**
1. **File Descriptor Validation**: Kernel checks if `sockfd` is valid and points to a socket
2. **Buffer Validation**: Verifies `buf` points to valid, writable user memory
3. **Socket State Check**: Ensures socket is connected and ready for reading
4. **Data Availability**: Checks TCP receive buffer for available data
5. **Data Copy**: Copies data from kernel space to user space buffer
6. **State Update**: Updates socket pointers and TCP window information
7. **Return**: Returns bytes copied or appropriate error code

#### **Parameters Deep Technical Analysis**

**`int sockfd` - Socket File Descriptor:**
- **What it is**: Integer handle representing the socket in the process's file descriptor table
- **Kernel lookup**: `sockfd → fdtable[sockfd] → file* → socket* → sock*`
- **Validation process**: 
  ```cpp
  // Kernel validation steps
  if (sockfd < 0 || sockfd >= current->files->max_fds) return -EBADF;
  if (!current->files->fd[sockfd]) return -EBADF;
  if (!S_ISSOCK(current->files->fd[sockfd]->f_inode->i_mode)) return -ENOTSOCK;
  ```
- **Range**: Typically 0-1023 for standard processes, higher with increased limits

**`void *buf` - Buffer Pointer:**
- **Memory validation**: Kernel uses `access_ok()` to verify address is in valid user space
- **Page fault handling**: If buffer pages are swapped out, kernel handles page faults automatically
- **Alignment**: No special alignment requirements (can be any valid address)
- **Size considerations**: Should be large enough for expected data chunks
- **Your implementation**: 7MB buffer handles very large HTTP requests

**`size_t len` - Buffer Length:**
- **Maximum bytes**: Upper limit of data to receive in this single call
- **Partial reads**: recv() commonly returns less than requested (TCP is stream-based)
- **Zero length**: Valid parameter, returns 0 immediately without blocking
- **Large values**: Limited by available socket buffer data and system limits
- **Performance impact**: Larger buffers reduce system call overhead

**`int flags` - Control Flags:**
- **`0` (Normal operation)**: Most common, used in your webserver
- **`MSG_DONTWAIT`**: Non-blocking operation (overrides socket setting)
- **`MSG_PEEK`**: Look at data without removing it from receive queue
- **`MSG_WAITALL`**: Wait for full request or error (blocking)
- **`MSG_OOB`**: Receive out-of-band (urgent) data
- **`MSG_TRUNC`**: Return real packet length even if truncated

#### **Return Values and Their Deep Meanings**

**Positive Number (> 0) - Successful Read:**
```cpp
ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
if (bytes_received > 0) {
    buffer[bytes_received] = '\0';  // Your code does this correctly
    // Process the received data
    std::cout << "Received " << bytes_received << " bytes" << std::endl;
}
```
- **Meaning**: Actual number of bytes successfully copied to user buffer
- **Important**: May be less than requested due to TCP stream nature
- **TCP behavior**: No message boundaries, data arrives in arbitrary chunks
- **Buffer management**: Always check actual bytes received, not requested

**Zero (0) - Graceful Connection Close:**
```cpp
if (bytes_received == 0) {
    std::cout << "Client closed connection gracefully" << std::endl;
    // TCP FIN received - peer initiated close
    cleanup_connection(epoll_fd, active_clients);
}
```
- **TCP mechanics**: Peer sent FIN (finish) packet
- **Connection state**: Socket now in half-closed state
- **Action required**: Close socket and clean up resources
- **Not an error**: Normal connection termination procedure

**Negative (-1) - Error Conditions:**
```cpp
if (bytes_received == -1) {
    switch (errno) {
        case EAGAIN:
        case EWOULDBLOCK:
            // No data available (non-blocking socket)
            return; // Try again when epoll notifies
        case ECONNRESET:
            // Connection reset by peer (abrupt close)
            std::cout << "Connection reset by peer" << std::endl;
            cleanup_connection(epoll_fd, active_clients);
            break;
        case EINTR:
            // Interrupted by signal, retry
            continue;
        case EBADF:
            // Invalid file descriptor
            std::cout << "Invalid socket descriptor" << std::endl;
            break;
        default:
            perror("recv failed");
            cleanup_connection(epoll_fd, active_clients);
    }
}
```

#### **TCP Receive Buffer Mechanics**

**How TCP Buffering Works:**
```
Network Packets → TCP Receive Buffer (Kernel) → recv() → Application Buffer (User)
      ↓                    ↓                      ↓              ↓
   Reassembly         Kernel Space          System Call     User Space
```

**Buffer States and Behavior:**
- **Empty Buffer**: recv() blocks (blocking mode) or returns EAGAIN (non-blocking)
- **Partial Data**: recv() returns available data (may be less than requested)
- **Full Buffer**: recv() returns requested amount or buffer size, whichever is smaller
- **Flow Control**: TCP advertises window size based on available buffer space

**TCP Window Management:**
```cpp
// Simplified TCP window calculation
available_space = socket_buffer_size - buffered_data;
advertised_window = min(available_space, max_window_size);
// Sender can only send up to advertised_window bytes
```

#### **Advanced recv() Concepts**

**Non-blocking I/O with EAGAIN:**
```cpp
// Your server uses non-blocking sockets
fcntl(client_fd, F_SETFL, O_NONBLOCK);

// recv() behavior changes
ssize_t result = recv(client_fd, buffer, size, 0);
if (result == -1 && errno == EAGAIN) {
    // No data available right now
    // epoll will notify when data arrives
    return;
}
```

**Partial Reads and Accumulation:**
```cpp
// Your implementation correctly handles partial reads
incoming_data.append(new_data, data_size);

// Check for complete HTTP request
if (incoming_data.find("\r\n\r\n") != std::string::npos) {
    // Complete headers received, can process
}
```

#### **🎯 Potential Evaluator Questions & Expert Answers**

**Q: Why might recv() return fewer bytes than requested?**
**A**: Several technical reasons:
1. **TCP Stream Nature**: TCP provides a byte stream, not message boundaries
2. **Network Fragmentation**: Large data may arrive in multiple IP packets
3. **Buffer Availability**: Only partial data available in kernel buffer
4. **Signal Interruption**: System call interrupted by signal (EINTR)
5. **Non-blocking Mode**: Would block, returns immediately with available data
6. **Flow Control**: Sender's window size limits data transmission

**Q: What happens if the recv() buffer is too small for incoming data?**
**A**: 
- **Data Truncation**: Only buffer-size bytes copied to user space
- **Remaining Data**: Stays in kernel TCP buffer for next recv() call
- **No Data Loss**: TCP guarantees delivery, data waits for next recv()
- **Your Buffer Size**: 7MB is very large, handles most HTTP requests
- **Best Practice**: Use reasonably sized buffers and loop until complete

**Q: How does recv() work with non-blocking sockets and epoll?**
**A**: 
```cpp
// Your architecture
1. Socket set to non-blocking: fcntl(fd, F_SETFL, O_NONBLOCK)
2. Added to epoll: epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)
3. epoll_wait() notifies when data available
4. recv() called, returns immediately with available data or EAGAIN
5. If EAGAIN, wait for next epoll notification
```
- **Advantage**: One slow client doesn't block others
- **Scalability**: Can handle thousands of concurrent connections
- **Efficiency**: CPU not wasted waiting for I/O

**Q: What's the difference between recv() and read() for sockets?**
**A**: 
- **recv()**: Socket-specific, has flags parameter for special behavior
- **read()**: Generic file I/O, works on any file descriptor
- **Functionality**: `read(fd, buf, len)` equals `recv(fd, buf, len, 0)`
- **Socket Features**: recv() provides MSG_PEEK, MSG_DONTWAIT, etc.
- **Portability**: recv() more portable across Unix systems for network code
- **Error Handling**: recv() provides socket-specific error codes

**Q: How do you handle connection drops during recv()?**
**A**: Multiple detection mechanisms:
```cpp
// 1. Graceful close (recv returns 0)
if (bytes_received == 0) {
    cleanup_connection();
}

// 2. Connection reset (ECONNRESET)
if (errno == ECONNRESET) {
    cleanup_connection();
}

// 3. epoll notifications (EPOLLHUP/EPOLLERR)
if (events & (EPOLLHUP | EPOLLERR)) {
    cleanup_connection();
}
```

**Q: Why null-terminate the recv() buffer?**
**A**: Your code does `buffer[bytes_received] = '\0'` because:
- **HTTP Protocol**: HTTP is text-based, benefits from string operations
- **String Functions**: Enables safe use of strlen(), strstr(), etc.
- **Debugging**: Makes buffer contents printable and debuggable
- **Safety**: Prevents buffer overruns in string operations
- **Parsing**: Simplifies HTTP header parsing with string methods

**Q: How do you prevent buffer overflow attacks?**
**A**: Multiple protection layers:
```cpp
// 1. Large buffer size
char buffer[7000000] = {0};  // 7MB buffer

// 2. Size validation
bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

// 3. Null termination
buffer[bytes_received] = '\0';

// 4. Request size limits (should add)
if (incoming_data.size() > MAX_REQUEST_SIZE) {
    return REQUEST_TOO_LARGE;
}
```

**Q: What happens during a recv() system call at the assembly level?**
**A**: 
```assembly
; Simplified x86_64 assembly
mov rax, 45          ; sys_recvfrom syscall number
mov rdi, sockfd      ; socket file descriptor
mov rsi, buffer      ; buffer address
mov rdx, length      ; buffer length
mov r10, 0           ; flags
syscall              ; trap to kernel
; Return value in rax (bytes received or -1)
```

**Q: How does TCP congestion control affect recv()?**
**A**: 
- **Slow Start**: Initial data arrives slowly, then accelerates
- **Congestion Window**: Limits sender's transmission rate
- **Receive Window**: Your buffer space advertised to sender
- **Flow Control**: Prevents sender from overwhelming receiver
- **Impact on recv()**: May receive data in bursts rather than steady stream

### **Data Reception Flow**: [`client/client.cpp:73-80`](client/client.cpp#L73-L80)

```cpp
if (bytes_received > 0)
{
    buffer[bytes_received] = '\0';  // Null-terminate for string operations
    std::cout << "=== CLIENT " << client_fd << ": PROCESSING REQUEST ===" << std::endl;
    increment_request_count();

    // Pass data to request parser
    RequestStatus result = current_request.add_new_data(buffer, bytes_received);
```

**Why null-terminate**: Allows safe string operations on the received data

---

## 🔍 **STEP 2: Request Parsing - Building the HTTP Request**

### Location: [`request/request.cpp:50-56`](request/request.cpp#L50-L56) - `add_new_data()`

```cpp
RequestStatus Request::add_new_data(const char *new_data, size_t data_size)
{
    std::cout << "=== GOT " << data_size << " NEW BYTES FROM CLIENT ===" << std::endl;

    // Append new data to our buffer
    incoming_data.append(new_data, data_size);
    std::cout << "Total data we have now: " << incoming_data.size() << " bytes" << std::endl;
```

### **HTTP Request Format (RFC 7230)**:

```
GET /path/to/resource HTTP/1.1\r\n
Host: example.com\r\n
User-Agent: Mozilla/5.0...\r\n
Content-Length: 123\r\n
\r\n
[request body]
```

### **Phase 1: Header Parsing** - [`request/request.cpp:58-71`](request/request.cpp#L58-L71)

```cpp
if (!got_all_headers)
{
    // Look for end of headers marker
    size_t headers_end_position = incoming_data.find("\r\n\r\n");
    if (headers_end_position == std::string::npos)
    {
        if (!check_for_valid_http_start())
        {
            std::cout << "Invalid HTTP request format detected" << std::endl;
            return BAD_REQUEST;
        }
        std::cout << "Headers are not complete yet - waiting for more data" << std::endl;
        return NEED_MORE_DATA;
    }
```

**What happens here**:

1. **Search for headers end**: Look for `\r\n\r\n` (double CRLF)
2. **Validate HTTP start**: Check if request starts with valid HTTP format
3. **Return status**: Either `NEED_MORE_DATA` or continue parsing

### **HTTP Request Line Validation** - [`request/request.cpp:103-125`](request/request.cpp#L103-L125):

```cpp
bool Request::check_for_valid_http_start()
{
    size_t first_line_end = incoming_data.find("\r\n");
    std::string first_line = incoming_data.substr(0, first_line_end);
    std::stringstream line_stream(first_line);
    std::string method, path, version;

    if (!(line_stream >> method >> path >> version))
    {
        std::cout << "error from stream \n";
        return false;
    }

    // Validate HTTP version
    if (version != "HTTP/1.1" && version != "HTTP/1.0")
        return false;

    // Validate path starts with /
    if (path[0] != '/')
        return false;

    // Validate method
    if (method != "GET" && method != "POST" && method != "DELETE")
        return false;

    return true;
}
```

**Validation checks**:

- **Method**: Only GET, POST, DELETE allowed
- **Path**: Must start with `/`
- **Version**: HTTP/1.0 or HTTP/1.1 only
- **Format**: Must have exactly 3 space-separated parts

### **Header Parsing Deep Dive** - [`request/request.cpp:127-175`](request/request.cpp#L127-L175):

```cpp
bool Request::parse_http_headers(const std::string &header_text)
{
    std::stringstream header_stream(header_text);
    std::string current_line;

    // Parse request line first
    if (!std::getline(header_stream, current_line))
        return false;
    std::stringstream first_line_stream(current_line);
    if (!(first_line_stream >> http_method >> requested_path >> http_version))
        return false;

    bool host_found = false;

    // Parse each header line
    while (std::getline(header_stream, current_line))
    {
        size_t colon_position = current_line.find(':');
        if (colon_position == std::string::npos)
        {
            std::cout << "no key value in header: '" << current_line << "'" << std::endl;
            return false;
        }

        // Check for invalid whitespace before colon (RFC violation)
        if (colon_position > 0 && (current_line[colon_position - 1] == ' ' ||
                                  current_line[colon_position - 1] == '\t'))
        {
            std::cout << "Invalid header (whitespace before colon)" << std::endl;
            return false;
        }

        std::string header_name = current_line.substr(0, colon_position);
        std::string header_value = current_line.substr(colon_position + 1);

        // Convert to lowercase for case-insensitive comparison
        std::string lower_name = remove_spaces_and_lower(header_name);
        http_headers[lower_name] = header_value;

        if (lower_name == "host")
            host_found = true;
    }

    // HTTP/1.1 requires Host header
    if (!host_found)
        return false;

    return true;
}
```

**Header parsing rules**:

- **Colon required**: Each header must have `name: value` format
- **No whitespace before colon**: RFC 7230 compliance
- **Case-insensitive names**: Convert to lowercase for storage
- **Host header required**: HTTP/1.1 specification requirement

### **Phase 2: Body Handling (POST requests)** - [`request/request.cpp:86-102`](request/request.cpp#L86-L102)

```cpp
if (http_method == "POST")
{
    std::map<std::string, std::string>::iterator it_content_len = http_headers.find("content-length");
    std::map<std::string, std::string>::iterator it_transfer_enc = http_headers.find("transfer-encoding");

    if (it_content_len != http_headers.end())
    {
        expected_body_size = std::atoi(it_content_len->second.c_str());
        std::cout << "This request should have a body with " << expected_body_size << " bytes" << std::endl;

        // Extract the request body from incoming_data
        if (incoming_data.size() >= expected_body_size) {
            request_body = incoming_data.substr(0, expected_body_size);
        } else {
            std::cout << "Not enough body data yet, have " << incoming_data.size()
                     << " need " << expected_body_size << std::endl;
            return NEED_MORE_DATA;
        }
    }
    else if (it_transfer_enc != http_headers.end() &&
             it_transfer_enc->second.find("chunked") != std::string::npos)
    {
        expected_body_size = 0;
        std::cout << "Using chunked transfer encoding - size unknown" << std::endl;
    }
    else
    {
        std::cout << "Missing Content-Length header and no chunked encoding" << std::endl;
        return LENGTH_REQUIRED;
    }
}
```

**Body handling logic**:

- **Content-Length**: Fixed-size body, wait for all bytes
- **Chunked encoding**: Variable-size body (not fully implemented)
- **Missing headers**: Return 411 Length Required error

---

## 🎯 **STEP 3: Request Processing Decision Point**

### Location: [`client/client.cpp:82-95`](client/client.cpp#L82-L95) - Processing the parsed request

```cpp
switch (result)
{
case NEED_MORE_DATA:
    std::cout << "We need more data from the client" << std::endl;
    return;

case HEADERS_ARE_READY:
{
    std::cout << "Processing request data - checking handler..." << std::endl;
    current_request.set_config(server_config);
    request_status = current_request.figure_out_http_method();
```

### **Configuration and Location Matching** - [`request/request.cpp:30-41`](request/request.cpp#L30-L41):

```cpp
void Request::set_config(ServerContext &cfg)
{
    config = &cfg;
    std::cout << " config is up \n";
    if (!requested_path.empty())
    {
        location = match_location(requested_path);
        if (location)
            std::cout << " location found: " << location->path << "\n";
        else
            std::cout << " location not found for path: " << requested_path << "\n";
    }
}
```

### **Location Matching Algorithm** - [`request/request.cpp:43-78`](request/request.cpp#L43-L78):

```cpp
LocationContext *Request::match_location(const std::string &requested_path)
{
    if (!config)
        return 0;
    LocationContext *matched_location = 0;
    size_t longest_len = 0;

    std::cout << "Matching path '" << requested_path << "' against locations:" << std::endl;
    std::vector<LocationContext>::iterator it_location = config->locations.begin();
    while (it_location != config->locations.end())
    {
        if (it_location->path.empty())
        {
            ++it_location;
            continue;
        }
        std::cout << "  Checking location: '" << it_location->path << "'" << std::endl;

        // Check if requested path starts with location path
        if (requested_path.compare(0, it_location->path.length(), it_location->path) == 0)
        {
            // Additional validation for proper path matching
            if (it_location->path == "/" ||
                requested_path.length() == it_location->path.length() ||
                (it_location->path[it_location->path.length() - 1] == '/') ||
                requested_path[it_location->path.length()] == '/')
            {
                std::cout << "matched location ;" << it_location->path << "\n";
                // Use longest match (most specific location)
                if (it_location->path.size() > longest_len)
                {
                    matched_location = &(*it_location);
                    longest_len = it_location->path.size();
                }
            }
        }
        ++it_location;
    }
    return matched_location;
}
```

**Location matching rules**:

- **Prefix matching**: Path must start with location path
- **Boundary checking**: Ensure proper path boundaries
- **Longest match**: Most specific location wins
- **Root handling**: Special case for "/" location

---

## 🔀 **STEP 4: Route Decision - CGI vs Regular File**

### **CGI Detection Logic** - [`client/client.cpp:107-112`](client/client.cpp#L107-L112):

```cpp
// Check if this is a CGI request
if (current_request.is_cgi_request()) {
    std::cout << "🔧 Detected CGI request - starting CGI process" << std::endl;
    LocationContext* location = current_request.get_location();
    // ... CGI processing path
}
```

**CGI Detection Method** - [`request/request.cpp:244-256`](request/request.cpp#L244-L256):

```cpp
bool Request::is_cgi_request() const {
    if (!location || location->cgiExtension.empty() || location->cgiPath.empty()) {
        return false;
    }

    // Check if the requested path ends with the CGI extension
    if (requested_path.size() >= location->cgiExtension.size()) {
        std::string file_ext = requested_path.substr(requested_path.size() - location->cgiExtension.size());
        return file_ext == location->cgiExtension;
    }

    return false;
}
```

**CGI detection criteria**:

- **Extension configured**: Location has `cgiExtension` set
- **Interpreter configured**: Location has `cgiPath` set
- **File extension match**: Request path ends with CGI extension

---

## 🐍 **PATH A: CGI Request Processing**

### **STEP A1: CGI Process Startup** - [`client/client.cpp:117-135`](client/client.cpp#L117-L135)

```cpp
if (location) {
    // Build script path
    std::string script_path = location->root + current_request.get_requested_path();

    // Start CGI process
    int cgi_output_fd = cgi_runner.start_cgi_process(current_request, *location, client_fd, script_path);
    if (cgi_output_fd >= 0) {
        // Add CGI output fd to epoll for monitoring
        struct epoll_event cgi_ev;
        cgi_ev.events = EPOLLIN;
        cgi_ev.data.fd = cgi_output_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi_output_fd, &cgi_ev) == -1) {
            std::cerr << "Failed to add CGI output fd to epoll" << std::endl;
            cgi_runner.cleanup_cgi_process(cgi_output_fd);
        } else {
            std::cout << "✅ CGI process started, monitoring output fd: " << cgi_output_fd << std::endl;
            return; // Don't switch to EPOLLOUT - CGI will handle response
        }
    }
}
```

### **STEP A2: CGI Process Creation** - [`cgi/cgi_runner.cpp:67-85`](cgi/cgi_runner.cpp#L67-L85)

```cpp
int CgiRunner::start_cgi_process(const Request& request,
                                const LocationContext& location,
                                int client_fd,
                                const std::string& script_path) {

    std::cout << "Starting CGI process for script: " << script_path << std::endl;
    std::cout << "CGI interpreter: " << location.cgiPath << std::endl;

    // Check if script file exists and is readable
    if (access(script_path.c_str(), F_OK) != 0) {
        std::cerr << "CGI script not found: " << script_path << std::endl;
        return -1;
    }

    if (access(script_path.c_str(), R_OK) != 0) {
        std::cerr << "CGI script not readable: " << script_path << std::endl;
        return -1;
    }
```

### 🔧 **Function Deep Dive: access()**

```cpp
#include <unistd.h>
int access(const char *pathname, int mode);
```

**What it does**: Tests file accessibility
**Parameters**:

- `script_path.c_str()`: Path to the file
- `F_OK`: Test for file existence
- `R_OK`: Test for read permission

**Return Values**:

- `0`: Test succeeded
- `-1`: Test failed (file doesn't exist or no permission)

### **Pipe Creation for Communication**:

```cpp
// Create pipes for communication
int input_pipe[2], output_pipe[2];
if (pipe(input_pipe) == -1 || pipe(output_pipe) == -1) {
    std::cerr << "Failed to create pipes for CGI" << std::endl;
    return -1;
}
```

### 🔧 **Function Deep Dive: pipe()**

```cpp
#include <unistd.h>
int pipe(int pipefd[2]);
```

**What it does**: Creates a pair of file descriptors for inter-process communication
**Parameters**:

- `pipefd[2]`: Array to store the two file descriptors

**Result**:

- `pipefd[0]`: Read end of the pipe
- `pipefd[1]`: Write end of the pipe

**How it works**: Data written to `pipefd[1]` can be read from `pipefd[0]`

### **Process Creation with fork()**:

```cpp
pid_t pid = fork();
if (pid < 0) {
    std::cerr << "Failed to fork CGI process" << std::endl;
    close(input_pipe[0]); close(input_pipe[1]);
    close(output_pipe[0]); close(output_pipe[1]);
    return -1;
}

if (pid == 0) {
    // Child process - execute CGI script
    // ... child process code
} else {
    // Parent process - manage CGI communication
    // ... parent process code
}
```

### 🔧 **Function Deep Dive: fork() - Complete Process Creation Analysis**

```cpp
#include <unistd.h>
pid_t fork(void);
```

#### **What fork() Actually Does at the Kernel Level**

**Process Creation Journey:**
```
Parent Process
        ↓ (fork() system call)
Kernel Process Management
        ↓
1. Allocate new Process Control Block (PCB)
2. Copy process memory (Copy-on-Write)
3. Duplicate file descriptor table
4. Set up parent-child relationship
5. Assign new Process ID (PID)
6. Make both processes runnable
        ↓
Two identical processes (except PID and return value)
```

**Kernel Internal Process:**
1. **PCB Allocation**: Create new process control block structure
2. **Memory Management**: Set up copy-on-write memory mappings
3. **File Descriptor Duplication**: Copy entire fd table with reference counts
4. **Process Tree Update**: Link child to parent in process hierarchy
5. **Scheduler Integration**: Add child process to scheduler's run queue
6. **Return Value Setup**: Set different return values for parent/child

#### **Copy-on-Write (COW) Optimization - Critical Concept**

**Before fork() - Single Process:**
```
Parent Process Memory:
[Code Segment] [Data Segment] [Heap] [Stack]
      ↓              ↓          ↓       ↓
[Physical Memory Pages - Read/Write]
```

**Immediately After fork() - COW Active:**
```
Parent Process:  [Virtual Memory] ↘
                                   → [Shared Physical Memory - Read Only]
Child Process:   [Virtual Memory] ↗

Both processes share same physical pages (marked read-only)
```

**After Write Operation - COW Triggered:**
```
Parent Process: [Virtual Memory] → [Original Physical Pages]
Child Process:  [Virtual Memory] → [New Physical Pages (copied)]

Only modified pages are actually copied
```

**Why COW is Revolutionary:**
- **Memory Efficiency**: No immediate copying of entire process memory
- **Speed**: fork() completes in microseconds regardless of process size
- **Lazy Allocation**: Pages copied only when actually modified
- **Your CGI**: Perfect for exec() immediately after fork()

#### **Return Values Deep Analysis**

**In Child Process (returns 0):**
```cpp
pid_t pid = fork();
if (pid == 0) {
    // This code runs in the CHILD process
    std::cout << "Child PID: " << getpid() << std::endl;
    std::cout << "Parent PID: " << getppid() << std::endl;
    
    // Child typically does exec() to run CGI script
    execve("/usr/bin/python3", argv, envp);
    _exit(1); // Only reached if exec fails
}
```

**In Parent Process (returns child's PID):**
```cpp
else if (pid > 0) {
    // This code runs in the PARENT process
    std::cout << "Parent PID: " << getpid() << std::endl;
    std::cout << "Child PID: " << pid << std::endl;
    
    // Parent manages child process
    active_cgi_processes[output_fd] = {pid, ...};
}
```

**Fork Failure (returns -1):**
```cpp
else {
    // Fork failed - no child created
    perror("fork failed");
    // Common causes: EAGAIN (process limit), ENOMEM (no memory)
}
```

#### **What Gets Copied vs What's Different**

**Copied to Child Process:**
```cpp
// Memory segments
- Code segment (shared, read-only)
- Data segment (copy-on-write)
- Heap (copy-on-write)  
- Stack (copy-on-write)

// File descriptors
- All open files, sockets, pipes
- Same file offset pointers
- Reference counts incremented

// Process attributes
- Environment variables
- Current working directory
- Signal handlers (initially)
- Process group ID
- Session ID
- Controlling terminal
- Resource limits (ulimit)
- Nice value
```

**Different in Child Process:**
```cpp
- Process ID (PID) - gets new unique PID
- Parent Process ID (PPID) - points to original process
- Pending signals - child starts with empty signal queue
- File locks - not inherited by child
- Process timers - reset in child
- Async I/O operations - not inherited
- Memory mappings - some may not be inherited
```

#### **File Descriptor Inheritance - Critical for CGI**

**Your CGI Implementation:**
```cpp
// Before fork() - parent has these FDs open
int input_pipe[2], output_pipe[2];
pipe(input_pipe);   // Creates FDs: input_pipe[0], input_pipe[1]
pipe(output_pipe);  // Creates FDs: output_pipe[0], output_pipe[1]

pid_t pid = fork();
if (pid == 0) {
    // Child inherits ALL file descriptors
    // Must close unused ends to prevent deadlock
    close(input_pipe[1]);   // Child doesn't write to input
    close(output_pipe[0]);  // Child doesn't read from output
    
    // Redirect stdin/stdout to pipes
    dup2(input_pipe[0], STDIN_FILENO);
    dup2(output_pipe[1], STDOUT_FILENO);
}
```

**File Descriptor Reference Counting:**
```cpp
// Each FD has reference count in kernel
// fork() increments reference count
// close() decrements reference count
// File actually closed when count reaches 0

// Example:
open("file.txt", O_RDONLY);  // ref_count = 1
fork();                      // ref_count = 2 (parent + child)
close(fd);  // in parent    // ref_count = 1
close(fd);  // in child     // ref_count = 0, file actually closed
```

#### **🎯 Potential Evaluator Questions & Expert Answers**

**Q: Why use fork() for CGI instead of threads?**
**A**: Critical advantages:
1. **Process Isolation**: CGI script crash can't affect server
2. **Memory Protection**: Separate address spaces prevent interference  
3. **Security**: Child process can be sandboxed with different privileges
4. **Resource Limits**: Can set per-process limits (CPU, memory, file descriptors)
5. **Clean Termination**: Can kill runaway processes with SIGKILL
6. **CGI Standard**: CGI/1.1 specification expects separate process
7. **Signal Isolation**: CGI signals don't affect server
8. **File Descriptor Control**: Precise control over inherited descriptors

**Q: What's the performance cost of fork()?**
**A**: 
- **Modern Systems**: Very fast due to copy-on-write optimization
- **Time Complexity**: O(1) for fork() itself, O(pages_modified) for COW
- **Memory Usage**: No immediate memory duplication
- **Your Use Case**: Minimal cost since child immediately calls exec()
- **Benchmarks**: Typically microseconds on modern hardware

**Q: How do you prevent zombie processes after fork()?**
**A**: Always reap child processes:
```cpp
// Method 1: Explicit waiting
pid_t pid = fork();
if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);  // Blocks until child exits
}

// Method 2: Non-blocking check
pid_t result = waitpid(pid, &status, WNOHANG);
if (result > 0) {
    // Child exited
} else if (result == 0) {
    // Child still running
}

// Method 3: Signal handler (your implementation)
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Reap all available children
    }
}
signal(SIGCHLD, sigchld_handler);
```

**Q: What happens if fork() fails?**
**A**: Common failure scenarios:
```cpp
if (fork() == -1) {
    switch (errno) {
        case EAGAIN:
            // Process limit reached (ulimit -u)
            std::cout << "Too many processes" << std::endl;
            break;
        case ENOMEM:
            // Insufficient memory for new process
            std::cout << "Out of memory" << std::endl;
            break;
        case ENOSYS:
            // fork() not supported (rare)
            std::cout << "fork() not available" << std::endl;
            break;
    }
}
```

**Q: How does fork() interact with threads?**
**A**: 
- **Single-threaded Process**: fork() creates identical copy
- **Multi-threaded Process**: Only calling thread is copied to child
- **Thread Safety**: fork() can cause deadlocks in multi-threaded programs
- **Your Server**: Single-threaded design avoids these issues
- **Best Practice**: Avoid fork() in multi-threaded programs

**Q: What's the difference between fork(), vfork(), and clone()?**
**A**: 
- **fork()**: Full process copy with COW (standard, portable)
- **vfork()**: Child shares memory until exec() (deprecated, dangerous)
- **clone()**: Linux-specific, fine-grained control over what's shared
- **Your Choice**: fork() is correct - portable and safe

**Q: How do you debug fork() issues?**
**A**: 
```cpp
// Add debugging output
pid_t pid = fork();
std::cout << "fork() returned: " << pid << std::endl;

if (pid == 0) {
    std::cout << "Child process: PID=" << getpid() 
              << " PPID=" << getppid() << std::endl;
} else if (pid > 0) {
    std::cout << "Parent process: PID=" << getpid() 
              << " Child=" << pid << std::endl;
}

// Use strace to trace system calls
// strace -f ./webserver  // -f follows forks
```

**Q: Why does your CGI implementation close pipe ends after fork()?**
**A**: Prevent deadlocks and ensure proper EOF:
```cpp
// Parent process
close(input_pipe[0]);   // Parent only writes to input
close(output_pipe[1]);  // Parent only reads from output

// Child process  
close(input_pipe[1]);   // Child only reads from input
close(output_pipe[0]);  // Child only writes to output

// If not closed:
// - Child might wait for EOF that never comes
// - Parent might block on full pipe buffer
// - Reference counts prevent proper cleanup
```

**Q: How does exec() after fork() affect copy-on-write?**
**A**: 
- **Immediate exec()**: COW pages never actually copied (very efficient)
- **Memory Replacement**: exec() replaces entire memory image
- **Optimization**: Kernel may skip COW setup if exec() detected quickly
- **Your Implementation**: Optimal pattern for CGI execution

### **STEP A3: Child Process Setup (CGI Script Execution)**

```cpp
if (pid == 0) {
    // Child process - execute CGI script
    close(input_pipe[1]);  // Close write end of input pipe
    close(output_pipe[0]); // Close read end of output pipe

    // Change to the directory containing the CGI script
    std::string script_dir = script_path;
    std::string script_filename = script_path;
    size_t last_slash = script_dir.find_last_of('/');
    if (last_slash != std::string::npos) {
        script_dir = script_dir.substr(0, last_slash);
        script_filename = script_path.substr(last_slash + 1);
        if (chdir(script_dir.c_str()) != 0) {
            std::cerr << "Failed to change directory to: " << script_dir << std::endl;
            _exit(1);
        }
        std::cout << "Changed working directory to: " << script_dir << std::endl;
    }
```

### 🔧 **Function Deep Dive: chdir()**

```cpp
#include <unistd.h>
int chdir(const char *path);
```

**What it does**: Changes the current working directory
**Why needed**: CGI scripts often expect to run from their own directory to access relative files
**Return Values**:

- `0`: Success
- `-1`: Failed (check errno)

### **I/O Redirection with dup2()**:

```cpp
// Redirect stdin and stdout
if (dup2(input_pipe[0], STDIN_FILENO) == -1 ||
    dup2(output_pipe[1], STDOUT_FILENO) == -1) {
    _exit(1);
}

// Close the original pipe file descriptors
close(input_pipe[0]);
close(output_pipe[1]);
```

### 🔧 **Function Deep Dive: dup2()**

```cpp
#include <unistd.h>
int dup2(int oldfd, int newfd);
```

**What it does**: Duplicates a file descriptor to a specific descriptor number
**Parameters**:

- `oldfd`: Source file descriptor
- `newfd`: Target file descriptor number

**In our case**:

- `dup2(input_pipe[0], STDIN_FILENO)`: CGI reads from our pipe
- `dup2(output_pipe[1], STDOUT_FILENO)`: CGI writes to our pipe

### **Environment Variable Setup**:

```cpp
// Build environment
std::vector<std::string> env_vars = build_cgi_env(request, "localhost", "8080", script_path);
std::vector<char*> envp = vector_to_char_array(env_vars);
```

```cpp
std::vector<std::string> CgiRunner::build_cgi_env(const Request& request,
                                                 const std::string& server_name,
                                                 const std::string& server_port,
                                                 const std::string& script_name) {
    std::vector<std::string> env;

    // Standard CGI environment variables
    env.push_back("REQUEST_METHOD=" + request.get_http_method());
    env.push_back("QUERY_STRING=");  // No query string support in this implementation
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("SERVER_NAME=" + server_name);
    env.push_back("SERVER_PORT=" + server_port);
    env.push_back("SCRIPT_NAME=" + script_name);
    env.push_back("PATH_INFO=");

    // Add HTTP headers as environment variables
    const std::map<std::string, std::string>& headers = request.get_all_headers();
    std::map<std::string, std::string>::const_iterator it;

    it = headers.find("content-type");
    if (it != headers.end()) {
        env.push_back("CONTENT_TYPE=" + it->second);
    }

    it = headers.find("content-length");
    if (it != headers.end()) {
        env.push_back("CONTENT_LENGTH=" + it->second);
    }

    // Add common HTTP headers
    it = headers.find("host");
    if (it != headers.end()) {
        env.push_back("HTTP_HOST=" + it->second);
    }

    it = headers.find("user-agent");
    if (it != headers.end()) {
        env.push_back("HTTP_USER_AGENT=" + it->second);
    }

    return env;
}
```

**CGI Environment Variables (CGI/1.1 Specification)**:

- `REQUEST_METHOD`: HTTP method (GET, POST, etc.)
- `QUERY_STRING`: URL parameters after `?`
- `CONTENT_LENGTH`: Size of request body
- `CONTENT_TYPE`: MIME type of request body
- `SERVER_PROTOCOL`: HTTP version
- `GATEWAY_INTERFACE`: CGI version
- `HTTP_*`: HTTP headers with HTTP\_ prefix

### **Script Execution with execve()**:

```cpp
// Build arguments
std::vector<std::string> args;
args.push_back(location.cgiPath);  // interpreter path (e.g., /usr/bin/python3)
args.push_back(script_filename);   // script filename (relative to working directory)
std::vector<char*> argv = vector_to_char_array(args);

// Execute the CGI script
execve(argv[0], &argv[0], &envp[0]);
_exit(127); // exec failed
```

### 🔧 **Function Deep Dive: execve() - Complete Program Execution Analysis**

```cpp
#include <unistd.h>
int execve(const char *pathname, char *const argv[], char *const envp[]);
```

#### **What execve() Actually Does at the Kernel Level**

**Process Image Replacement Journey:**
```
Current Process (CGI child)
        ↓ (execve() system call)
Kernel Program Loader
        ↓
1. Validate executable file
2. Parse executable format (ELF)
3. Unmap old memory regions
4. Map new program segments
5. Set up new stack with argv/envp
6. Initialize registers and jump to entry point
        ↓
New Program Starts Execution (Python interpreter)
```

**Kernel Internal Process:**
1. **File Validation**: Check executable exists, has execute permission
2. **Format Recognition**: Parse ELF header, identify program type
3. **Memory Unmapping**: Release all old memory mappings
4. **Program Loading**: Map code and data segments into memory
5. **Dynamic Linking**: Resolve shared library dependencies
6. **Stack Setup**: Create new stack with arguments and environment
7. **Register Initialization**: Set up CPU registers for program start
8. **Jump to Entry**: Transfer control to program's entry point (_start)

#### **Process Image Transformation**

**Before execve() - Child Process:**
```
Memory Layout:
[Webserver Code] [Webserver Data] [Webserver Heap] [Webserver Stack]
      ↓                ↓                ↓               ↓
[File Descriptors: pipes, sockets, etc.]
[Environment: webserver environment]
[PID: same as will be after exec]
```

**After execve() - Python Interpreter:**
```
Memory Layout:
[Python Code] [Python Data] [Python Heap] [Python Stack]
     ↓             ↓             ↓            ↓
[File Descriptors: inherited pipes, sockets]
[Environment: CGI environment variables]
[PID: unchanged]
```

#### **Parameters Deep Technical Analysis**

**`const char *pathname` - Executable Path:**
```cpp
// Your implementation
execve(location.cgiPath.c_str(), argv, envp);
// Example: execve("/usr/bin/python3", ...)
```
- **Path Resolution**: Can be absolute or relative to current directory
- **File Validation**: Must exist, be executable, and proper format
- **Shebang Handling**: Kernel recognizes `#!/usr/bin/python3` and adjusts execution
- **Security**: Kernel checks execute permissions and file ownership

**`char *const argv[]` - Command Line Arguments:**
```cpp
// Your CGI argument construction
std::vector<std::string> args;
args.push_back(location.cgiPath);     // argv[0] = "/usr/bin/python3"
args.push_back(script_filename);      // argv[1] = "script.py"
std::vector<char*> argv = vector_to_char_array(args);
argv.push_back(NULL);                 // NULL terminator required

// Kernel receives:
// argv[0] = "/usr/bin/python3"
// argv[1] = "script.py"  
// argv[2] = NULL
```
- **argv[0] Convention**: Traditionally program name (can be anything)
- **NULL Termination**: Array must end with NULL pointer
- **Memory Layout**: Kernel copies arguments to new process stack
- **Argument Limits**: System limits on total argument size (ARG_MAX)

**`char *const envp[]` - Environment Variables:**
```cpp
// Your CGI environment construction
std::vector<std::string> env_vars = build_cgi_env(request, ...);
// Results in:
// "REQUEST_METHOD=GET"
// "CONTENT_LENGTH=123"
// "HTTP_HOST=localhost"
// etc.

std::vector<char*> envp = vector_to_char_array(env_vars);
envp.push_back(NULL);  // NULL terminator
```
- **Format**: Each string is "KEY=VALUE"
- **CGI Standard**: Specific environment variables required by CGI/1.1
- **NULL Termination**: Array must end with NULL pointer
- **Inheritance**: Replaces entire environment (doesn't inherit parent's)

#### **What Gets Preserved vs Replaced**

**Preserved Across execve():**
```cpp
// Process identity
- Process ID (PID) - stays exactly the same
- Parent Process ID (PPID) - unchanged
- Process group ID (PGID) - preserved
- Session ID (SID) - preserved

// File descriptors (unless close-on-exec)
- Pipes (your CGI communication pipes)
- Sockets (if any were inherited)
- Open files (unless FD_CLOEXEC set)

// Process attributes
- Current working directory (unless changed)
- File mode creation mask (umask)
- Process priority (nice value)
- Resource limits (ulimit settings)
```

**Completely Replaced:**
```cpp
// Memory contents
- All code segments (program instructions)
- All data segments (global variables)
- Entire heap (malloc'd memory)
- Complete stack (local variables, call stack)

// Signal handling
- Signal handlers reset to default (SIG_DFL)
- Signal mask cleared
- Pending signals cleared

// Program state
- CPU registers reinitialized
- Program counter set to entry point
- Stack pointer set to new stack
```

#### **ELF Binary Loading Process**

**ELF Header Parsing:**
```cpp
// Simplified ELF header structure
struct elf_header {
    unsigned char e_ident[16];  // Magic number, class, endianness
    uint16_t e_type;            // Executable, shared object, etc.
    uint16_t e_machine;         // Target architecture (x86_64, ARM, etc.)
    uint32_t e_version;         // ELF version
    uint64_t e_entry;           // Entry point address
    uint64_t e_phoff;           // Program header offset
    // ... more fields
};
```

**Memory Mapping Process:**
```cpp
// Kernel maps program segments
for (each program header) {
    if (segment_type == PT_LOAD) {
        mmap(virtual_addr, size, permissions, MAP_PRIVATE, fd, offset);
    }
}

// Typical segments:
// .text   - executable code (read-only)
// .data   - initialized data (read-write)  
// .bss    - uninitialized data (read-write, zero-filled)
// .rodata - read-only data (constants)
```

#### **Dynamic Linking and Library Loading**

**Shared Library Resolution:**
```cpp
// Python interpreter depends on shared libraries
// ldd /usr/bin/python3 shows:
// libpython3.x.so => /usr/lib/libpython3.x.so
// libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
// libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2

// Kernel/dynamic linker process:
1. Parse ELF dynamic section
2. Load required shared libraries
3. Resolve symbol references
4. Apply relocations
5. Call library initialization functions
```

#### **🎯 Potential Evaluator Questions & Expert Answers**

**Q: Why does execve() never return on success?**
**A**: 
- **Complete Replacement**: Entire process memory image is replaced
- **No Return Path**: The code that called execve() no longer exists
- **New Program**: Control transfers to new program's entry point
- **Only Failure Returns**: execve() only returns if it fails to load new program
- **Your Code**: `_exit(127)` after execve() only runs if exec failed

**Q: How do file descriptors behave across execve()?**
**A**: 
```cpp
// Default behavior: FDs remain open
int fd = open("file.txt", O_RDONLY);
execve("/usr/bin/python3", argv, envp);
// Python can still access fd

// Close-on-exec flag: FD closed during exec
fcntl(fd, F_SETFD, FD_CLOEXEC);
execve(...);
// fd is automatically closed

// Your CGI pipes: Remain open for communication
dup2(input_pipe[0], STDIN_FILENO);   // Python reads from this
dup2(output_pipe[1], STDOUT_FILENO); // Python writes to this
```

**Q: What happens if execve() fails?**
**A**: 
```cpp
execve("/usr/bin/python3", argv, envp);
// If we reach here, exec failed
perror("execve failed");
_exit(127);  // Standard exit code for exec failure

// Common failure reasons:
// ENOENT - file not found
// EACCES - permission denied  
// ENOEXEC - not executable format
// ENOMEM - insufficient memory
// E2BIG - argument list too long
```

**Q: How does the shebang (#!) mechanism work?**
**A**: 
```python
#!/usr/bin/python3
print("Hello from CGI")
```
```cpp
// When kernel sees shebang:
1. Reads first line of script file
2. Extracts interpreter path: "/usr/bin/python3"
3. Modifies exec arguments:
   // Original: execve("script.py", ["script.py"], envp)
   // Becomes:  execve("/usr/bin/python3", ["/usr/bin/python3", "script.py"], envp)
4. Executes interpreter with script as argument
```

**Q: What's the difference between the exec family functions?**
**A**: 
```cpp
// execve() - most basic, takes environment
execve("/usr/bin/python3", argv, envp);

// execv() - uses current environment  
execv("/usr/bin/python3", argv);

// execl() - arguments as separate parameters
execl("/usr/bin/python3", "python3", "script.py", NULL);

// execlp() - searches PATH
execlp("python3", "python3", "script.py", NULL);

// Your choice: execve() for full control over environment
```

**Q: How does execve() handle different executable formats?**
**A**: 
- **ELF**: Native Linux executable format (most common)
- **Script**: Text file with shebang (#!/interpreter)
- **a.out**: Legacy Unix format (rarely used)
- **Format Detection**: Kernel checks magic numbers in file header
- **Your CGI**: Python scripts use shebang mechanism

**Q: What happens to memory mappings during execve()?**
**A**: 
```cpp
// Before execve():
// - Webserver code mapped at various addresses
// - Shared libraries (libc, libstdc++) mapped
// - Heap and stack regions allocated

// During execve():
// - All old mappings unmapped (munmap())
// - New program segments mapped
// - New shared libraries loaded
// - Fresh heap and stack created

// After execve():
// - Completely new memory layout
// - Python interpreter and libraries
// - No trace of webserver memory
```

**Q: How do you debug execve() failures?**
**A**: 
```cpp
// Check file existence and permissions
if (access(pathname, F_OK) != 0) {
    perror("File not found");
}
if (access(pathname, X_OK) != 0) {
    perror("Not executable");
}

// Use strace to see system calls
// strace -f ./webserver
// Shows exact execve() call and failure reason

// Check environment and arguments
for (int i = 0; argv[i]; i++) {
    std::cout << "argv[" << i << "] = " << argv[i] << std::endl;
}
```

**Q: Why use execve() instead of system() for CGI?**
**A**: 
- **Security**: No shell interpretation, prevents injection attacks
- **Performance**: Direct execution, no shell overhead
- **Control**: Precise control over arguments and environment
- **Error Handling**: Better error reporting and handling
- **Resource Management**: Cleaner process management
- **CGI Standard**: CGI expects direct process execution

**Q: How does execve() interact with process scheduling?**
**A**: 
- **Process Priority**: Inherited from parent (nice value preserved)
- **Scheduler State**: Process remains in same scheduling class
- **CPU Affinity**: Processor affinity settings preserved
- **Resource Limits**: All ulimit settings carried forward
- **Real-time**: Real-time scheduling parameters preserved

### **STEP A4: Parent Process Setup (Server Side)**

```cpp
// Parent process
close(input_pipe[0]);  // Close read end of input pipe
close(output_pipe[1]); // Close write end of output pipe

// Make output pipe non-blocking
int flags = fcntl(output_pipe[0], F_GETFL, 0);
fcntl(output_pipe[0], F_SETFL, flags | O_NONBLOCK);
```

### 🔧 **Function Deep Dive: fcntl()**

```cpp
#include <fcntl.h>
int fcntl(int fd, int cmd, ...);
```

**What it does**: Performs control operations on file descriptors
**Commands used**:

- `F_GETFL`: Get file descriptor flags
- `F_SETFL`: Set file descriptor flags

**Flags**:

- `O_NONBLOCK`: Non-blocking I/O mode

**Why non-blocking**: Prevents server from hanging if CGI doesn't produce output immediately

### **Process Information Storage**:

```cpp
// Store CGI process info
CgiProcess cgi_proc;
cgi_proc.pid = pid;
cgi_proc.input_fd = input_pipe[1];
cgi_proc.output_fd = output_pipe[0];
cgi_proc.client_fd = client_fd;
cgi_proc.script_path = script_path;
cgi_proc.finished = false;
cgi_proc.start_time = time(NULL);

active_cgi_processes[output_pipe[0]] = cgi_proc;
```

### **Request Body Transmission (POST requests)**:

```cpp
// Write request body to CGI stdin if it's a POST request
if (request.get_http_method() == "POST" && !request.get_request_body().empty()) {
    const std::string& body = request.get_request_body();
    write(input_pipe[1], body.c_str(), body.size());
}

// Close input pipe to signal EOF to CGI
close(input_pipe[1]);
active_cgi_processes[output_pipe[0]].input_fd = -1;
```

### 🔧 **Function Deep Dive: write()**

```cpp
#include <unistd.h>
ssize_t write(int fd, const void *buf, size_t count);
```

**What it does**: Writes data to a file descriptor
**Parameters**:

- `fd`: File descriptor to write to
- `buf`: Data to write
- `count`: Number of bytes to write

**Return Values**:

- `> 0`: Number of bytes written
- `0`: Nothing written
- `-1`: Error occurred

**Why close input pipe**: Signals EOF to CGI script (no more input coming)

### 🔧 **Function Deep Dive: epoll_wait() - Complete Event Monitoring Analysis**

```cpp
#include <sys/epoll.h>
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

#### **What epoll_wait() Actually Does at the Kernel Level**

**Event Monitoring Journey:**
```
Application calls epoll_wait()
        ↓
Kernel epoll subsystem
        ↓
1. Check ready list for immediate events
2. If no events ready, add process to wait queue
3. Process sleeps (if timeout != 0)
4. Network/file events trigger callbacks
5. Callbacks add events to ready list
6. Process woken up
7. Events copied to user space
        ↓
Return to application with ready events
```

**Why epoll is Superior to select/poll:**
- **O(1) Performance**: Time doesn't increase with number of monitored FDs
- **Event-Driven**: Only returns FDs with actual events, no scanning
- **Scalability**: Can handle 100,000+ concurrent connections
- **Memory Efficiency**: Uses O(monitored_fds) vs O(max_fd)

#### **Parameters Deep Analysis**

**`int epfd`**: Epoll instance file descriptor (from epoll_create1())
**`struct epoll_event *events`**: Array to store ready events
**`int maxevents`**: Maximum events to return (prevents buffer overflow)
**`int timeout`**: 
- `-1`: Block forever until events arrive
- `0`: Return immediately (non-blocking)
- `> 0`: Wait up to timeout milliseconds

#### **Return Values**

**Positive (> 0)**: Number of ready file descriptors
```cpp
int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
for (int i = 0; i < nfds; i++) {
    int fd = events[i].data.fd;
    if (events[i].events & EPOLLIN) {
        // Data available for reading
        handle_client_input(fd);
    }
    if (events[i].events & EPOLLOUT) {
        // Ready for writing
        handle_client_output(fd);
    }
    if (events[i].events & (EPOLLERR | EPOLLHUP)) {
        // Error or connection closed
        cleanup_connection(fd);
    }
}
```

**Zero (0)**: Timeout occurred (only if timeout > 0)
**Negative (-1)**: Error occurred (check errno)

#### **🎯 Evaluator Questions & Answers**

**Q: Why use epoll instead of select/poll?**
**A**: 
- **Performance**: O(1) vs O(n) complexity
- **Scalability**: No 1024 FD limit like select()
- **Efficiency**: Only returns FDs with actual events
- **Memory**: Doesn't copy large FD sets between user/kernel space

**Q: What's edge-triggered vs level-triggered mode?**
**A**: 
- **Level-triggered (default)**: Event fires while condition is true
- **Edge-triggered (EPOLLET)**: Event fires when condition changes
- **Your server**: Uses level-triggered for simpler programming

**Q: How do you handle EINTR from epoll_wait()?**
**A**: 
```cpp
while (server_running) {
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
        if (errno == EINTR) {
            continue;  // Signal interrupted, retry
        } else {
            perror("epoll_wait failed");
            break;
        }
    }
    // Process events...
}
```

### 🔧 **Function Deep Dive: pipe() - Inter-Process Communication**

```cpp
#include <unistd.h>
int pipe(int pipefd[2]);
```

#### **What pipe() Creates**

**Pipe Architecture:**
```
Write End (pipefd[1]) → [Kernel Buffer] → Read End (pipefd[0])
                         (FIFO Queue)
```

**Characteristics:**
- **Unidirectional**: Data flows one way only
- **FIFO**: First In, First Out ordering
- **Atomic**: Small writes (< PIPE_BUF) are atomic
- **Blocking**: Blocks when buffer full (write) or empty (read)
- **Capacity**: Limited buffer size (typically 64KB)

#### **Your CGI Implementation**

```cpp
int input_pipe[2], output_pipe[2];
pipe(input_pipe);   // Server writes, CGI reads
pipe(output_pipe);  // CGI writes, server reads

// After fork():
// Parent (server):
close(input_pipe[0]);   // Don't read from input
close(output_pipe[1]);  // Don't write to output

// Child (CGI):
close(input_pipe[1]);   // Don't write to input
close(output_pipe[0]);  // Don't read from output
dup2(input_pipe[0], STDIN_FILENO);   // CGI reads from stdin
dup2(output_pipe[1], STDOUT_FILENO); // CGI writes to stdout
```

#### **🎯 Evaluator Questions & Answers**

**Q: Why close unused pipe ends?**
**A**: 
- **Prevent deadlock**: Ensures proper EOF detection
- **Resource cleanup**: Reduces file descriptor usage
- **Reference counting**: Allows proper pipe closure
- **Flow control**: Prevents blocking on full buffers

**Q: What happens if pipe buffer fills up?**
**A**: 
- **Blocking mode**: write() blocks until space available
- **Non-blocking mode**: write() returns EAGAIN
- **Reader must consume**: Data to free buffer space
- **Typical size**: 64KB buffer in kernel

### 🔧 **Function Deep Dive: dup2() - File Descriptor Redirection**

```cpp
#include <unistd.h>
int dup2(int oldfd, int newfd);
```

#### **What dup2() Does**

**File Descriptor Redirection:**
```cpp
// Before dup2():
fd 0 (stdin)  → terminal
fd 3 (pipe)   → pipe

// After dup2(pipe_fd, STDIN_FILENO):
fd 0 (stdin)  → pipe  (same as fd 3)
fd 3 (pipe)   → pipe
```

#### **CGI Redirection Process**

```cpp
// Redirect CGI stdin/stdout to pipes
dup2(input_pipe[0], STDIN_FILENO);    // CGI reads from server
dup2(output_pipe[1], STDOUT_FILENO);  // CGI writes to server
dup2(output_pipe[1], STDERR_FILENO);  // CGI errors to server

// Now CGI script's printf() goes to server
// And CGI script's scanf() reads from server
```

#### **🎯 Evaluator Questions & Answers**

**Q: Why use dup2() for CGI redirection?**
**A**: 
- **Standard I/O**: CGI scripts expect stdin/stdout
- **Transparent**: Script doesn't know about pipes
- **Atomic**: Close and duplicate in one operation
- **Thread-safe**: No race conditions

**Q: What happens to original file descriptor?**
**A**: 
- **Both point to same file**: oldfd and newfd reference same file
- **Independent**: Closing one doesn't affect the other
- **Reference counting**: File closed when all references closed

### 🔧 **Function Deep Dive: stat() - File Information**

```cpp
#include <sys/stat.h>
int stat(const char *pathname, struct stat *statbuf);
```

#### **What stat() Provides**

```cpp
struct stat file_info;
if (stat(file_path.c_str(), &file_info) == 0) {
    if (S_ISDIR(file_info.st_mode)) {
        // It's a directory
        handle_directory_request();
    } else if (S_ISREG(file_info.st_mode)) {
        // It's a regular file
        handle_file_request();
    }
    
    std::cout << "File size: " << file_info.st_size << " bytes" << std::endl;
    std::cout << "Last modified: " << ctime(&file_info.st_mtime) << std::endl;
}
```

#### **File Type Checking Macros**

- **S_ISREG()**: Regular file
- **S_ISDIR()**: Directory
- **S_ISLNK()**: Symbolic link
- **S_ISFIFO()**: Named pipe
- **S_ISSOCK()**: Socket

#### **🎯 Evaluator Questions & Answers**

**Q: What's the difference between stat(), lstat(), and fstat()?**
**A**: 
- **stat()**: Follows symbolic links
- **lstat()**: Doesn't follow symbolic links (returns link info)
- **fstat()**: Works on open file descriptor instead of path

**Q: How do you check file permissions?**
**A**: 
```cpp
if (file_info.st_mode & S_IRUSR) // Owner read
if (file_info.st_mode & S_IWUSR) // Owner write  
if (file_info.st_mode & S_IXUSR) // Owner execute
```

### 🔧 **Function Deep Dive: close() - Resource Cleanup**

```cpp
#include <unistd.h>
int close(int fd);
```

#### **What close() Does**

**For Regular Files:**
- Decrements reference count
- Flushes any pending writes
- Frees file descriptor number

**For Sockets:**
- Sends TCP FIN packet (graceful close)
- Initiates connection termination sequence
- Enters TIME_WAIT state

**For Pipes:**
- Signals EOF to readers (if last write end)
- Wakes up blocked readers/writers

#### **🎯 Evaluator Questions & Answers**

**Q: What happens if you close() twice?**
**A**: 
- **First close()**: Succeeds, frees the descriptor
- **Second close()**: Returns -1 with errno=EBADF
- **Security risk**: FD number might be reused by another file

**Q: Do you need to close() before exit()?**
**A**: 
- **Automatic cleanup**: Kernel closes all FDs on process exit
- **Good practice**: Explicit cleanup in long-running programs
- **Resource limits**: Prevents hitting FD limits

### **STEP A5: CGI Output Monitoring** - [`Server_setup/server.cpp:75-105`](Server_setup/server.cpp#L75-L105)

```cpp
else if (is_cgi_socket(fd)) {
    // Handle CGI process I/O here
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
                    ssize_t sent = send(client_fd, response_data.c_str(), response_data.size(), 0);
                    if (sent > 0) {
                        std::cout << "Sent " << sent << " bytes of CGI response to client " << client_fd << std::endl;
                    }

                    // Close client connection after sending response
                    client_it->second.cleanup_connection(epoll_fd, active_clients);
                }
            }

            // Clean up CGI process
            cgi_runner.cleanup_cgi_process(fd);
        }
    }
```

### **STEP A6: CGI Output Processing** - [`cgi/cgi_runner.cpp:179-230`](cgi/cgi_runner.cpp#L179-L230)

```cpp
bool CgiRunner::handle_cgi_output(int fd, std::string& response_data) {
    std::map<int, CgiProcess>::iterator it = active_cgi_processes.find(fd);
    if (it == active_cgi_processes.end()) {
        std::cout << "CGI fd " << fd << " not found in active processes" << std::endl;
        return false;
    }

    // Check if process is already finished to prevent infinite loop
    if (it->second.finished) {
        std::cout << "CGI process already finished, returning cached response" << std::endl;
        response_data = format_cgi_response(it->second.output_buffer);
        return true;
    }

    // Check for timeout (30 seconds)
    time_t current_time = time(NULL);
    if (current_time - it->second.start_time > 30) {
        std::cout << "CGI process timed out, killing process" << std::endl;
        if (it->second.pid > 0) {
            kill(it->second.pid, SIGKILL);
            waitpid(it->second.pid, NULL, 0);
        }
        it->second.finished = true;
        response_data = "HTTP/1.1 504 Gateway Timeout\r\nContent-Type: text/plain\r\nContent-Length: 19\r\nConnection: close\r\n\r\nCGI process timeout";
        return true;
    }
```

### 🔧 **Function Deep Dive: kill()**

```cpp
#include <signal.h>
int kill(pid_t pid, int sig);
```

**What it does**: Sends a signal to a process
**Parameters**:

- `pid`: Process ID to signal
- `sig`: Signal to send

**Signals used**:

- `SIGKILL`: Forcefully terminate process (cannot be ignored)
- `SIGTERM`: Request process termination (can be handled)

### 🔧 **Function Deep Dive: waitpid()**

```cpp
#include <sys/wait.h>
pid_t waitpid(pid_t pid, int *status, int options);
```

**What it does**: Waits for a child process to change state
**Parameters**:

- `pid`: Process ID to wait for
- `status`: Where to store exit status (can be NULL)
- `options`: Wait options (0 = block until process exits)

**Why needed**: Prevents zombie processes (dead processes that haven't been cleaned up)

### **Reading CGI Output**:

```cpp
char buffer[4096];
ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

if (bytes_read > 0) {
    it->second.output_buffer.append(buffer, bytes_read);
    std::cout << "Read " << bytes_read << " bytes from CGI process, total: " << it->second.output_buffer.size() << std::endl;
    return false; // Continue reading, don't send response yet
} else if (bytes_read == 0) {
    // EOF - CGI process finished
    std::cout << "CGI process finished (EOF), total output: " << it->second.output_buffer.size() << " bytes" << std::endl;
    it->second.finished = true;

    // Wait for the child process to avoid zombies
    if (it->second.pid > 0) {
        int status;
        pid_t result = waitpid(it->second.pid, &status, WNOHANG);
        if (result > 0) {
            std::cout << "CGI process " << it->second.pid << " exited with status " << status << std::endl;
        } else if (result == 0) {
            // Process still running, wait for it
            result = waitpid(it->second.pid, &status, 0);
            if (result > 0) {
                std::cout << "CGI process " << it->second.pid << " exited with status " << status << std::endl;
            }
        }
    }

    // Format CGI output as HTTP response
    response_data = format_cgi_response(it->second.output_buffer);
    return true; // Response is ready
} else {
    // Error or would block
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return false; // Would block, try again later
    } else {
        std::cerr << "Error reading from CGI process: " << strerror(errno) << std::endl;
        it->second.finished = true;
        response_data = "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nCGI process failed.\n";
        return true;
    }
}
```

### 🔧 **Function Deep Dive: read()**

```cpp
#include <unistd.h>
ssize_t read(int fd, void *buf, size_t count);
```

**What it does**: Reads data from a file descriptor
**Return Values**:

- `> 0`: Number of bytes read
- `0`: End of file (EOF)
- `-1`: Error or would block

**Reading states**:

- **Data available**: Accumulate in buffer, continue reading
- **EOF**: CGI finished, format response
- **Would block**: Try again later (non-blocking I/O)
- **Error**: Generate error response

### **STEP A7: CGI Response Formatting** - [`cgi/cgi_runner.cpp:290-340`](cgi/cgi_runner.cpp#L290-L340)

```cpp
std::string CgiRunner::format_cgi_response(const std::string& cgi_output) {
    // Split CGI output into headers and body
    std::string headers, body;
    size_t header_end = cgi_output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = cgi_output.find("\n\n");
        if (header_end != std::string::npos) {
            headers = cgi_output.substr(0, header_end);
            body = cgi_output.substr(header_end + 2);
        } else {
            // No headers found, treat entire output as body
            headers = "";
            body = cgi_output;
        }
    } else {
        headers = cgi_output.substr(0, header_end);
        body = cgi_output.substr(header_end + 4);
    }

    // Extract Content-Type from CGI headers
    std::string content_type = "text/plain; charset=utf-8";
    if (!headers.empty()) {
        std::istringstream header_stream(headers);
        std::string line;
        while (std::getline(header_stream, line)) {
            if (line.find("Content-Type:") == 0) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    content_type = line.substr(colon_pos + 1);
                    // Trim whitespace
                    while (!content_type.empty() && (content_type[0] == ' ' || content_type[0] == '\t')) {
                        content_type.erase(0, 1);
                    }
                    if (!content_type.empty() && content_type[content_type.size() - 1] == '\r') {
                        content_type.erase(content_type.size() - 1);
                    }
                }
                break;
            }
        }
    }

    // Build HTTP response
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;

    return response.str();
}
```

**CGI Response Format**:

```
Content-Type: text/html

<html><body><h1>Hello from CGI!</h1></body></html>
```

**HTTP Response Format**:

```
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 52
Connection: close

<html><body><h1>Hello from CGI!</h1></body></html>
```

**Formatting steps**:

1. **Split headers and body**: Look for double newline separator
2. **Extract Content-Type**: Parse CGI headers for content type
3. **Build HTTP response**: Add status line, headers, and body
4. **Calculate Content-Length**: Set based on body size

---

## 📁 **PATH B: Regular File Request Processing**

### **STEP B1: Method Handler Selection** - [`request/request.cpp:177-242`](request/request.cpp#L177-L242)

```cpp
RequestStatus Request::figure_out_http_method()
{
    // Check for return directive first
    if (location && !location->returnDirective.empty())
    {
        std::cout << "Found return directive: " << location->returnDirective << std::endl;
        return EVERYTHING_IS_OK;
    }

    // Check if location exists and has root
    if (!location || location->root.empty())
    {
        std::cout << " location not found \n";
        return NOT_FOUND;
    }

    // Check allowed methods
    if (!location->allowedMethods.empty())
    {
        bool ok = false;
        for (std::vector<std::string>::iterator it_method = location->allowedMethods.begin();
             it_method != location->allowedMethods.end(); ++it_method)
        {
            if (*it_method == http_method)
            {
                ok = true;
                break;
            }
        }
        if (!ok)
            return METHOD_NOT_ALLOWED;
    }

    // Check if this is a CGI request first, before handling with regular handlers
    if (!location->cgiExtension.empty() && !location->cgiPath.empty()) {
        if (requested_path.size() >= location->cgiExtension.size()) {
            std::string file_ext = requested_path.substr(requested_path.size() - location->cgiExtension.size());
            if (file_ext == location->cgiExtension) {
                // This is a CGI request - return success and let client handle CGI processing
                return EVERYTHING_IS_OK;
            }
        }
    }

    // Route to appropriate handler
    std::string full_path = location->root + requested_path;
    if (http_method == "GET")
        return get_handler.handle_get_request(full_path);
    else if (http_method == "POST")
    {
        return post_handler.handle_post_request(http_headers, incoming_data, expected_body_size, config, location);
    }
    else if (http_method == "DELETE")
        return delete_handler.handle_delete_request(full_path);
    else
        return METHOD_NOT_ALLOWED;
}
```

**Request processing order**:

1. **Return directive**: Handle redirects first
2. **Location validation**: Ensure location exists and has root
3. **Method validation**: Check if method is allowed
4. **CGI detection**: Check for CGI before file handlers
5. **Handler routing**: Route to GET/POST/DELETE handler

### **STEP B2: Response Generation** - [`client/client.cpp:155-200`](client/client.cpp#L155-L200)

```cpp
void Client::handle_client_data_output(int client_fd, int epoll_fd,
                                       std::map<int, Client> &active_clients, ServerContext &server_config)
{
    std::cout << "=== GENERATING RESPONSE FOR CLIENT ===========>" << client_fd << " ===" << std::endl;

    if (current_response.is_still_streaming())
    {
        std::cout << "Continuing file streaming..." << std::endl;
        current_response.handle_response(client_fd);
    }
    else
    {
        // Handle different response types
        if (request_status == DELETED_SUCCESSFULLY)
        {
            current_response.set_code(200);
            current_response.set_content("<html><body><h1>200 OK</h1><p>File deleted successfully.</p></body></html>");
            current_response.set_header("Content-Type", "text/html");
            current_response.set_header("Connection", "close");
        }
        else if (request_status == POSTED_SUCCESSFULLY)
        {
            current_response.set_code(201);
            current_response.set_content("<html><body><h1>201 Created</h1><p>File created successfully.</p></body></html>");
            current_response.set_header("Content-Type", "text/html");
            current_response.set_header("Connection", "close");
        }
        else if (request_status != EVERYTHING_IS_OK)
        {
            std::cout << "Setting error response for status: " << request_status << std::endl;
            current_response.set_error_response(request_status);
        }
        else
        {
            std::string request_path = current_request.get_requested_path();
            LocationContext *location = current_request.get_location();
            std::cout << "Creating normal response for path: " << request_path << std::endl;
            current_response.analyze_request_and_set_response(request_path, location);
        }

        current_response.handle_response(client_fd);
    }

    if (!current_response.is_still_streaming())
    {
        std::cout << "Response complete - cleaning up connection" << std::endl;
        cleanup_connection(epoll_fd, active_clients);
    }
    else
    {
        std::cout << "File streaming in progress - keeping connection alive" << std::endl;
    }
}
```

### **STEP B3: File Analysis and Response Setup** - [`response/response.cpp:150-200`](response/response.cpp#L150-L200)

```cpp
void Response::analyze_request_and_set_response(const std::string &path, LocationContext *location_config)
{
    // Handle return directive (redirects)
    if (!location_config->returnDirective.empty())
    {
        if (handle_return_directive(location_config->returnDirective))
            return;
    }

    std::string file_path = location_config->root + path;
    std::cout << "=== ANALYZING REQUEST PATH: " << file_path << " ===" << std::endl;

    struct stat s;
    if (stat(file_path.c_str(), &s) == 0)
    {
        if (s.st_mode & S_IFDIR)
        {
            // Directory handling
            std::vector<std::string> index_files;
            if (location_config && !location_config->indexes.empty())
            {
                index_files = location_config->indexes;

                bool index_found = false;
                for (std::vector<std::string>::iterator index_it = index_files.begin();
                     index_it != index_files.end(); ++index_it)
                {
                    std::string index_path = file_path;
                    if (index_path[index_path.length() - 1] != '/')
                        index_path += "/";
                    index_path += *index_it;

                    std::ifstream index_file(index_path.c_str());
                    if (index_file.is_open())
                    {
                        std::cout << "Found index file: " << *index_it << std::endl;
                        check_file(index_path);
                        index_found = true;
                        break;
                    }
                }

                if (!index_found)
                {
                    handle_directory_listing(file_path, path, location_config);
                }
            }
            else
            {
                handle_directory_listing(file_path, path, location_config);
            }
        }
        else
        {
            // Regular file
            check_file(file_path);
        }
    }
    else
    {
        // File not found
        set_code(404);
        set_content("<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>");
        set_header("Content-Type", "text/html");
        std::cout << "Path does not exist - returning 404 Not Found" << std::endl;
    }
}
```

### 🔧 **Function Deep Dive: stat()**

```cpp
#include <sys/stat.h>
int stat(const char *pathname, struct stat *statbuf);
```

**What it does**: Gets file status information
**Parameters**:

- `pathname`: Path to the file
- `statbuf`: Structure to store file information

**struct stat members**:

- `st_mode`: File type and permissions
- `st_size`: File size in bytes
- `st_mtime`: Last modification time

**File type checking**:

- `S_IFDIR`: Directory
- `S_IFREG`: Regular file
- `S_IFLNK`: Symbolic link

### **Directory Handling**:

```cpp
void Response::handle_directory_listing(const std::string &file_path, const std::string &path, LocationContext *location_config)
{
    if (location_config && location_config->autoindex == "on")
    {
        std::cout << "Generating directory listing" << std::endl;
        std::string dir_listing = list_dir(file_path, path);
        set_content(dir_listing);
        set_header("Content-Type", "text/html");
    }
    else
    {
        std::cout << "Directory access forbidden" << std::endl;
        set_code(403);
        set_content("<html><body><h1>403 Forbidden</h1><p>Directory access is forbidden.</p></body></html>");
        set_header("Content-Type", "text/html");
    }
}
```

### **Directory Listing Generation**:

```cpp
std::string Response::list_dir(const std::string &path, const std::string &request_path)
{
    std::stringstream html;
    html << "<html><body><h1>Directory list</h1><ul>";

    DIR *dir = opendir(path.c_str());
    if (dir == NULL)
    {
        html << "<li>Error: Could not open directory</li>";
        set_code(403);
    }
    else
    {
        struct dirent *item;
        while ((item = readdir(dir)) != NULL)
        {
            if (std::string(item->d_name) == "." || std::string(item->d_name) == "..")
                continue;

            std::string url;
            if (request_path[request_path.length() - 1] == '/')
                url = request_path + item->d_name;
            else
                url = request_path + "/" + item->d_name;

            if (item->d_type == DT_REG)
                html << "<li><a href=\"" << url << "\">" << item->d_name << "</a> File</li>";
            else if (item->d_type == DT_DIR)
                html << "<li><a href=\"" << url << "/\">" << item->d_name << "/</a> Directory</li>";
        }
        closedir(dir);
        set_code(200);
    }

    html << "</ul></body></html>";
    return html.str();
}
```

### 🔧 **Function Deep Dive: opendir() and readdir()**

```cpp
#include <dirent.h>
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
```

**opendir()**: Opens a directory stream
**readdir()**: Reads next entry from directory stream
**closedir()**: Closes directory stream

**struct dirent members**:

- `d_name`: Filename
- `d_type`: File type (DT_REG for file, DT_DIR for directory)

### **File Streaming Setup**:

```cpp
void Response::check_file(const std::string &file_path)
{
    std::ifstream file(file_path.c_str());
    if (file.is_open())
    {
        file.close();
        current_file_path = file_path;
        set_code(200);
    }
    else
    {
        set_code(403);
        set_content("<html><body><h1>403 Forbidden</h1><p>Access to this file is forbidden.</p></body></html>");
        set_header("Content-Type", "text/html");
        std::cout << "File exists but cannot be opened (403 Forbidden)" << std::endl;
    }
}
```

### **STEP B4: File Streaming** - [`response/response.cpp:80-120`](response/response.cpp#L80-L120)

```cpp
void Response::start_file_streaming(int client_fd)
{
    struct stat file_stat;
    if (stat(current_file_path.c_str(), &file_stat) != 0)
    {
        std::cout << "Error: Cannot stat file" << std::endl;
        set_code(500);
        return;
    }

    std::cout << "File size: " << file_stat.st_size << " bytes" << std::endl;

    // Build HTTP response headers
    std::stringstream response;
    response << "HTTP/1.0 200 OK\r\n";
    response << "Content-Type: " << mine_type.get_mime_type(current_file_path) << "\r\n";
    response << "Content-Length: " << file_stat.st_size << "\r\n";
    response << "Connection: close\r\n\r\n";

    std::string headers = response.str();
    std::cout << "Sending headers " << std::endl;
    send(client_fd, headers.c_str(), headers.length(), 0);

    // Open file for streaming
    file_stream = new std::ifstream(current_file_path.c_str(), std::ios::binary);
    if (!file_stream->is_open())
    {
        std::cout << "ERROR: Cannot open file for streaming: " << current_file_path << std::endl;
        delete file_stream;
        file_stream = NULL;
        set_code(500);
        current_file_path.clear();
        return;
    }

    std::cout << "File opened successfully for streaming" << std::endl;
    is_streaming_file = true;
}
```

### **Chunked File Transmission**:

```cpp
void Response::continue_file_streaming(int client_fd)
{
    if (!file_stream || !file_stream->is_open())
    {
        std::cout << "ERROR: File stream is not open - finishing streaming" << std::endl;
        finish_file_streaming();
        return;
    }

    std::cout << "Reading from file stream" << std::endl;
    file_stream->read(file_buffer, sizeof(file_buffer));
    std::streamsize bytes_read = file_stream->gcount();

    std::cout << "Read " << bytes_read << " bytes from file" << std::endl;

    if (bytes_read > 0)
    {
        ssize_t bytes_sent = send(client_fd, file_buffer, bytes_read, 0);
        std::cout << "Sent " << bytes_sent << " bytes to client" << std::endl;

        if (bytes_sent == -1)
        {
            std::cout << "Client disconnected" << std::endl;
            finish_file_streaming();
            return;
        }

        if (file_stream->eof())
        {
            std::cout << "File streaming completed" << std::endl;
            finish_file_streaming();
        }
    }
    else
    {
        finish_file_streaming();
    }
}
```

**File streaming benefits**:

- **Memory efficient**: Only loads small chunks at a time
- **Non-blocking**: Doesn't block server for large files
- **Resumable**: Can continue streaming across multiple epoll events

---

## 📤 **STEP 5: Response Transmission** - [`response/response.cpp:250-290`](response/response.cpp#L250-L290)

### **Simple Response Sending**:

```cpp
void Response::handle_response(int client_fd)
{
    std::cout << "-----------------RESPONSE---------------------" << std::endl;
    if (status_code == 200 && !current_file_path.empty())
    {
        // File streaming path
        if (!is_streaming_file)
        {
            std::cout << "Starting file streaming for: " << current_file_path << std::endl;
            start_file_streaming(client_fd);
        }
        else
        {
            std::cout << "Continuing file streaming..." << std::endl;
            continue_file_streaming(client_fd);
        }
    }
    else
    {
        // Simple response path
        std::stringstream status_stream;
        status_stream << status_code;
        std::string code_str = status_stream.str();
        std::string reason_phrase = what_reason(status_code);
        std::string status_line = "HTTP/1.0 " + code_str + " " + reason_phrase + "\r\n";
        std::string headers_line;

        // Build headers
        std::map<std::string, std::string>::iterator ite = headers.begin();
        while (ite != headers.end())
        {
            headers_line += ite->first + ": " + ite->second + "\r\n";
            ite++;
        }

        // Add Content-Length
        std::stringstream content_length;
        content_length << content.length();
        headers_line += "Content-Length: " + content_length.str() + "\r\n";
        headers_line += "\r\n";

        std::string full_response = status_line + headers_line + content;

        std::cout << "Sending response to client " << client_fd << std::endl;
        send(client_fd, full_response.c_str(), full_response.length(), 0);
    }
}
```

### 🔧 **Function Deep Dive: send() - Complete Technical Analysis**

```cpp
#include <sys/socket.h>
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

#### **What send() Actually Does at the Kernel Level**

**System Call Journey:**
```
User Space Application
        ↓ (system call)
Kernel Space
        ↓
Socket buffer space check
        ↓
Data copy from user to kernel
        ↓
TCP segmentation and queuing
        ↓
Network transmission scheduling
        ↓
Return bytes accepted
```

**Internal Kernel Process:**
1. **Socket Validation**: Verify sockfd points to valid, connected socket
2. **Buffer Space Check**: Check available space in socket's send buffer
3. **Data Copy**: Copy data from user space to kernel space send buffer
4. **TCP Processing**: TCP layer segments data, adds headers, calculates checksums
5. **Network Queuing**: Queue packets for network interface transmission
6. **Flow Control**: Respect receiver's advertised window size
7. **Return**: Return bytes accepted into kernel buffer (not necessarily transmitted)

#### **Critical Understanding: send() vs Actual Transmission**

**What send() Return Means:**
```cpp
ssize_t sent = send(client_fd, response_data.c_str(), response_data.size(), 0);
if (sent > 0) {
    // Data is in KERNEL BUFFER, not necessarily at client yet
    std::cout << "Accepted " << sent << " bytes into send buffer" << std::endl;
}
```

**TCP Send Buffer Mechanics:**
```
Application → send() → Kernel Send Buffer → TCP Transmission → Network → Client
     ↑                      ↑                    ↑              ↑         ↑
  Your data            Buffered here        Actually sent    On wire   Received
```

#### **Parameters Deep Technical Analysis**

**`int sockfd` - Socket File Descriptor:**
- **Validation**: Same as recv(), must be valid connected socket
- **State Requirements**: Socket must be in ESTABLISHED state for TCP
- **Error Conditions**: EBADF (bad fd), ENOTCONN (not connected), EPIPE (broken pipe)

**`const void *buf` - Data Buffer:**
- **Memory Access**: Kernel reads from user space memory
- **Page Faults**: Kernel handles if pages are swapped out
- **Const Correctness**: Data is read-only, won't be modified
- **Alignment**: No special alignment requirements

**`size_t len` - Data Length:**
- **Maximum Size**: Limited by socket send buffer size and TCP window
- **Partial Sends**: Common, especially for large data
- **Zero Length**: Valid, returns 0 immediately
- **Performance**: Larger sends more efficient (fewer system calls)

**`int flags` - Send Flags:**
- **`0`**: Normal operation (used in your webserver)
- **`MSG_DONTWAIT`**: Non-blocking send (don't wait if buffer full)
- **`MSG_NOSIGNAL`**: Don't generate SIGPIPE on broken pipe
- **`MSG_OOB`**: Send out-of-band (urgent) data
- **`MSG_MORE`**: More data coming (optimize for batching)

#### **Return Values Deep Analysis**

**Positive Number (> 0) - Partial or Complete Send:**
```cpp
std::string response = "HTTP/1.1 200 OK\r\n...";
ssize_t total_sent = 0;
size_t response_size = response.length();

while (total_sent < response_size) {
    ssize_t sent = send(client_fd, response.c_str() + total_sent, 
                       response_size - total_sent, 0);
    if (sent > 0) {
        total_sent += sent;
        std::cout << "Sent " << sent << " bytes, total: " << total_sent << std::endl;
    } else if (sent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Send buffer full, wait and retry
            continue;
        } else {
            perror("send failed");
            break;
        }
    }
}
```

**Zero (0) - No Data Sent:**
- **Rare with send()**: Unlike recv(), send() rarely returns 0
- **Possible Causes**: Zero-length send request
- **Not Connection Close**: Zero doesn't indicate connection closure for send()

**Negative (-1) - Error Conditions:**
```cpp
if (sent == -1) {
    switch (errno) {
        case EAGAIN:
        case EWOULDBLOCK:
            // Send buffer full (non-blocking socket)
            std::cout << "Send buffer full, retry later" << std::endl;
            break;
        case EPIPE:
            // Broken pipe - client disconnected
            std::cout << "Client disconnected (broken pipe)" << std::endl;
            break;
        case ECONNRESET:
            // Connection reset by peer
            std::cout << "Connection reset by peer" << std::endl;
            break;
        case EINTR:
            // Interrupted by signal
            continue; // Retry
        case EMSGSIZE:
            // Message too large
            std::cout << "Message too large for socket" << std::endl;
            break;
        default:
            perror("send failed");
    }
}
```

#### **TCP Send Buffer and Flow Control**

**Send Buffer Architecture:**
```
Application Data → Send Buffer → TCP Segments → Network Interface → Client
                      ↑              ↑              ↑
                 Kernel Space    Transmission    Physical Layer
```

**Buffer States:**
- **Available Space**: send() accepts data up to available buffer space
- **Buffer Full**: send() blocks (blocking) or returns EAGAIN (non-blocking)
- **Flow Control**: TCP respects receiver's advertised window size
- **Congestion Control**: TCP adjusts sending rate based on network conditions

**Why Partial Sends Occur:**
1. **Send Buffer Space**: Limited kernel buffer space available
2. **TCP Window**: Receiver's advertised window is small
3. **Network Congestion**: TCP congestion control limits transmission
4. **Signal Interruption**: System call interrupted by signal
5. **Non-blocking Mode**: Would block, returns with partial send

#### **SIGPIPE Signal Handling**

**What is SIGPIPE:**
```cpp
// When SIGPIPE occurs
send(broken_socket_fd, data, len, 0);  // Client already disconnected
// Result: SIGPIPE signal sent to process (can terminate program)
```

**Prevention Strategies:**
```cpp
// Method 1: Ignore SIGPIPE globally
signal(SIGPIPE, SIG_IGN);

// Method 2: Use MSG_NOSIGNAL flag
send(client_fd, data, len, MSG_NOSIGNAL);

// Method 3: Signal handler
void sigpipe_handler(int sig) {
    std::cout << "Broken pipe detected" << std::endl;
}
signal(SIGPIPE, sigpipe_handler);
```

#### **🎯 Potential Evaluator Questions & Expert Answers**

**Q: Why might send() return fewer bytes than requested?**
**A**: Technical reasons:
1. **Send Buffer Limitation**: Kernel send buffer has limited space
2. **TCP Flow Control**: Receiver's window size restricts transmission
3. **Network Congestion**: TCP congestion control limits sending rate
4. **Non-blocking Socket**: Would block, returns with partial send
5. **Signal Interruption**: System call interrupted (EINTR)
6. **Large Data**: System breaks large sends into smaller chunks

**Q: What happens to data after send() returns successfully?**
**A**: 
- **Kernel Buffer**: Data is copied to kernel's TCP send buffer
- **Not Transmitted Yet**: send() return doesn't mean data reached client
- **TCP Guarantees**: TCP will eventually deliver data or report error
- **Acknowledgment**: TCP waits for ACK from receiver
- **Retransmission**: TCP retransmits lost packets automatically
- **Application Continues**: Your program can continue immediately

**Q: How do you handle partial sends correctly?**
**A**: Always loop until all data is sent:
```cpp
ssize_t send_all(int sockfd, const void *buf, size_t len) {
    size_t total_sent = 0;
    const char *data = static_cast<const char*>(buf);
    
    while (total_sent < len) {
        ssize_t sent = send(sockfd, data + total_sent, len - total_sent, 0);
        if (sent > 0) {
            total_sent += sent;
        } else if (sent == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block, wait and retry
                usleep(1000); // Brief delay
                continue;
            } else {
                return -1; // Real error
            }
        }
    }
    return total_sent;
}
```

**Q: What's the difference between send() and write() for sockets?**
**A**: 
- **Functionality**: `send(fd, buf, len, 0)` equals `write(fd, buf, len)`
- **Flags Parameter**: send() has flags for socket-specific behavior
- **SIGPIPE Handling**: send() can use MSG_NOSIGNAL to avoid SIGPIPE
- **Socket-Specific**: send() designed specifically for network sockets
- **Error Codes**: send() provides socket-specific error information
- **Portability**: send() more portable for network programming

**Q: How does TCP congestion control affect send()?**
**A**: 
- **Slow Start**: Initial sends may be limited, then increase
- **Congestion Window**: TCP maintains cwnd limiting transmission rate
- **Network Feedback**: Packet loss reduces sending rate
- **RTT Impact**: Round-trip time affects transmission timing
- **Buffer Interaction**: Congestion control works with send buffer management

**Q: What happens when you send() to a closed socket?**
**A**: 
```cpp
// First send() after close
ssize_t result = send(closed_fd, data, len, 0);
// Returns -1 with errno = EPIPE
// SIGPIPE signal sent (unless ignored or MSG_NOSIGNAL used)

// Subsequent sends
// May return ECONNRESET or ENOTCONN depending on timing
```

**Q: How do you detect client disconnection during send()?**
**A**: Multiple detection methods:
```cpp
// 1. EPIPE error
if (sent == -1 && errno == EPIPE) {
    std::cout << "Client disconnected (broken pipe)" << std::endl;
}

// 2. ECONNRESET error  
if (sent == -1 && errno == ECONNRESET) {
    std::cout << "Connection reset by peer" << std::endl;
}

// 3. epoll notifications
if (events & (EPOLLHUP | EPOLLERR)) {
    std::cout << "Client disconnected (epoll)" << std::endl;
}

// 4. SIGPIPE signal (if not ignored)
void sigpipe_handler(int sig) {
    std::cout << "Broken pipe signal" << std::endl;
}
```

**Q: Why use large buffers vs multiple small sends?**
**A**: 
- **System Call Overhead**: Fewer system calls = better performance
- **TCP Efficiency**: Larger segments more efficient on network
- **Nagle's Algorithm**: TCP may delay small packets for efficiency
- **Buffer Management**: Kernel handles large buffers efficiently
- **Your Implementation**: Single large response send is optimal

**Q: How does send() interact with TCP_NODELAY?**
**A**: 
```cpp
// Nagle's algorithm (default)
// TCP delays small packets to combine them
int nodelay = 1;
setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

// With TCP_NODELAY:
// - Small sends transmitted immediately
// - Lower latency, potentially higher bandwidth usage
// - Good for interactive applications
// - Your webserver: May benefit for small responses
```

---

## 🧹 **STEP 6: Connection Cleanup** - [`client/client.cpp:202-230`](client/client.cpp#L202-L230)

```cpp
void Client::cleanup_connection(int epoll_fd, std::map<int, Client> &active_clients)
{
    std::cout << "=== CLEANING UP CLIENT " << client_fd << " ===" << std::endl;

    // Display connection statistics
    std::cout << "✓ Client " << client_fd << " was connected for " << (time(NULL) - get_connect_time()) << " seconds" << std::endl;
    std::cout << "✓ Client " << client_fd << " made " << get_request_count() << " requests" << std::endl;

    // Remove from epoll monitoring
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
    {
        std::cout << "Warning: Failed to remove client " << client_fd << " from epoll" << std::endl;
    }
    else
    {
        std::cout << "✓ Client " << client_fd << " removed from epoll" << std::endl;
    }

    // Close the socket
    if (close(client_fd) == -1)
    {
        std::cout << "Warning: Failed to close client " << client_fd << " socket" << std::endl;
    }
    else
    {
        std::cout << "✓ Client " << client_fd << " socket closed" << std::endl;
    }

    // Remove from active clients map
    active_clients.erase(client_fd);
    std::cout << "✓ Client removed from active clients map" << std::endl;
}
```

### 🔧 **Function Deep Dive: close()**

```cpp
#include <unistd.h>
int close(int fd);
```

**What it does**: Closes a file descriptor
**Return Values**:

- `0`: Success
- `-1`: Error occurred

**What happens when closing a socket**:

1. **TCP FIN sent**: Initiates connection termination
2. **Buffers flushed**: Pending data is sent
3. **Resources freed**: File descriptor becomes available
4. **Peer notified**: Other end receives connection close

---

## 🔄 **Complete Request Flow Summary**

### **Regular File Request Flow**:

```
recv() → parse_headers() → match_location() → figure_out_http_method() →
analyze_request_and_set_response() → handle_response() → send() → cleanup_connection()
```

### **CGI Request Flow**:

```
recv() → parse_headers() → match_location() → is_cgi_request() → start_cgi_process() →
fork() → execve() → handle_cgi_output() → format_cgi_response() → send() → cleanup_connection()
```

---

## 🛡️ **Error Handling Throughout the Journey**

### **Network Errors**:

- **ECONNRESET**: Client disconnected unexpectedly
- **EPIPE**: Broken pipe (client closed connection)
- **EAGAIN**: Would block (non-blocking I/O)

### **HTTP Errors**:

- **400 Bad Request**: Malformed HTTP request
- **403 Forbidden**: File access denied
- **404 Not Found**: File doesn't exist
- **405 Method Not Allowed**: Unsupported HTTP method
- **411 Length Required**: Missing Content-Length
- **500 Internal Server Error**: Server-side error

### **CGI Errors**:

- **502 Bad Gateway**: CGI script failed
- **504 Gateway Timeout**: CGI script timed out

### **System Errors**:

- **EMFILE**: Too many open files
- **ENOMEM**: Out of memory
- **EACCES**: Permission denied

---

## 🔧 **Key System Functions Reference**

### **Network I/O**:

- `recv()`: Receive data from socket
- `send()`: Send data through socket
- `close()`: Close file descriptor

### **Process Management**:

- `fork()`: Create new process
- `execve()`: Execute program
- `waitpid()`: Wait for child process
- `kill()`: Send signal to process

### **File Operations**:

- `open()`: Open file
- `read()`: Read from file descriptor
- `write()`: Write to file descriptor
- `stat()`: Get file information
- `access()`: Check file accessibility

### **Directory Operations**:

- `opendir()`: Open directory
- `readdir()`: Read directory entry
- `closedir()`: Close directory
- `chdir()`: Change working directory

### **I/O Control**:

- `fcntl()`: File descriptor control
- `dup2()`: Duplicate file descriptor
- `pipe()`: Create pipe

### **Event Monitoring**:

- `epoll_create1()`: Create epoll instance
- `epoll_ctl()`: Control epoll monitoring
- `epoll_wait()`: Wait for events

---

## 🎯 **Performance and Security Considerations**

### **Performance Optimizations**:

- **Non-blocking I/O**: Prevents server blocking
- **File streaming**: Memory-efficient large file handling
- **Connection reuse**: Efficient resource management
- **Event-driven architecture**: Scales to many concurrent connections

### **Security Measures**:

- **Input validation**: Validate all HTTP input
- **Path sanitization**: Prevent directory traversal
- **Resource limits**: Timeout and size restrictions
- **Process isolation**: CGI runs in separate process
- **Error handling**: Graceful failure without information leakage

### **Resource Management**:

- **File descriptor cleanup**: Always close unused descriptors
- **Process cleanup**: Prevent zombie processes
- **Memory management**: Proper allocation and deallocation
- **Connection limits**: Handle resource exhaustion gracefully

## 🎯 **COMPREHENSIVE EVALUATOR QUESTIONS & EXPERT ANSWERS**

### **System Architecture Questions**

**Q: Explain the complete flow when a client sends "GET /script.py HTTP/1.1"**
**A**: 
1. **epoll_wait()** notifies server of incoming data on client socket
2. **recv()** reads HTTP request data into 7MB buffer
3. **Request parser** validates HTTP format, extracts method/path/headers
4. **Location matcher** finds best matching location block for "/script.py"
5. **CGI detector** checks if path ends with configured CGI extension
6. **fork()** creates child process for CGI execution
7. **pipe()** creates communication channels between server and CGI
8. **dup2()** redirects CGI stdin/stdout to pipes
9. **execve()** replaces child process with Python interpreter
10. **CGI script** executes, reads from stdin, writes to stdout
11. **Server** monitors CGI output via epoll on pipe file descriptor
12. **read()** collects CGI output, formats as HTTP response
13. **send()** transmits response to client
14. **waitpid()** reaps CGI process to prevent zombies
15. **close()** cleans up all file descriptors

**Q: How does your server handle 1000 concurrent connections?**
**A**: 
- **Non-blocking I/O**: All sockets set to O_NONBLOCK
- **epoll**: O(1) event notification for all connections
- **Single-threaded**: No thread synchronization overhead
- **Event-driven**: Only processes connections with actual events
- **Memory efficient**: 7MB buffer per connection, but only active when processing
- **CGI isolation**: Each CGI runs in separate process, can't affect others

**Q: What happens if a CGI script hangs indefinitely?**
**A**: 
```cpp
// 30-second timeout mechanism
time_t current_time = time(NULL);
if (current_time - cgi_process.start_time > 30) {
    kill(cgi_process.pid, SIGKILL);  // Force termination
    waitpid(cgi_process.pid, NULL, 0);  // Reap zombie
    // Send 504 Gateway Timeout to client
    response = "HTTP/1.1 504 Gateway Timeout\r\n...";
}
```

### **Memory Management Questions**

**Q: Why use a 7MB buffer for recv()?**
**A**: 
- **Large HTTP requests**: Handles file uploads, large POST data
- **Reduces system calls**: Fewer recv() calls for large requests
- **HTTP compliance**: Some requests can be very large
- **Performance**: Single large buffer more efficient than multiple small ones
- **Trade-off**: Memory usage vs system call overhead

**Q: How do you prevent memory leaks in C++98?**
**A**: 
- **RAII pattern**: Constructors acquire resources, destructors release them
- **Smart pointers**: Use auto_ptr (C++98) or manual management
- **Stack allocation**: Prefer stack over heap when possible
- **Explicit cleanup**: Always match new/delete, malloc/free
- **Exception safety**: Use try/catch blocks for cleanup

### **Network Programming Questions**

**Q: Explain TCP flow control and how it affects your server**
**A**: 
- **TCP Window**: Receiver advertises available buffer space
- **Backpressure**: When buffer full, TCP stops accepting data
- **send() behavior**: May return partial sends when window is small
- **recv() behavior**: May return less data than requested
- **Your handling**: Loop until all data sent/received

**Q: What's the difference between SIGPIPE and EPIPE?**
**A**: 
- **SIGPIPE**: Signal sent when writing to broken pipe (can terminate process)
- **EPIPE**: errno value returned by write/send to broken pipe
- **Prevention**: `signal(SIGPIPE, SIG_IGN)` or `MSG_NOSIGNAL` flag
- **Detection**: Check errno after failed send() call

### **Process Management Questions**

**Q: Why fork() instead of threads for CGI?**
**A**: 
1. **Process isolation**: CGI crash can't affect server
2. **Security**: Separate memory spaces prevent interference
3. **Resource limits**: Can set per-process limits
4. **Clean termination**: Can kill runaway processes
5. **CGI standard**: CGI/1.1 expects separate process
6. **Signal isolation**: CGI signals don't affect server

**Q: Explain copy-on-write and why it's important for fork()**
**A**: 
- **Memory sharing**: Parent and child initially share same physical pages
- **Lazy copying**: Pages copied only when written to
- **Performance**: fork() completes quickly regardless of process size
- **Memory efficiency**: Saves memory when child immediately calls exec()
- **Your CGI**: Perfect since child immediately executes Python

### **HTTP Protocol Questions**

**Q: How do you handle HTTP/1.1 persistent connections?**
**A**: 
- **Current implementation**: Uses "Connection: close" (HTTP/1.0 style)
- **HTTP/1.1 requirement**: Should support persistent connections
- **Keep-alive**: Would need to parse multiple requests per connection
- **Timeout handling**: Close idle connections after timeout
- **Pipeline support**: Handle multiple requests in pipeline

**Q: What HTTP status codes does your server generate?**
**A**: 
- **200 OK**: Successful GET/POST/DELETE
- **201 Created**: Successful POST with file creation
- **400 Bad Request**: Malformed HTTP request
- **403 Forbidden**: File access denied or directory listing disabled
- **404 Not Found**: File doesn't exist
- **405 Method Not Allowed**: Unsupported HTTP method
- **411 Length Required**: POST without Content-Length
- **500 Internal Server Error**: Server-side errors
- **502 Bad Gateway**: CGI script failure
- **504 Gateway Timeout**: CGI script timeout

### **Security Questions**

**Q: How do you prevent directory traversal attacks?**
**A**: 
```cpp
// Check for directory traversal patterns
if (requested_path.find("../") != std::string::npos || 
    requested_path.find("..\\") != std::string::npos) {
    return BAD_REQUEST;  // Reject malicious paths
}

// Validate path starts with /
if (requested_path[0] != '/') {
    return BAD_REQUEST;
}
```

**Q: What prevents buffer overflow attacks?**
**A**: 
- **Large buffer**: 7MB buffer handles most requests
- **Size checking**: Always check recv() return value
- **Null termination**: `buffer[bytes_received] = '\0'`
- **Bounds checking**: Never write beyond buffer limits
- **Input validation**: Validate all HTTP input

### **Performance Questions**

**Q: How would you optimize this server for high load?**
**A**: 
1. **Connection pooling**: Reuse connection objects
2. **Buffer pooling**: Reuse large buffers instead of allocating
3. **sendfile()**: Use zero-copy file transmission
4. **Multiple workers**: Fork multiple server processes
5. **CPU affinity**: Bind processes to specific CPU cores
6. **Kernel bypass**: Use DPDK for extreme performance

**Q: What are the bottlenecks in your current implementation?**
**A**: 
1. **Single-threaded**: Can't utilize multiple CPU cores
2. **Large buffers**: 7MB per connection uses significant memory
3. **File I/O**: Blocking file operations (should use aio)
4. **CGI overhead**: fork/exec overhead for each CGI request
5. **No caching**: No file or response caching

### **Debugging Questions**

**Q: How would you debug a connection that hangs?**
**A**: 
```bash
# Check server state
netstat -tlnp | grep :8080
ss -tlnp | grep :8080

# Monitor connections
netstat -an | grep :8080
lsof -p <server_pid>

# Trace system calls
strace -f -p <server_pid>

# Check for zombies
ps aux | grep '<defunct>'

# Monitor file descriptors
ls -la /proc/<pid>/fd/
```

**Q: How do you test the server under load?**
**A**: 
```bash
# Simple load test
ab -n 1000 -c 10 http://localhost:8080/

# More advanced testing
wrk -t12 -c400 -d30s http://localhost:8080/

# CGI testing
for i in {1..100}; do
    curl http://localhost:8080/script.py &
done
wait
```

This comprehensive guide covers every aspect of HTTP request processing in your webserver, from the initial `recv()` call to the final response transmission, including all system calls, error handling, and both regular file serving and CGI execution paths.

---

## 📍 **COMPLETE FILE LOCATION INDEX**

### **Core Server Files**

- **Main Event Loop**: [`Server_setup/server.cpp:25-130`](Server_setup/server.cpp#L25-L130)
- **Server Class Definition**: [`Server_setup/server.hpp`](Server_setup/server.hpp)
- **Socket Setup**: [`Server_setup/socket_setup.cpp`](Server_setup/socket_setup.cpp)

### **Client Handling**

- **Client Class**: [`client/client.hpp`](client/client.hpp)
- **New Connection Handler**: [`client/client.cpp:50-66`](client/client.cpp#L50-L66)
- **Data Input Handler**: [`client/client.cpp:67-150`](client/client.cpp#L67-L150)
- **Data Output Handler**: [`client/client.cpp:155-200`](client/client.cpp#L155-L200)
- **Connection Cleanup**: [`client/client.cpp:202-230`](client/client.cpp#L202-L230)

### **Request Processing**

- **Request Class**: [`request/request.hpp`](request/request.hpp)
- **Request Parser**: [`request/request.cpp:50-102`](request/request.cpp#L50-L102)
- **Header Validation**: [`request/request.cpp:103-125`](request/request.cpp#L103-L125)
- **Header Parsing**: [`request/request.cpp:127-175`](request/request.cpp#L127-L175)
- **Location Matching**: [`request/request.cpp:43-78`](request/request.cpp#L43-L78)
- **Method Routing**: [`request/request.cpp:177-242`](request/request.cpp#L177-L242)
- **CGI Detection**: [`request/request.cpp:244-256`](request/request.cpp#L244-L256)

### **CGI Implementation**

- **CGI Runner Class**: [`cgi/cgi_runner.hpp`](cgi/cgi_runner.hpp)
- **CGI Process Structures**: [`cgi/cgi_headers.hpp`](cgi/cgi_headers.hpp)
- **CGI Process Creation**: [`cgi/cgi_runner.cpp:67-140`](cgi/cgi_runner.cpp#L67-L140)
- **Environment Building**: [`cgi/cgi_runner.cpp:30-65`](cgi/cgi_runner.cpp#L30-L65)
- **CGI Output Handling**: [`cgi/cgi_runner.cpp:179-230`](cgi/cgi_runner.cpp#L179-L230)
- **Response Formatting**: [`cgi/cgi_runner.cpp:290-340`](cgi/cgi_runner.cpp#L290-L340)
- **Process Cleanup**: [`cgi/cgi_runner.cpp:260-285`](cgi/cgi_runner.cpp#L260-L285)

### **Response Generation**

- **Response Class**: [`response/response.hpp`](response/response.hpp)
- **Response Analysis**: [`response/response.cpp:150-200`](response/response.cpp#L150-L200)
- **File Streaming**: [`response/response.cpp:80-120`](response/response.cpp#L80-L120)
- **Directory Listing**: [`response/response.cpp:120-150`](response/response.cpp#L120-L150)
- **Response Transmission**: [`response/response.cpp:250-290`](response/response.cpp#L250-L290)
- **Error Responses**: [`response/response.cpp:40-80`](response/response.cpp#L40-L80)

### **Configuration**

- **Config Parser**: [`config/parser.hpp`](config/parser.hpp)
- **Server Context**: [`config/parser.hpp:20-40`](config/parser.hpp#L20-L40)
- **Location Context**: [`config/parser.hpp:40-60`](config/parser.hpp#L40-L60)

### **Utility Files**

- **Request Status Codes**: [`request/request_status.hpp`](request/request_status.hpp)
- **MIME Types**: [`utils/mime_types.hpp`](utils/mime_types.hpp)

### **Key Function Locations Quick Reference**

| Function       | File                      | Lines   | Purpose                    |
| -------------- | ------------------------- | ------- | -------------------------- |
| `recv()`       | `client/client.cpp`       | 72      | Receive client data        |
| `send()`       | `response/response.cpp`   | 280     | Send response data         |
| `fork()`       | `cgi/cgi_runner.cpp`      | 95      | Create CGI process         |
| `execve()`     | `cgi/cgi_runner.cpp`      | 135     | Execute CGI script         |
| `pipe()`       | `cgi/cgi_runner.cpp`      | 90      | Create communication pipes |
| `epoll_wait()` | `Server_setup/server.cpp` | 30      | Wait for events            |
| `epoll_ctl()`  | Multiple files            | Various | Control epoll monitoring   |
| `stat()`       | `response/response.cpp`   | 160     | Get file information       |
| `opendir()`    | `response/response.cpp`   | 130     | Open directory             |
| `access()`     | `cgi/cgi_runner.cpp`      | 75      | Check file accessibility   |

### **Error Handling Locations**

| Error Type            | File                    | Lines   | HTTP Code |
| --------------------- | ----------------------- | ------- | --------- |
| Bad Request           | `request/request.cpp`   | 65, 140 | 400       |
| Forbidden             | `response/response.cpp` | 50      | 403       |
| Not Found             | `response/response.cpp` | 55      | 404       |
| Method Not Allowed    | `request/request.cpp`   | 190     | 405       |
| Length Required       | `request/request.cpp`   | 100     | 411       |
| Internal Server Error | `response/response.cpp` | 70      | 500       |
| Bad Gateway           | `cgi/cgi_runner.cpp`    | 220     | 502       |
| Gateway Timeout       | `cgi/cgi_runner.cpp`    | 185     | 504       |

This index provides direct links to all the key components and functions discussed in the guide, making it easy to navigate the codebase during evaluation or debugging.
