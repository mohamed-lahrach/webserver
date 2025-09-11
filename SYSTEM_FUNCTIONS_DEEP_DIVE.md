# System Functions Deep Dive: Complete Technical Reference

## Overview

This document provides an in-depth explanation of every system function used in the webserver, how they work at the kernel level, and answers to potential evaluator questions. Each function is explained with its internal mechanics, common pitfalls, and real-world considerations.

---

## 🌐 **NETWORK I/O FUNCTIONS**

### **recv() - Receive Data from Socket**

```c
#include <sys/socket.h>
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

#### **What recv() Actually Does**

**At the Kernel Level:**
1. **System Call Entry**: User space calls `recv()`, kernel switches to kernel mode
2. **Socket Lookup**: Kernel finds the socket structure using `sockfd`
3. **Buffer Check**: Checks if data is available in the socket's receive buffer
4. **Data Copy**: Copies data from kernel space to user space buffer
5. **Return to User**: Returns number of bytes copied or error code

**Internal Process:**
```
User Space:     recv(fd, buffer, size, 0)
                     ↓
Kernel Space:   sys_recv() → sock_recvmsg() → tcp_recvmsg()
                     ↓
TCP Layer:      Check receive buffer → Copy data → Update pointers
                     ↓
User Space:     Return bytes received
```

#### **Parameters Deep Dive**

**`int sockfd`**:
- **What it is**: File descriptor representing the socket
- **How it works**: Index into the process's file descriptor table
- **Kernel lookup**: `fd → file* → socket* → sock*`
- **Validation**: Kernel checks if fd is valid and points to a socket

**`void *buf`**:
- **What it is**: Pointer to user space buffer
- **Memory safety**: Kernel validates the address is writable
- **Page faults**: If buffer is swapped out, kernel handles page fault
- **Alignment**: No special alignment requirements

**`size_t len`**:
- **What it is**: Maximum bytes to receive
- **Limits**: Cannot exceed socket buffer size or available data
- **Partial reads**: recv() may return less than requested
- **Zero length**: Valid, returns 0 immediately

**`int flags`**:
- **`0`**: Normal operation (most common)
- **`MSG_DONTWAIT`**: Non-blocking operation
- **`MSG_PEEK`**: Look at data without removing it
- **`MSG_WAITALL`**: Wait for full request or error

#### **Return Values Explained**

**Positive Number (> 0)**:
- **Meaning**: Number of bytes successfully received
- **Partial reads**: Common, especially with large requests
- **Buffer management**: Application must handle partial data

**Zero (0)**:
- **Meaning**: Peer closed connection gracefully
- **TCP FIN**: Other end sent FIN packet
- **Connection state**: Socket is now half-closed
- **Action required**: Close socket and clean up

**Negative (-1)**:
- **Meaning**: Error occurred, check `errno`
- **Non-blocking**: `EAGAIN`/`EWOULDBLOCK` means try again
- **Connection issues**: `ECONNRESET`, `ECONNABORTED`
- **System issues**: `ENOMEM`, `EINTR`

#### **Common errno Values**

```c
EAGAIN/EWOULDBLOCK  // No data available (non-blocking socket)
EBADF               // Invalid file descriptor
ECONNREFUSED        // Connection refused by peer
ECONNRESET          // Connection reset by peer
EFAULT              // Invalid buffer address
EINTR               // Interrupted by signal
EINVAL              // Invalid arguments
ENOMEM              // Insufficient memory
ENOTCONN            // Socket not connected
ENOTSOCK            // File descriptor is not a socket
```

#### **TCP Receive Buffer Mechanics**

**How TCP Buffering Works:**
```
Network → TCP Receive Buffer → recv() → Application Buffer
   ↓            ↓                ↓            ↓
Packets    Kernel Space    System Call   User Space
```

**Buffer States:**
- **Empty**: recv() blocks (blocking) or returns EAGAIN (non-blocking)
- **Partial**: recv() returns available data (may be less than requested)
- **Full**: recv() returns requested amount or buffer size

**Flow Control:**
- **TCP Window**: Advertises available buffer space to sender
- **Backpressure**: When buffer fills, TCP stops accepting data
- **Application reads**: Free buffer space, TCP can accept more data

#### **Evaluator Questions & Answers**

**Q: Why might recv() return fewer bytes than requested?**
A: Several reasons:
1. **TCP is stream-based**: No message boundaries, data arrives in chunks
2. **Buffer availability**: Only partial data available in kernel buffer
3. **Signal interruption**: System call interrupted (EINTR)
4. **Non-blocking mode**: Would block, returns immediately with available data

**Q: What happens if the buffer is too small?**
A: Data is truncated to buffer size. Remaining data stays in kernel buffer for next recv() call. No data is lost unless you don't call recv() again.

**Q: How does recv() work with non-blocking sockets?**
A: Returns immediately with available data or EAGAIN if no data. Never blocks the calling thread.

**Q: What's the difference between recv() and read()?**
A: recv() is socket-specific with flags parameter. read() is generic for all file descriptors. recv() provides more control over socket behavior.

**Q: Can recv() be interrupted?**
A: Yes, by signals. Returns -1 with errno=EINTR. Application should retry the call.

---

### **send() - Send Data Through Socket**

```c
#include <sys/socket.h>
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

#### **What send() Actually Does**

**At the Kernel Level:**
1. **Buffer Check**: Checks available space in socket's send buffer
2. **Data Copy**: Copies data from user space to kernel space
3. **TCP Processing**: TCP layer segments data, adds headers
4. **Network Transmission**: Data queued for network transmission
5. **Return**: Returns bytes accepted into kernel buffer

**Internal Process:**
```
User Space:     send(fd, data, size, 0)
                     ↓
Kernel Space:   sys_send() → sock_sendmsg() → tcp_sendmsg()
                     ↓
TCP Layer:      Check send buffer → Copy data → Queue for transmission
                     ↓
Network Layer:  IP routing → Network interface → Physical transmission
```

#### **Send Buffer Mechanics**

**TCP Send Buffer:**
- **Purpose**: Holds data until acknowledged by receiver
- **Size**: Configurable (SO_SNDBUF), typically 16KB-64KB
- **Flow control**: Receiver's window size limits transmission
- **Retransmission**: Buffer holds data for potential retransmission

**Partial Sends:**
```c
char data[1000];
ssize_t sent = send(fd, data, 1000, 0);
if (sent < 1000) {
    // Only 'sent' bytes were accepted
    // Must send remaining data: data + sent, 1000 - sent
}
```

#### **Flags Parameter**

**`MSG_DONTWAIT`**: Non-blocking send
**`MSG_NOSIGNAL`**: Don't generate SIGPIPE on broken pipe
**`MSG_OOB`**: Send out-of-band data (urgent)
**`MSG_MORE`**: More data coming (optimize for batching)

#### **Evaluator Questions & Answers**

**Q: Why might send() return fewer bytes than requested?**
A: 
1. **Send buffer full**: Kernel buffer has limited space
2. **Flow control**: Receiver's window is small
3. **Non-blocking socket**: Would block, returns with partial send
4. **Signal interruption**: System call interrupted

**Q: What happens to data after send() returns?**
A: Data is in kernel buffer, being transmitted. Not guaranteed to be received until TCP ACK. Application can continue immediately.

**Q: How do you handle partial sends?**
A: Loop until all data is sent:
```c
ssize_t send_all(int fd, const char *data, size_t len) {
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(fd, data + total_sent, len - total_sent, 0);
        if (sent <= 0) return sent; // Error or connection closed
        total_sent += sent;
    }
    return total_sent;
}
```

**Q: What's SIGPIPE and how do you handle it?**
A: Signal sent when writing to broken pipe/socket. Can crash program if not handled. Solutions:
1. `signal(SIGPIPE, SIG_IGN)` - Ignore globally
2. `MSG_NOSIGNAL` flag - Ignore for this send
3. Signal handler - Custom handling

---

## 🔄 **PROCESS MANAGEMENT FUNCTIONS**

### **fork() - Create New Process**

```c
#include <unistd.h>
pid_t fork(void);
```

#### **What fork() Actually Does**

**At the Kernel Level:**
1. **Process Creation**: Creates new process control block (PCB)
2. **Memory Duplication**: Copies parent's memory space (copy-on-write)
3. **File Descriptor Duplication**: Copies all open file descriptors
4. **PID Assignment**: Assigns new process ID to child
5. **Scheduling**: Both processes become runnable

**Copy-on-Write (COW) Optimization:**
```
Before fork():
Parent Process: [Memory Pages] → Physical Memory

After fork():
Parent Process: [Memory Pages] ↘
                                 → Shared Physical Memory (read-only)
Child Process:  [Memory Pages] ↗

After write:
Parent Process: [Memory Pages] → Original Physical Memory
Child Process:  [Memory Pages] → New Physical Memory (copied)
```

#### **Return Values**

**In Parent Process**: Returns child's PID (positive number)
**In Child Process**: Returns 0
**On Error**: Returns -1, no child created

#### **What Gets Copied**

**Memory:**
- Code segment (shared, read-only)
- Data segment (copied)
- Heap (copied)
- Stack (copied)

**File Descriptors:**
- All open files, sockets, pipes
- Same file offset pointers
- Reference counts incremented

**Process Attributes:**
- Environment variables
- Current working directory
- Signal handlers
- Process group ID

**What's Different:**
- Process ID (PID)
- Parent process ID (PPID)
- Pending signals (child starts with none)
- File locks (not inherited)

#### **Evaluator Questions & Answers**

**Q: Why use fork() for CGI instead of threads?**
A:
1. **Process isolation**: CGI script can't crash server
2. **Security**: Separate memory spaces prevent interference
3. **Resource limits**: Can set limits per process
4. **Clean termination**: Can kill runaway processes
5. **CGI standard**: CGI/1.1 expects separate process

**Q: What's copy-on-write and why is it important?**
A: Memory pages are shared until written to, then copied. Saves memory and makes fork() fast. Without COW, fork() would be extremely expensive for large processes.

**Q: How do you prevent zombie processes?**
A: Always call wait()/waitpid() to reap child processes:
```c
// Method 1: Explicit waiting
pid_t pid = fork();
if (pid == 0) {
    // Child process
    execve(...);
} else {
    // Parent process
    int status;
    waitpid(pid, &status, 0); // Reap child
}

// Method 2: Signal handler
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
signal(SIGCHLD, sigchld_handler);
```

**Q: What happens if fork() fails?**
A: Returns -1, errno indicates reason:
- `EAGAIN`: Process limit reached
- `ENOMEM`: Insufficient memory
- `ENOSYS`: fork() not supported

---

### **execve() - Execute Program**

```c
#include <unistd.h>
int execve(const char *pathname, char *const argv[], char *const envp[]);
```

#### **What execve() Actually Does**

**At the Kernel Level:**
1. **Binary Loading**: Loads executable file into memory
2. **Memory Replacement**: Replaces current process image
3. **Stack Setup**: Sets up new stack with argv/envp
4. **Entry Point**: Jumps to program's entry point (_start)
5. **Never Returns**: On success, never returns to caller

**Process Image Replacement:**
```
Before execve():
Process: [Old Code] [Old Data] [Old Heap] [Old Stack]

After execve():
Process: [New Code] [New Data] [New Heap] [New Stack]
         (Same PID, different program)
```

#### **Parameters**

**`pathname`**: Path to executable file
- Must be executable file or script with shebang
- Kernel checks execute permissions
- Can be absolute or relative path

**`argv[]`**: Command line arguments
- NULL-terminated array of strings
- argv[0] traditionally program name
- Passed to main(int argc, char *argv[])

**`envp[]`**: Environment variables
- NULL-terminated array of "KEY=VALUE" strings
- Replaces current environment
- Available via environ global variable

#### **What Happens During execve()**

**File Loading:**
1. **ELF Parsing**: Reads executable format headers
2. **Memory Mapping**: Maps code/data sections into memory
3. **Dynamic Linking**: Resolves shared library dependencies
4. **Stack Initialization**: Sets up initial stack frame

**What's Preserved:**
- Process ID (PID)
- Parent process ID (PPID)
- Process group ID
- Open file descriptors (unless close-on-exec set)
- Current working directory
- Signal handlers (reset to default)

**What's Replaced:**
- All memory contents (code, data, heap, stack)
- Signal handlers
- Memory mappings

#### **Evaluator Questions & Answers**

**Q: Why does execve() never return on success?**
A: The entire process image is replaced. The calling code no longer exists in memory. Only returns on failure.

**Q: How do file descriptors behave across execve()?**
A: By default, they remain open. Use `fcntl(fd, F_SETFD, FD_CLOEXEC)` to close on exec.

**Q: What's the difference between exec family functions?**
A:
- `execve()`: Most basic, takes environment
- `execv()`: Uses current environment
- `execl()`: Takes arguments as separate parameters
- `execlp()`: Searches PATH for executable

**Q: How does the shebang (#!) work?**
A: Kernel recognizes shebang, extracts interpreter path, and executes:
```bash
#!/usr/bin/python3
# Becomes: execve("/usr/bin/python3", ["python3", "script.py"], envp)
```

---

### **waitpid() - Wait for Child Process**

```c
#include <sys/wait.h>
pid_t waitpid(pid_t pid, int *status, int options);
```

#### **What waitpid() Actually Does**

**At the Kernel Level:**
1. **Process Search**: Finds child process by PID
2. **State Check**: Checks if child has changed state
3. **Resource Cleanup**: Frees child's process control block
4. **Status Return**: Returns exit status information
5. **Zombie Prevention**: Prevents zombie process accumulation

**Process States:**
```
Running → Terminated → Zombie → Reaped (by waitpid)
   ↓         ↓          ↓         ↓
Active    Exit code   PCB kept   PCB freed
```

#### **Parameters**

**`pid`**:
- `> 0`: Wait for specific child PID
- `0`: Wait for any child in same process group
- `-1`: Wait for any child process
- `< -1`: Wait for any child in process group |pid|

**`status`**: Pointer to store exit status (can be NULL)

**`options`**:
- `0`: Block until child changes state
- `WNOHANG`: Return immediately if no child available
- `WUNTRACED`: Also return for stopped children
- `WCONTINUED`: Also return for continued children

#### **Status Macros**

```c
int status;
waitpid(pid, &status, 0);

if (WIFEXITED(status)) {
    int exit_code = WEXITSTATUS(status);
    printf("Child exited with code %d\n", exit_code);
}

if (WIFSIGNALED(status)) {
    int signal = WTERMSIG(status);
    printf("Child killed by signal %d\n", signal);
}
```

#### **Evaluator Questions & Answers**

**Q: What are zombie processes and why are they bad?**
A: Zombie processes are terminated processes whose exit status hasn't been read by parent. They consume process table entries and can exhaust system resources.

**Q: What happens if parent doesn't call waitpid()?**
A: Child becomes zombie until parent exits. Then init process (PID 1) adopts and reaps zombies.

**Q: How do you handle multiple children?**
A:
```c
// Reap all available children
while (waitpid(-1, NULL, WNOHANG) > 0);

// Or use signal handler
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
```

**Q: What's the difference between wait() and waitpid()?**
A: wait() waits for any child, waitpid() can wait for specific child and has options for non-blocking operation.

---

## 📁 **FILE I/O FUNCTIONS**

### **read() - Read from File Descriptor**

```c
#include <unistd.h>
ssize_t read(int fd, void *buf, size_t count);
```

#### **What read() Actually Does**

**At the Kernel Level:**
1. **File Descriptor Lookup**: Finds file structure from fd
2. **Permission Check**: Verifies read permission
3. **Data Transfer**: Copies data from kernel buffer to user buffer
4. **Offset Update**: Updates file position pointer
5. **Return**: Returns bytes read or error

**For Different File Types:**
- **Regular files**: Reads from disk/cache
- **Pipes**: Reads from pipe buffer
- **Sockets**: Same as recv() with flags=0
- **Devices**: Device-specific behavior

#### **Evaluator Questions & Answers**

**Q: What's the difference between read() and recv()?**
A: read() works on any file descriptor, recv() is socket-specific with additional flags. For sockets, `read(fd, buf, len)` equals `recv(fd, buf, len, 0)`.

**Q: How does read() work with pipes?**
A: Reads from pipe buffer. Blocks if empty (blocking mode) or returns EAGAIN (non-blocking). Returns 0 when write end is closed.

**Q: What happens when reading from a closed file descriptor?**
A: Returns -1 with errno=EBADF (Bad file descriptor).

---

### **write() - Write to File Descriptor**

```c
#include <unistd.h>
ssize_t write(int fd, const void *buf, size_t count);
```

#### **What write() Actually Does**

**At the Kernel Level:**
1. **Permission Check**: Verifies write permission
2. **Buffer Space Check**: Checks available buffer space
3. **Data Copy**: Copies from user space to kernel buffer
4. **Write Scheduling**: Schedules actual write operation
5. **Return**: Returns bytes accepted into buffer

#### **Evaluator Questions & Answers**

**Q: What's SIGPIPE and when does it occur?**
A: Signal sent when writing to broken pipe/socket. Occurs when:
- Writing to pipe with no readers
- Writing to closed socket
- Can terminate process if not handled

**Q: How do you handle partial writes?**
A: Same as send(), loop until all data written:
```c
ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        ssize_t result = write(fd, (char*)buf + written, count - written);
        if (result <= 0) return result;
        written += result;
    }
    return written;
}
```

---

### **close() - Close File Descriptor**

```c
#include <unistd.h>
int close(int fd);
```

#### **What close() Actually Does**

**At the Kernel Level:**
1. **Reference Count**: Decrements file reference count
2. **Descriptor Release**: Frees file descriptor number
3. **Resource Cleanup**: If last reference, frees file structure
4. **Network Cleanup**: For sockets, initiates connection termination
5. **Buffer Flush**: Flushes any pending writes

**For Sockets:**
- Sends TCP FIN packet (graceful close)
- Enters TIME_WAIT state
- Eventually frees socket resources

#### **Evaluator Questions & Answers**

**Q: What happens if you close() a file descriptor twice?**
A: Second close() returns -1 with errno=EBADF. First close() freed the descriptor.

**Q: Do you need to close() file descriptors before exit()?**
A: No, kernel automatically closes all file descriptors when process exits. But good practice for long-running programs.

**Q: What's the difference between close() and shutdown() for sockets?**
A: close() closes file descriptor, shutdown() closes socket connection. shutdown() allows half-close (close write but keep read open).

---

## 🔧 **FILE CONTROL FUNCTIONS**

### **fcntl() - File Control Operations**

```c
#include <fcntl.h>
int fcntl(int fd, int cmd, ...);
```

#### **What fcntl() Actually Does**

**At the Kernel Level:**
1. **Command Dispatch**: Routes to appropriate handler based on cmd
2. **File Structure Access**: Modifies file structure flags/properties
3. **Validation**: Checks permissions and validity
4. **Atomic Operations**: Ensures thread-safe flag modifications

#### **Common Commands**

**`F_GETFL`**: Get file descriptor flags
**`F_SETFL`**: Set file descriptor flags
**`F_GETFD`**: Get file descriptor flags (close-on-exec)
**`F_SETFD`**: Set file descriptor flags

#### **File Status Flags**

**`O_NONBLOCK`**: Non-blocking I/O
- Read/write operations return immediately if would block
- Essential for event-driven servers
- Returns EAGAIN/EWOULDBLOCK when would block

**`O_APPEND`**: Always append to end of file
**`O_SYNC`**: Synchronous writes (wait for disk)
**`O_ASYNC`**: Asynchronous I/O notifications

#### **Evaluator Questions & Answers**

**Q: Why is non-blocking I/O important for servers?**
A: Prevents one slow client from blocking the entire server. Allows handling multiple clients concurrently with single thread.

**Q: How do you make a socket non-blocking?**
A:
```c
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

**Q: What's the difference between file descriptor flags and file status flags?**
A: File descriptor flags (F_GETFD/F_SETFD) are per-descriptor (like FD_CLOEXEC). File status flags (F_GETFL/F_SETFL) are per-file (like O_NONBLOCK).

---

### **dup2() - Duplicate File Descriptor**

```c
#include <unistd.h>
int dup2(int oldfd, int newfd);
```

#### **What dup2() Actually Does**

**At the Kernel Level:**
1. **Close newfd**: If newfd is open, closes it first
2. **Duplicate Entry**: Makes newfd point to same file as oldfd
3. **Reference Count**: Increments file reference count
4. **Atomic Operation**: Ensures no race conditions

**File Descriptor Table:**
```
Before dup2(pipe_fd, STDIN_FILENO):
fd 0 (stdin)  → terminal
fd 3 (pipe)   → pipe

After dup2(pipe_fd, STDIN_FILENO):
fd 0 (stdin)  → pipe  (same as fd 3)
fd 3 (pipe)   → pipe
```

#### **Evaluator Questions & Answers**

**Q: Why use dup2() for CGI redirection?**
A: CGI scripts expect to read from stdin (fd 0) and write to stdout (fd 1). dup2() redirects these standard descriptors to pipes for communication with server.

**Q: What happens to the original file descriptor after dup2()?**
A: Both file descriptors point to the same file. Closing one doesn't affect the other until all references are closed.

**Q: Is dup2() atomic?**
A: Yes, the close-and-duplicate operation is atomic, preventing race conditions in multi-threaded programs.

---

## 🔍 **FILE SYSTEM FUNCTIONS**

### **stat() - Get File Status**

```c
#include <sys/stat.h>
int stat(const char *pathname, struct stat *statbuf);
```

#### **What stat() Actually Does**

**At the Kernel Level:**
1. **Path Resolution**: Resolves pathname to inode
2. **Inode Access**: Reads inode information from filesystem
3. **Structure Population**: Fills stat structure with file metadata
4. **Permission Check**: Verifies access permissions

#### **struct stat Members**

```c
struct stat {
    mode_t    st_mode;     // File type and permissions
    ino_t     st_ino;      // Inode number
    dev_t     st_dev;      // Device ID
    nlink_t   st_nlink;    // Number of hard links
    uid_t     st_uid;      // User ID of owner
    gid_t     st_gid;      // Group ID of owner
    off_t     st_size;     // File size in bytes
    time_t    st_atime;    // Last access time
    time_t    st_mtime;    // Last modification time
    time_t    st_ctime;    // Last status change time
    blksize_t st_blksize;  // Block size for I/O
    blkcnt_t  st_blocks;   // Number of 512B blocks allocated
};
```

#### **File Type Checking**

```c
struct stat st;
stat(path, &st);

if (S_ISREG(st.st_mode))  // Regular file
if (S_ISDIR(st.st_mode))  // Directory
if (S_ISLNK(st.st_mode))  // Symbolic link
if (S_ISFIFO(st.st_mode)) // Named pipe (FIFO)
if (S_ISSOCK(st.st_mode)) // Socket
```

#### **Evaluator Questions & Answers**

**Q: What's the difference between stat(), lstat(), and fstat()?**
A:
- `stat()`: Follows symbolic links
- `lstat()`: Doesn't follow symbolic links (returns link info)
- `fstat()`: Works on open file descriptor instead of path

**Q: How do you check file permissions?**
A:
```c
struct stat st;
stat(path, &st);
if (st.st_mode & S_IRUSR) // Owner read permission
if (st.st_mode & S_IWUSR) // Owner write permission
if (st.st_mode & S_IXUSR) // Owner execute permission
```

**Q: What happens if file doesn't exist?**
A: Returns -1 with errno=ENOENT (No such file or directory).

---

### **access() - Check File Accessibility**

```c
#include <unistd.h>
int access(const char *pathname, int mode);
```

#### **What access() Actually Does**

**At the Kernel Level:**
1. **Path Resolution**: Resolves pathname to inode
2. **Permission Check**: Checks requested permissions against file mode
3. **User/Group Check**: Verifies caller's uid/gid against file ownership
4. **Return Result**: Returns 0 if accessible, -1 if not

#### **Mode Constants**

**`F_OK`**: Test for existence
**`R_OK`**: Test for read permission
**`W_OK`**: Test for write permission
**`X_OK`**: Test for execute permission

#### **Evaluator Questions & Answers**

**Q: Why check access() before opening file?**
A: Provides early validation and better error messages. However, there's a race condition (TOCTOU - Time of Check, Time of Use).

**Q: What's the race condition with access()?**
A: File permissions can change between access() check and actual file operation. Better to attempt operation and handle errors.

**Q: How does access() handle symbolic links?**
A: Follows symbolic links and checks the target file's permissions.

---

## 📂 **DIRECTORY FUNCTIONS**

### **opendir() / readdir() / closedir()**

```c
#include <dirent.h>
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
```

#### **What These Functions Do**

**opendir():**
1. **Directory Open**: Opens directory for reading
2. **DIR Structure**: Allocates DIR structure for state tracking
3. **Position Initialize**: Sets read position to beginning

**readdir():**
1. **Entry Read**: Reads next directory entry
2. **Structure Fill**: Fills dirent structure with entry info
3. **Position Advance**: Advances to next entry

**closedir():**
1. **Resource Free**: Frees DIR structure and associated resources
2. **File Descriptor Close**: Closes underlying file descriptor

#### **struct dirent**

```c
struct dirent {
    ino_t d_ino;           // Inode number
    off_t d_off;           // Offset to next record
    unsigned short d_reclen; // Length of this record
    unsigned char d_type;   // File type
    char d_name[];         // Null-terminated filename
};
```

#### **File Types (d_type)**

**`DT_REG`**: Regular file
**`DT_DIR`**: Directory
**`DT_LNK`**: Symbolic link
**`DT_FIFO`**: Named pipe
**`DT_SOCK`**: Socket
**`DT_CHR`**: Character device
**`DT_BLK`**: Block device
**`DT_UNKNOWN`**: Unknown type

#### **Evaluator Questions & Answers**

**Q: Why use readdir() instead of parsing directory manually?**
A: Directory format is filesystem-specific. readdir() provides portable interface across different filesystems.

**Q: Is readdir() thread-safe?**
A: No, readdir() uses static buffer. Use readdir_r() for thread safety (though it's deprecated in favor of readdir() with proper locking).

**Q: How do you handle large directories efficiently?**
A: readdir() is already efficient, reading entries on-demand. For very large directories, consider using scandir() or implementing custom filtering.

---

## 🔄 **INTER-PROCESS COMMUNICATION**

### **pipe() - Create Pipe**

```c
#include <unistd.h>
int pipe(int pipefd[2]);
```

#### **What pipe() Actually Does**

**At the Kernel Level:**
1. **Buffer Allocation**: Allocates kernel buffer (typically 64KB)
2. **File Descriptor Creation**: Creates two file descriptors
3. **Pipe Structure**: Links descriptors to same pipe buffer
4. **Reference Setup**: Sets up read/write references

**Pipe Buffer:**
```
Write End (pipefd[1]) → [Kernel Buffer] → Read End (pipefd[0])
                         (FIFO Queue)
```

#### **Pipe Characteristics**

**Unidirectional**: Data flows one way only
**FIFO**: First In, First Out ordering
**Atomic Writes**: Small writes (< PIPE_BUF) are atomic
**Blocking**: Blocks when buffer full (write) or empty (read)
**Capacity**: Limited buffer size (typically 64KB)

#### **Pipe States**

**Normal Operation:**
- Write end open, read end open: Normal pipe operation
- Data written to write end appears at read end

**Write End Closed:**
- Read returns 0 (EOF) when buffer empty
- No more data can be written

**Read End Closed:**
- Write generates SIGPIPE signal
- Write returns -1 with errno=EPIPE

#### **Evaluator Questions & Answers**

**Q: Why use pipes for CGI communication?**
A: Pipes provide secure, efficient IPC between server and CGI process. Data doesn't go through filesystem, and each process can only access its end.

**Q: What happens if pipe buffer fills up?**
A: Write blocks until space available (blocking mode) or returns EAGAIN (non-blocking mode). Reader must consume data to free space.

**Q: How do you make pipes non-blocking?**
A:
```c
int pipefd[2];
pipe(pipefd);
fcntl(pipefd[0], F_SETFL, O_NONBLOCK); // Non-blocking read
fcntl(pipefd[1], F_SETFL, O_NONBLOCK); // Non-blocking write
```

**Q: What's the difference between pipe() and socketpair()?**
A: pipe() creates unidirectional pipe, socketpair() creates bidirectional socket pair. Pipes are simpler and more efficient for one-way communication.

---

## ⚡ **EVENT MONITORING (EPOLL)**

### **epoll_create1() - Create Epoll Instance**

```c
#include <sys/epoll.h>
int epoll_create1(int flags);
```

#### **What epoll_create1() Actually Does**

**At the Kernel Level:**
1. **Epoll Structure**: Allocates epoll file structure
2. **Red-Black Tree**: Creates RB-tree for file descriptor tracking
3. **Ready List**: Initializes ready event list
4. **File Descriptor**: Returns file descriptor for epoll instance

**Epoll Architecture:**
```
Epoll Instance
├── Red-Black Tree (All monitored FDs)
├── Ready List (FDs with events)
└── Wait Queue (Sleeping processes)
```

#### **Flags**

**`EPOLL_CLOEXEC`**: Close epoll fd on exec()
**`0`**: No special flags

#### **Evaluator Questions & Answers**

**Q: Why is epoll more efficient than select/poll?**
A:
- **O(1) performance**: epoll_wait() time doesn't increase with number of FDs
- **Event-driven**: Only returns FDs with actual events
- **No FD limit**: Can monitor unlimited file descriptors
- **Edge-triggered**: Can use edge-triggered mode for efficiency

**Q: What's the difference between epoll_create() and epoll_create1()?**
A: epoll_create1() is newer, supports flags parameter. epoll_create() size parameter is ignored in modern kernels.

---

### **epoll_ctl() - Control Epoll Instance**

```c
#include <sys/epoll.h>
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

#### **What epoll_ctl() Actually Does**

**At the Kernel Level:**
1. **Operation Dispatch**: Routes to add/modify/delete handler
2. **RB-Tree Update**: Updates red-black tree of monitored FDs
3. **Event Registration**: Registers callback for file descriptor events
4. **Memory Management**: Allocates/frees tracking structures

#### **Operations**

**`EPOLL_CTL_ADD`**: Add file descriptor to monitoring
**`EPOLL_CTL_MOD`**: Modify events for existing file descriptor
**`EPOLL_CTL_DEL`**: Remove file descriptor from monitoring

#### **struct epoll_event**

```c
struct epoll_event {
    uint32_t events;      // Event mask
    epoll_data_t data;    // User data
};

union epoll_data_t {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
};
```

#### **Event Types**

**`EPOLLIN`**: Data available for reading
**`EPOLLOUT`**: Ready for writing
**`EPOLLERR`**: Error condition
**`EPOLLHUP`**: Hang up (connection closed)
**`EPOLLET`**: Edge-triggered mode
**`EPOLLONESHOT`**: One-shot mode

#### **Evaluator Questions & Answers**

**Q: What's the difference between level-triggered and edge-triggered?**
A:
- **Level-triggered (default)**: Event fires while condition is true
- **Edge-triggered (EPOLLET)**: Event fires when condition changes
- **Edge-triggered requires**: Reading all available data in one go

**Q: When should you use EPOLLONESHOT?**
A: When multiple threads might handle the same FD. Prevents race conditions by automatically removing FD from monitoring after event.

**Q: How do you handle EPOLLHUP?**
A: Connection closed by peer. Clean up resources and remove from epoll:
```c
if (events & EPOLLHUP) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    // Clean up application state
}
```

---

### **epoll_wait() - Wait for Events**

```c
#include <sys/epoll.h>
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

#### **What epoll_wait() Actually Does**

**At the Kernel Level:**
1. **Ready Check**: Checks if any events are ready
2. **Sleep/Wake**: Sleeps if no events, wakes when events arrive
3. **Event Copy**: Copies ready events to user buffer
4. **Return Count**: Returns number of ready events

**Event Processing:**
```
File Descriptor Event → Kernel → Ready List → epoll_wait() → Application
```

#### **Parameters**

**`epfd`**: Epoll file descriptor
**`events`**: Array to store ready events
**`maxevents`**: Maximum events to return
**`timeout`**: Timeout in milliseconds (-1 = block forever, 0 = return immediately)

#### **Return Values**

**> 0**: Number of ready file descriptors
**0**: Timeout occurred (only if timeout > 0)
**-1**: Error occurred (check errno)

#### **Evaluator Questions & Answers**

**Q: How do you handle epoll_wait() errors?**
A:
```c
int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
if (nfds == -1) {
    if (errno == EINTR) {
        // Interrupted by signal, retry
        continue;
    } else {
        // Real error
        perror("epoll_wait");
        break;
    }
}
```

**Q: What's a good maxevents value?**
A: Typically 64-1024. Higher values reduce system call overhead but increase memory usage. Tune based on expected concurrent connections.

**Q: Should you use timeout with epoll_wait()?**
A: Usually -1 (block forever) for servers. Use timeout for periodic maintenance tasks or graceful shutdown.

---

## 🛡️ **SIGNAL HANDLING**

### **kill() - Send Signal to Process**

```c
#include <signal.h>
int kill(pid_t pid, int sig);
```

#### **What kill() Actually Does**

**At the Kernel Level:**
1. **Process Lookup**: Finds target process by PID
2. **Permission Check**: Verifies sender can signal target
3. **Signal Delivery**: Adds signal to target's pending signal mask
4. **Process Wake**: Wakes target process if sleeping

#### **Common Signals**

**`SIGTERM` (15)**: Termination request (can be handled)
**`SIGKILL` (9)**: Force termination (cannot be handled)
**`SIGPIPE` (13)**: Broken pipe
**`SIGCHLD` (17)**: Child process terminated
**`SIGUSR1/SIGUSR2`**: User-defined signals

#### **PID Values**

**> 0**: Send to specific process
**0**: Send to all processes in same process group
**-1**: Send to all processes (except init)
**< -1**: Send to all processes in process group |pid|

#### **Evaluator Questions & Answers**

**Q: Why use SIGKILL instead of SIGTERM for CGI timeout?**
A: SIGTERM can be ignored or handled by CGI script. SIGKILL cannot be ignored and guarantees process termination.

**Q: What happens if you kill() a non-existent process?**
A: Returns -1 with errno=ESRCH (No such process).

**Q: How do you handle SIGCHLD to prevent zombies?**
A:
```c
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Reap all available children
    }
}

signal(SIGCHLD, sigchld_handler);
// or
struct sigaction sa;
sa.sa_handler = sigchld_handler;
sigemptyset(&sa.sa_mask);
sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
sigaction(SIGCHLD, &sa, NULL);
```

---

## 🕒 **TIME FUNCTIONS**

### **time() - Get Current Time**

```c
#include <time.h>
time_t time(time_t *tloc);
```

#### **What time() Actually Does**

**At the Kernel Level:**
1. **Clock Read**: Reads system clock (usually TSC or HPET)
2. **Epoch Calculation**: Calculates seconds since Unix epoch (1970-01-01)
3. **Return Value**: Returns time as time_t (usually 64-bit integer)

#### **Evaluator Questions & Answers**

**Q: Why use time() for timeouts instead of more precise functions?**
A: Second precision is sufficient for CGI timeouts (30 seconds). time() is fast and doesn't require additional libraries.

**Q: What happens in 2038?**
A: 32-bit time_t overflows. Modern systems use 64-bit time_t, which won't overflow for billions of years.

**Q: How do you measure elapsed time?**
A:
```c
time_t start = time(NULL);
// ... do work ...
time_t elapsed = time(NULL) - start;
if (elapsed > TIMEOUT) {
    // Handle timeout
}
```

---

## 🔧 **SYSTEM CALL ERROR HANDLING**

### **errno - Error Reporting**

```c
#include <errno.h>
extern int errno;
```

#### **How errno Works**

**Thread-Local Storage**: Each thread has its own errno value
**Set by System Calls**: Only set on error, never cleared on success
**Preserved Across Calls**: Value persists until next error

#### **Common errno Values**

```c
EAGAIN/EWOULDBLOCK  // Resource temporarily unavailable
EBADF               // Bad file descriptor
ECONNRESET          // Connection reset by peer
EFAULT              // Bad address
EINTR               // Interrupted system call
EINVAL              // Invalid argument
ENOMEM              // Out of memory
ENOENT              // No such file or directory
EPERM               // Operation not permitted
EPIPE               // Broken pipe
```

#### **Error Handling Best Practices**

```c
// Always check return values
ssize_t result = recv(fd, buffer, size, 0);
if (result == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Non-blocking operation would block
        return NEED_MORE_DATA;
    } else if (errno == ECONNRESET) {
        // Connection reset by peer
        return CONNECTION_CLOSED;
    } else {
        // Other error
        perror("recv failed");
        return ERROR;
    }
}

// Use perror() for debugging
if (open(filename, O_RDONLY) == -1) {
    perror("open failed"); // Prints "open failed: No such file or directory"
}

// Use strerror() for custom messages
fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
```

#### **Evaluator Questions & Answers**

**Q: Why check errno only after system call fails?**
A: System calls only set errno on error. Success doesn't clear errno, so checking it after successful call gives stale value.

**Q: Is errno thread-safe?**
A: Yes, each thread has its own errno value. However, signal handlers can modify errno, so save/restore if needed.

**Q: What's the difference between perror() and strerror()?**
A: perror() prints to stderr with program name prefix. strerror() returns error string for custom formatting.

---

## 🎯 **PERFORMANCE CONSIDERATIONS**

### **System Call Overhead**

**Context Switching Cost:**
- User mode → Kernel mode transition
- Register save/restore
- Cache pollution
- TLB flushes

**Optimization Strategies:**
1. **Batch Operations**: Use vectored I/O (readv/writev)
2. **Reduce Calls**: Buffer data to minimize system calls
3. **Non-blocking I/O**: Avoid blocking on slow operations
4. **Event-driven**: Use epoll instead of polling

### **Memory Management**

**Buffer Sizing:**
- **Too small**: More system calls, higher overhead
- **Too large**: Memory waste, cache misses
- **Sweet spot**: Usually 4KB-64KB for network I/O

**Memory Allocation:**
- **Stack buffers**: Fast, limited size
- **Heap buffers**: Flexible, allocation overhead
- **Static buffers**: Fast, not thread-safe

### **Network Optimization**

**TCP_NODELAY**: Disable Nagle's algorithm for low latency
**SO_REUSEADDR**: Allow address reuse for server restart
**SO_KEEPALIVE**: Detect dead connections
**Buffer sizes**: Tune SO_SNDBUF/SO_RCVBUF for throughput

---

## 🔍 **DEBUGGING AND TROUBLESHOOTING**

### **Common Issues and Solutions**

**File Descriptor Leaks:**
```bash
# Check open file descriptors
lsof -p <pid>
ls -la /proc/<pid>/fd/

# Monitor FD usage
watch "ls /proc/<pid>/fd | wc -l"
```

**Memory Leaks:**
```bash
# Use valgrind
valgrind --leak-check=full ./webserver

# Monitor memory usage
ps aux | grep webserver
cat /proc/<pid>/status | grep Vm
```

**Network Issues:**
```bash
# Check listening sockets
netstat -tlnp | grep :8080
ss -tlnp | grep :8080

# Monitor connections
netstat -an | grep :8080
ss -an | grep :8080

# Check network traffic
tcpdump -i lo port 8080
```

**Process Issues:**
```bash
# Check zombie processes
ps aux | grep '<defunct>'

# Monitor process tree
pstree -p <pid>

# Check process limits
cat /proc/<pid>/limits
```

### **Debugging Tools**

**strace**: Trace system calls
```bash
strace -f -e trace=network ./webserver
strace -f -e trace=file ./webserver
```

**gdb**: Debug crashes
```bash
gdb ./webserver
(gdb) run
(gdb) bt  # backtrace on crash
```

**ltrace**: Trace library calls
```bash
ltrace ./webserver
```

This comprehensive guide covers all the system functions used in your webserver with deep technical explanations and answers to potential evaluator questions. Each function is explained from both user-space and kernel perspectives, with practical examples and troubleshooting information.