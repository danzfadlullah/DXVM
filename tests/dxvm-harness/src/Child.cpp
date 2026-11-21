#include "Child.h"
#include "Report.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <csignal>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <unistd.h>
#endif

namespace dxvmt {

  namespace {

    constexpr const char* kChildFlag = "--selftest-child";
    constexpr const char* kChildOut  = "--child-out";
    constexpr int         kTimeoutMs = 120000;

    /** Read the first result line the child printed, if any. */
    ChildOutcome readChildOutput(const std::string& path, int exitCode) {
      ChildOutcome outcome;
      outcome.exitCode = exitCode;

      FILE* f = std::fopen(path.c_str(), "rb");
      if (!f)
        return outcome;

      std::string text;
      char buf[512];
      size_t n;

      while ((n = std::fread(buf, 1u, sizeof(buf), f)) > 0u)
        text.append(buf, n);

      std::fclose(f);

      size_t start = 0;

      while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
          end = text.size();

        const std::string line = text.substr(start, end - start);

        static const struct { const char* prefix; SceneStatus status; } kinds[] = {
          { "PASS ",  SceneStatus::Pass  },
          { "FAIL ",  SceneStatus::Fail  },
          { "SKIP ",  SceneStatus::Skip  },
          { "CRASH ", SceneStatus::Crash },
        };

        for (const auto& k : kinds) {
          const size_t len = std::strlen(k.prefix);
          if (line.size() >= len && !line.compare(0u, len, k.prefix)) {
            std::string rest = line.substr(len);

            // Skip the padded name: names may contain spaces, so consume
            // tokens until a run of two or more spaces ends the name field.
            size_t pos = rest.find("  ");
            if (pos == std::string::npos) {
              const size_t sp = rest.find(' ');
              pos = (sp == std::string::npos) ? rest.size() : sp;
              outcome.result.detail = (sp == std::string::npos)
                ? std::string() : rest.substr(sp + 1u);
            } else {
              while (pos < rest.size() && rest[pos] == ' ')
                pos++;
              outcome.result.detail = rest.substr(pos);
            }

            outcome.produced = true;
            outcome.result.status = k.status;
            return outcome;
          }
        }

        start = end + 1u;
      }

      return outcome;
    }

  }

  int childTimeoutMs() {
    return kTimeoutMs;
  }

  ChildOutcome runSceneAsChild(const std::string& exePath, const std::string& sceneName) {
    const std::string outPath = exePath + ".scene-out.tmp";

#ifdef _WIN32

    // CreateProcessA runs no shell: "> file" would arrive as argv, not a
    // redirect. The child is told the path and freopen()s it itself.
    char cmd[2048];
    std::snprintf(cmd, sizeof(cmd), "\"%s\" %s %s %s \"%s\"",
      exePath.c_str(), kChildFlag, sceneName.c_str(), kChildOut, outPath.c_str());

    STARTUPINFOA si = { };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { };

    std::string mutableCmd = cmd;

    if (!CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
          CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
      ChildOutcome outcome;
      outcome.result = SceneResult::crash(": could not spawn child");
      return outcome;
    }

    const DWORD wait = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(kTimeoutMs));
    const bool timedOut = (wait == WAIT_TIMEOUT);

    if (timedOut)
      TerminateProcess(pi.hProcess, 0xDEADu);

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    ChildOutcome outcome = readChildOutput(outPath, static_cast<int>(code));
    std::remove(outPath.c_str());

    if (timedOut) {
      outcome.produced = false;
      outcome.result = SceneResult::crash(": timeout");
    } else if (!outcome.produced) {
      outcome.result = SceneResult::crash(": exit " + std::to_string(code));
    } else if (code != 0u) {
      // Printed a result then died anyway (teardown crash): never a PASS.
      outcome.result = SceneResult::crash(": exit " + std::to_string(code)
        + " after result");
    }

    return outcome;

#else

    const pid_t pid = fork();

    if (pid < 0) {
      ChildOutcome outcome;
      outcome.result = SceneResult::crash(": fork failed");
      return outcome;
    }

    if (pid == 0) {
      if (!std::freopen(outPath.c_str(), "wb", stdout))
        _exit(126);
      dup2(STDOUT_FILENO, STDERR_FILENO);

      execl(exePath.c_str(), exePath.c_str(), kChildFlag, sceneName.c_str(),
        kChildOut, outPath.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }

    int status = 0;
    bool timedOut = false;

    for (int waited = 0; waited < kTimeoutMs; waited += 50) {
      const pid_t r = waitpid(pid, &status, WNOHANG);
      if (r == pid || r < 0)
        break;
      usleep(50 * 1000);
      if (waited + 50 >= kTimeoutMs) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        timedOut = true;
        break;
      }
    }

    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);

    ChildOutcome outcome = readChildOutput(outPath, code);
    std::remove(outPath.c_str());

    if (timedOut) {
      outcome.produced = false;
      outcome.result = SceneResult::crash(": timeout");
    } else if (!outcome.produced) {
      outcome.result = SceneResult::crash(
        WIFSIGNALED(status)
          ? ": signal " + std::to_string(WTERMSIG(status))
          : ": exit " + std::to_string(code));
    } else if (code != 0) {
      outcome.result = SceneResult::crash(": exit " + std::to_string(code)
        + " after result");
    }

    return outcome;

#endif
  }

  std::vector<std::pair<std::string, std::string>> parseEnvLines(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> result;

    size_t start = 0;

    while (start <= text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string::npos)
        end = text.size();

      std::string line = text.substr(start, end - start);

      if (!line.empty() && line.back() == '\r')
        line.pop_back();

      const size_t first = line.find_first_not_of(" \t");

      if (first != std::string::npos && line[first] != '#') {
        const size_t eq = line.find('=', first);

        if (eq != std::string::npos && eq > first) {
          const size_t keyEnd = line.find_last_not_of(" \t", eq - 1u);
          result.emplace_back(
            line.substr(first, keyEnd - first + 1u),
            line.substr(eq + 1u));
        }
      }

      if (end == text.size())
        break;

      start = end + 1u;
    }

    return result;
  }

  void applyEnv(const std::vector<std::pair<std::string, std::string>>& pairs) {
    for (const auto& kv : pairs) {
#ifdef _WIN32
      _putenv_s(kv.first.c_str(), kv.second.c_str());
#else
      setenv(kv.first.c_str(), kv.second.c_str(), 1);
#endif
    }
  }

}