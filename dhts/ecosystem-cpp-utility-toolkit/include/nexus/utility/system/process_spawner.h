#pragma once

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <csignal>

namespace nexus::utility::system {

/// @brief Process spawner with output capture and timeout support.
///
/// Fixes the classic pipe deadlock by reading stdout and stderr simultaneously
/// using select(). Supports timeout, environment variables, working directory,
/// and stdin input.
class ProcessSpawner {
public:
    struct Options {
        std::vector<std::string> args;
        std::optional<std::string> stdin_input;
        std::optional<std::string> working_dir;
        std::vector<std::string> env;         // "KEY=VALUE" format
        std::optional<std::chrono::milliseconds> timeout;
        bool merge_stderr = false;            // Merge stderr into stdout
    };

    struct Result {
        int exit_code = -1;
        std::string stdout_output;
        std::string stderr_output;
        bool timed_out = false;
        bool exec_failed = false;
    };

    /// @brief Execute a command (no options).
    static Result execute(const std::string& command) {
        Options opts;
        return executeImpl(command, opts);
    }

    /// @brief Execute a command with options.
    static Result execute(const std::string& command, const Options& opts) {
        return executeImpl(command, opts);
    }

private:
    static Result executeImpl(const std::string& command, const Options& opts) {
        int stdout_pipe[2] = {-1, -1};
        int stderr_pipe[2] = {-1, -1};
        int stdin_pipe[2] = {-1, -1};

        if (pipe(stdout_pipe) == -1) {
            throw std::runtime_error("ProcessSpawner: failed to create stdout pipe");
        }
        if (!opts.merge_stderr && pipe(stderr_pipe) == -1) {
            close(stdout_pipe[0]); close(stdout_pipe[1]);
            throw std::runtime_error("ProcessSpawner: failed to create stderr pipe");
        }
        if (opts.stdin_input.has_value() && pipe(stdin_pipe) == -1) {
            close(stdout_pipe[0]); close(stdout_pipe[1]);
            if (!opts.merge_stderr) { close(stderr_pipe[0]); close(stderr_pipe[1]); }
            throw std::runtime_error("ProcessSpawner: failed to create stdin pipe");
        }

        pid_t pid = fork();
        if (pid == -1) {
            closePipes(stdout_pipe, stderr_pipe, stdin_pipe);
            throw std::runtime_error("ProcessSpawner: fork failed");
        }

        if (pid == 0) {
            // ── Child Process ──────────────────────────────────────────────
            setupChildPipes(stdout_pipe, stderr_pipe, stdin_pipe, opts);
            if (opts.working_dir.has_value()) {
                chdir(opts.working_dir->c_str());
            }

            std::vector<char*> argv = buildArgv(command, opts.args);
            if (!opts.env.empty()) {
                std::vector<char*> envp = buildEnvp(opts.env);
                execve(command.c_str(), argv.data(), envp.data());
            } else {
                execvp(command.c_str(), argv.data());
            }
            _exit(127);
        }

        // ── Parent Process ─────────────────────────────────────────────────
        close(stdout_pipe[1]);
        if (!opts.merge_stderr) close(stderr_pipe[1]);
        if (opts.stdin_input.has_value()) close(stdin_pipe[0]);

        // Write stdin if provided
        if (opts.stdin_input.has_value()) {
            writeAll(stdin_pipe[1], opts.stdin_input.value());
            close(stdin_pipe[1]);
        }

        // Read stdout and stderr simultaneously using select()
        Result result;
        auto deadline = opts.timeout.has_value()
                            ? std::chrono::steady_clock::now() + opts.timeout.value()
                            : std::chrono::steady_clock::time_point::max();

        bool stdout_eof = false;
        bool stderr_eof = opts.merge_stderr;

        while (!stdout_eof || !stderr_eof) {
            fd_set readfds;
            FD_ZERO(&readfds);
            int max_fd = -1;

            if (!stdout_eof) {
                FD_SET(stdout_pipe[0], &readfds);
                max_fd = stdout_pipe[0];
            }
            if (!stderr_eof && !opts.merge_stderr) {
                FD_SET(stderr_pipe[0], &readfds);
                if (stderr_pipe[0] > max_fd) max_fd = stderr_pipe[0];
            }

            auto now = std::chrono::steady_clock::now();
            struct timeval tv {};
            struct timeval* ptv = nullptr;

            if (opts.timeout.has_value()) {
                if (now >= deadline) {
                    result.timed_out = true;
                    kill(pid, SIGKILL);
                    break;
                }
                auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now);
                tv.tv_sec = static_cast<__time_t>(remaining.count() / 1000000);
                tv.tv_usec = static_cast<__suseconds_t>(remaining.count() % 1000000);
                ptv = &tv;
            }

            int ret = select(max_fd + 1, &readfds, nullptr, nullptr, ptv);
            if (ret < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (ret == 0) {
                // Timeout
                continue;
            }

            char buffer[4096];
            if (!stdout_eof && FD_ISSET(stdout_pipe[0], &readfds)) {
                ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
                if (n > 0) {
                    result.stdout_output.append(buffer, static_cast<size_t>(n));
                } else {
                    stdout_eof = true;
                }
            }
            if (!stderr_eof && FD_ISSET(stderr_pipe[0], &readfds)) {
                ssize_t n = read(stderr_pipe[0], buffer, sizeof(buffer));
                if (n > 0) {
                    result.stderr_output.append(buffer, static_cast<size_t>(n));
                } else {
                    stderr_eof = true;
                }
            }
        }

        close(stdout_pipe[0]);
        if (!opts.merge_stderr) close(stderr_pipe[0]);

        int status;
        waitpid(pid, &status, 0);

        if (!result.timed_out) {
            result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }

        return result;
    }

public:
    /// @brief Execute a shell command (no options).
    static Result executeShell(const std::string& command) {
        Options opts;
        return executeShell(command, opts);
    }

    /// @brief Execute a shell command with options.
    static Result executeShell(const std::string& command, const Options& opts) {
        Options shell_opts = opts;
        shell_opts.args = {"-c", command};
        return execute("/bin/sh", shell_opts);
    }

private:
    static void setupChildPipes(int stdout_pipe[2], int stderr_pipe[2],
                                 int stdin_pipe[2], const Options& opts) {
        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[1]);

        if (opts.merge_stderr) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        } else {
            close(stderr_pipe[0]);
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[1]);
        }

        if (opts.stdin_input.has_value()) {
            close(stdin_pipe[1]);
            dup2(stdin_pipe[0], STDIN_FILENO);
            close(stdin_pipe[0]);
        }
    }

    static std::vector<char*> buildArgv(const std::string& cmd,
                                         const std::vector<std::string>& args) {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(cmd.c_str()));
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        return argv;
    }

    static std::vector<char*> buildEnvp(const std::vector<std::string>& env) {
        std::vector<char*> envp;
        for (const auto& e : env) {
            envp.push_back(const_cast<char*>(e.c_str()));
        }
        envp.push_back(nullptr);
        return envp;
    }

    static void writeAll(int fd, const std::string& data) {
        size_t written = 0;
        while (written < data.size()) {
            ssize_t n = write(fd, data.data() + written, data.size() - written);
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            written += static_cast<size_t>(n);
        }
    }

    static void closePipes(int stdout_pipe[2], int stderr_pipe[2], int stdin_pipe[2]) {
        for (int fd : {stdout_pipe[0], stdout_pipe[1], stderr_pipe[0], stderr_pipe[1],
                        stdin_pipe[0], stdin_pipe[1]}) {
            if (fd >= 0) close(fd);
        }
    }
};

} // namespace nexus::utility::system
