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
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <unistd.h>
#endif

namespace dxvmt {

  namespace {

    constexpr const char* kChildFlag = "--selftest-child";

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

      // Find the first line that is a result line.
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
            // Strip the kind, then the padded name, keeping the detail.
            std::string rest = line.substr(len);
            size_t nameEnd = rest.find(' ');
            while (nameEnd != std::string::npos
                && nameEnd + 1u < rest.size()
                && rest[nameEnd + 1u] == ' ')
              nameEnd = rest.find(' ', nameEnd + 1u);

            outcome.produced = true;
            outcome.result.status = k.status;
            outcome.result.detail = (nameEnd == std::string::npos)
              ? std::string()
              : rest.substr(nameEnd + 1u);
            return outcome;
          }
        }

        start = end + 1u;
      }

      return outcome;
    }

  }

  ChildOutcome runSceneAsChild(const std::string& exePath, const std::string& sceneName) {
    const std::string outPath = exePath + ".scene-out.tmp";

#ifdef _WIN32

    char cmd[2048];
    std::snprintf(cmd, sizeof(cmd), "\"%s\" %s %s > \"%s\" 2>&1",
      exePath.c_str(), kChildFlag, sceneName.c_str(), outPath.c_str());

    STARTUPINFOA si = { };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { };

    std::string mutableCmd = cmd;

    if (!CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
          CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
      ChildOutcome outcome;
      outcome.result = SceneResult::crash("could not spawn child");
      return outcome;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    ChildOutcome outcome = readChildOutput(outPath, static_cast<int>(code));
    std::remove(outPath.c_str());

    if (!outcome.produced) {
      outcome.result = SceneResult::crash("exit " + std::to_string(code));
    }

    return outcome;

#else

    const pid_t pid = fork();

    if (pid < 0) {
      ChildOutcome outcome;
      outcome.result = SceneResult::crash("fork failed");
      return outcome;
    }

    if (pid == 0) {
      // Child: send both streams to the temp file, then exec the same binary.
      if (!std::freopen(outPath.c_str(), "wb", stdout))
        _exit(126);
      dup2(STDOUT_FILENO, STDERR_FILENO);

      execl(exePath.c_str(), exePath.c_str(), kChildFlag, sceneName.c_str(),
        static_cast<char*>(nullptr));
      _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);

    ChildOutcome outcome = readChildOutput(outPath, code);
    std::remove(outPath.c_str());

    if (!outcome.produced) {
      outcome.result = SceneResult::crash(
        WIFSIGNALED(status)
          ? "signal " + std::to_string(WTERMSIG(status))
          : "exit " + std::to_string(code));
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