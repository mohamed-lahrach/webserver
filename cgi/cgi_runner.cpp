// cgi_runner.cpp  (POSIX-only; fine for 42)
#include "cgi_runner.hpp"
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static void set_nonblock(int fd, bool nb) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return;
    if (nb) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    else    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

bool run_cgi(const CgiConfig& cfg,
             const std::map<std::string,std::string>& env,
             const std::string& stdin_payload,
             CgiResult& result)
{
    int inpipe[2], outpipe[2];
    if (pipe(inpipe) < 0) return false;
    if (pipe(outpipe) < 0) { close(inpipe[0]); close(inpipe[1]); return false; }

    pid_t pid = fork();
    if (pid < 0) {
        close(inpipe[0]); close(inpipe[1]);
        close(outpipe[0]); close(outpipe[1]);
        return false;
    }

    if (pid == 0) {
        // Child
        // stdin from inpipe[0], stdout to outpipe[1]
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        // Optional: stderr to stdout too for debugging
      //  dup2(outpipe[1], STDERR_FILENO);

        close(inpipe[1]); close(outpipe[0]);

        if (!cfg.working_dir.empty()) chdir(cfg.working_dir.c_str());

        // Build envp
        std::vector<std::string> envbuf;
        envbuf.reserve(env.size()+1);
        for (std::map<std::string,std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
            envbuf.push_back(it->first + "=" + it->second);
        }
        std::vector<char*> envp; envp.reserve(envbuf.size()+1);
        for (size_t i=0;i<envbuf.size();++i) envp.push_back(const_cast<char*>(envbuf[i].c_str()));
        envp.push_back(NULL);

        // Build argv
        std::vector<std::string> argv_str;
        if (!cfg.interpreter.empty())
            argv_str.push_back(cfg.interpreter);
        argv_str.push_back(cfg.script_path);

        std::vector<char*> argvv; argvv.reserve(argv_str.size()+1);
        for (size_t i=0;i<argv_str.size();++i) argvv.push_back(const_cast<char*>(argv_str[i].c_str()));
        argvv.push_back(NULL);

        if (!cfg.interpreter.empty())
            execve(cfg.interpreter.c_str(), &argvv[0], &envp[0]);
        else
            execve(cfg.script_path.c_str(), &argvv[0], &envp[0]);

        // If exec fails:
        _exit(127);
    }

    // Parent
    close(inpipe[0]);
    close(outpipe[1]);

    // Non-blocking I/O and timed select loop
    set_nonblock(inpipe[1], true);
    set_nonblock(outpipe[0], true);

    const char* wptr = stdin_payload.empty() ? NULL : stdin_payload.data();
    size_t      wlen = stdin_payload.size();
    std::string captured;

    int elapsed = 0;
    const int step = 50; // ms

    while (true) {
        fd_set rset, wset;
        FD_ZERO(&rset); FD_ZERO(&wset);
        int maxfd = -1;

        // read from child
        FD_SET(outpipe[0], &rset);
        if (outpipe[0] > maxfd) maxfd = outpipe[0];

        // write request body to child
        if (wlen > 0) {
            FD_SET(inpipe[1], &wset);
            if (inpipe[1] > maxfd) maxfd = inpipe[1];
        }

        struct timeval tv;
        tv.tv_sec = 0; tv.tv_usec = step * 1000;

        int n = select(maxfd+1, &rset, &wset, NULL, &tv);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (wlen > 0 && FD_ISSET(inpipe[1], &wset)) {
            ssize_t m = write(inpipe[1], wptr, wlen);
            if (m > 0) { wptr += m; wlen -= (size_t)m; }
            else if (m < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                // child closed stdin early; stop writing
                wlen = 0;
            }
            if (wlen == 0) { close(inpipe[1]); }
        }

        if (FD_ISSET(outpipe[0], &rset)) {
            char buf[4096];
            ssize_t m = read(outpipe[0], buf, sizeof(buf));
            if (m > 0) captured.append(buf, buf + m);
            else if (m == 0) {
                // EOF from child; break after waitpid
                // fall-through to wait below
            }
        }

        // Check child status in a non-blocking way
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            // child exited; we can finish reading (socket EOF will arrive soon)
            // small grace read loop:
            for (int i=0; i<5; ++i) {
                char buf[4096];
                ssize_t m = read(outpipe[0], buf, sizeof(buf));
                if (m <= 0) break;
                captured.append(buf, buf + m);
                usleep(1000);
            }
            break;
        }

        elapsed += step;
        if (cfg.timeout_ms > 0 && elapsed >= cfg.timeout_ms) {
            // timeout → kill child
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            break;
        }
    }

    close(outpipe[0]);

    // Parse headers/body
    parse_cgi_headers(captured, result);
    return true;
}
