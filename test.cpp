#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>

int main() {
    char buffer[7] = {0};

    // Open for reading, set O_NONBLOCK
    int fd = open("output.txt", O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    // Try to read immediately
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "No data available yet (non-blocking read)" << std::endl;
        } else {
            perror("read");
        }
    } else if (n == 0) {
        std::cout << "End of file reached" << std::endl;
    } else {
        std::cout << "Read " << n << " bytes: [" << buffer << "]" << std::endl;
    }

    close(fd);
    return 0;
}

