// Copyright (c) 2008, Google Inc.
// Copyright (c) 2026, The ng-log contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Shinichiro Hamaji
#include "utilities.h"

#include <gtest/gtest.h>

#if defined(NGLOG_OS_WINDOWS) && defined(HAVE_GET_THREAD_DESCRIPTION)
#  include <windows.h>
#elif defined(HAVE_PTHREAD_GETNAME_NP)
#  include <pthread.h>
#endif

#include "internal/emscripten_console.h"
#include "ng-log/logging.h"
#include "testing_utilities.h"

#ifdef NGLOG_USE_GFLAGS
#  include <gflags/gflags.h>
using namespace GFLAGS_NAMESPACE;
#endif

using namespace nglog;

TEST(utilities, InitializeLoggingDeathTest) {
  ASSERT_DEATH(InitializeLogging("foobar"), "");
}

TEST(utilities, MakeLogFilename) {
  EXPECT_EQ(nglog::MakeLogFilename("/tmp/app[1].", "20260817-123456.42",
                                   ".foo+", true),
            "/tmp/app[1].20260817-123456.42.foo+");
  EXPECT_EQ(nglog::MakeLogFilename("/tmp/app[1].", "20260817-123456.42",
                                   ".foo+", false),
            "/tmp/app[1]..foo+");
}

TEST(utilities, MakeLogFilenameMatcher) {
  const std::regex timestamp_regex =
      nglog::tools::MakeLogFilenameMatcher("app[1].", ".foo+", true);
  EXPECT_TRUE(
      std::regex_match("app[1].20260817-123456.42.foo+", timestamp_regex));
  EXPECT_FALSE(
      std::regex_match("app11.20260817-123456.42.foo+", timestamp_regex));

  const std::regex non_timestamp_regex =
      nglog::tools::MakeLogFilenameMatcher("app[1].", ".foo+", false);
  EXPECT_TRUE(std::regex_match("app[1]..foo+", non_timestamp_regex));
  EXPECT_FALSE(
      std::regex_match("app[1].20260817-123456.42.foo+", non_timestamp_regex));
}

TEST(utilities, MakeLogFilenameMatcherRequiresBaseFilename) {
  const std::regex regex =
      nglog::tools::MakeLogFilenameMatcher("", ".foo+", true);
  EXPECT_FALSE(std::regex_match("20260817-123456.42.foo+", regex));
}

TEST(EmscriptenConsole, MapsSeverityToConsoleLevel) {
  EXPECT_EQ(internal::EmscriptenLogLevelForSeverity(NGLOG_INFO),
            internal::EmscriptenLogLevel::kOut);
  EXPECT_EQ(internal::EmscriptenLogLevelForSeverity(NGLOG_WARNING),
            internal::EmscriptenLogLevel::kWarn);
  EXPECT_EQ(internal::EmscriptenLogLevelForSeverity(NGLOG_ERROR),
            internal::EmscriptenLogLevel::kError);
  EXPECT_EQ(internal::EmscriptenLogLevelForSeverity(NGLOG_FATAL),
            internal::EmscriptenLogLevel::kDbg);
}

TEST(utilities, TrimTrailingCRLFRemovesTrailingNewlines) {
  EXPECT_EQ(nglog::TrimTrailingCRLF("message\r\n"), "message");
  EXPECT_EQ(nglog::TrimTrailingCRLF("message\n"), "message");
  EXPECT_EQ(nglog::TrimTrailingCRLF("message"), "message");
  EXPECT_EQ(nglog::TrimTrailingCRLF("\r\n"), "");
}

TEST(utilities, TrimTrailingCharacters) {
  constexpr char delimiters[] = {' ', '\t'};

  EXPECT_EQ(nglog::TrimTrailingCharacters("message \t", delimiters), "message");
  EXPECT_EQ(nglog::TrimTrailingCharacters("message \t", delimiters,
                                          sizeof(delimiters)),
            "message");
  EXPECT_EQ(nglog::TrimTrailingCharacters("message", " \t"), "message");
  EXPECT_EQ(nglog::TrimTrailingCharacters(" \t", delimiters), "");
}

#if defined(NGLOG_OS_WINDOWS) && defined(HAVE_GET_THREAD_DESCRIPTION)
TEST(utilities, SetThreadName) {
  constexpr char kThreadName[] = "TestThread";
  nglog::SetThreadName(kThreadName);

  PWSTR thread_name = nullptr;
  ASSERT_TRUE(
      SUCCEEDED(GetThreadDescription(GetCurrentThread(), &thread_name)));
  ASSERT_NE(thread_name, nullptr);
  EXPECT_STREQ(thread_name, L"TestThread");
  LocalFree(thread_name);
}
#elif defined(HAVE_PTHREAD_GETNAME_NP)
TEST(utilities, SetThreadName) {
  constexpr char kThreadName[] = "TestThread";
  nglog::SetThreadName(kThreadName);

  char thread_name[16];
  ASSERT_EQ(
      pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name)), 0);
  EXPECT_STREQ(thread_name, kThreadName);
}
#endif

namespace {
class CountingSink : public nglog::LogSink {
 public:
  void send(LogSeverity /*severity*/, const char* /*full_filename*/,
            const char* /*base_filename*/, int /*line*/,
            const LogMessageTime& /*time*/, const char* /*message*/,
            size_t /*message_len*/) override {
    ++count;
  }

  int count = 0;
};

template <typename Log>
int CountEmitted(int iterations, Log log) {
  CountingSink sink;
  nglog::AddLogSink(&sink);
  for (int i = 0; i < iterations; ++i) {
    log(i);
  }
  nglog::RemoveLogSink(&sink);
  return sink.count;
}
}  // namespace

// LOG_IF_EVERY_N used to compute "% n", so n == 0 divided by zero. It now
// counts the same way as LOG_EVERY_N.
TEST(LogEveryN, IfEveryNMatchesEveryN) {
  // Each macro keeps its counter in a static at its call site, so every
  // period gets its own pair of call sites.
  EXPECT_EQ(CountEmitted(7, [](int) { LOG_IF_EVERY_N(INFO, true, 1) << "x"; }),
            CountEmitted(7, [](int) { LOG_EVERY_N(INFO, 1) << "x"; }));
  EXPECT_EQ(CountEmitted(7, [](int) { LOG_IF_EVERY_N(INFO, true, 2) << "x"; }),
            CountEmitted(7, [](int) { LOG_EVERY_N(INFO, 2) << "x"; }));
  EXPECT_EQ(CountEmitted(7, [](int) { LOG_IF_EVERY_N(INFO, true, 3) << "x"; }),
            3);
}

TEST(LogEveryN, IfEveryNWithZeroPeriodDoesNotCrash) {
  const int every_n =
      CountEmitted(5, [](int) { LOG_EVERY_N(INFO, 0) << "every 0"; });
  const int if_every_n = CountEmitted(
      5, [](int) { LOG_IF_EVERY_N(INFO, true, 0) << "if every 0"; });
  EXPECT_EQ(if_every_n, every_n);
}

TEST(LogEveryN, IfEveryNCountsOnlyWhenConditionHolds) {
  // Iterations 0, 2, 4, 6 pass the condition; every second of those logs.
  EXPECT_EQ(
      CountEmitted(
          8, [](int i) { LOG_IF_EVERY_N(INFO, i % 2 == 0, 2) << "even"; }),
      2);
}

int main(int argc, char** argv) {
  InitializeLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);

  CHECK_EQ(RUN_ALL_TESTS(), 0);
}
