// A rejected combination must abort in the native constructor, with its exact
// configuration guard. Never call an unsuccessful test a performance result.
#include "ft_test_fixture.h"
#include <csignal>
#include <string>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
int main(int argc, char **argv) {
  assert(argc == 6);
  const unsigned mode = std::stoul(argv[3]);
  struct rlimit zero = {0, 0}; assert(setrlimit(RLIMIT_CORE, &zero) == 0);
  int error_pipe[2]; assert(pipe(error_pipe) == 0);
  const auto pid = fork(); assert(pid >= 0);
  if (pid == 0) {
    close(error_pipe[0]); assert(dup2(error_pipe[1], STDERR_FILENO) >= 0);
    close(error_pipe[1]); fixture f(argv[1], argv[2], mode, 16);
    _exit(77);  // Native constructor incorrectly accepted the combination.
  }
  close(error_pipe[1]); std::string error; char bytes[1024]; ssize_t n;
  while ((n = read(error_pipe[0], bytes, sizeof(bytes))) > 0) error.append(bytes, n);
  close(error_pipe[0]); int status = 0; assert(waitpid(pid, &status, 0) == pid);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
  assert(error.find(argv[5]) != std::string::npos);
  printf("MS_NATIVE_GUARD_PASS|case=%s|signal=SIGABRT|message=%s\n", argv[4], argv[5]);
}
