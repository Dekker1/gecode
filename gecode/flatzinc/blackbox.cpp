/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *
 *  Copyright:
 *     Jip J. Dekker, 2024
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <gecode/flatzinc.hh>
#include <gecode/flatzinc/blackbox.hh>

#include <algorithm>
#include <cassert>
#include <string>

#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace Gecode {
namespace FlatZinc {

BlackBoxDLL::BlackBoxDLL(const std::string &name) {
  std::string loadError;
#ifdef _WIN32
  library = LoadLibrary(name.c_str());
  if (!library) {
    loadError = std::string("unable to locate library `") + name + "'";
    library = LoadLibrary((std::string(name) + ".dll").c_str());
  }
  if (!library) {
    library = LoadLibrary((std::string("lib") + name + ".dll").c_str());
  }
#else
  library = dlopen(name.c_str(), RTLD_LAZY);
  if (!library) {
    loadError = std::string(dlerror());
    library = dlopen((name + ".so").c_str(), RTLD_NOW);
  }
  if (!library) {
    library = dlopen((std::string("lib") + name + ".so").c_str(), RTLD_NOW);
  }
#endif
  if (!library) {
    throw Error("Blackbox", "Unable to open dynamic library: " + loadError);
  }

  // find symbol for blacbox function
#ifdef _WIN32
  *(void **)(&dll_fzn_blackbox) =
      GetProcAddress((HMODULE)library, "fzn_blackbox");
  std::string symError = ".";
#else
  *(void **)(&dll_fzn_blackbox) = dlsym(library, "fzn_blackbox");
  std::string symError(": ");
  if (!dll_fzn_blackbox) {
    symError += std::string(dlerror());
  }
#endif
  if (!dll_fzn_blackbox) {
    throw Error("Blackbox",
                "Unable to find symbol `fzn_blackbox` in dynamic library" +
                    symError);
  }
}

BlackBoxDLL::~BlackBoxDLL() {
  if (library) {
#ifdef _WIN32
    FreeLibrary((HMODULE)library);
#else
    dlclose(library);
#endif
  }
}

BlackBoxExec::BlackBoxExec(const std::string &program) {
#ifdef _WIN32
  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  HANDLE g_hChildStd_IN_Rd = NULL;
  HANDLE g_hChildStd_IN_Wr = NULL;
  HANDLE g_hChildStd_OUT_Rd = NULL;
  HANDLE g_hChildStd_OUT_Wr = NULL;

  // Create a pipe for the child process's STDOUT.
  if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
    std::cerr << "Stdout CreatePipe" << std::endl;
  // Ensure the read handle to the pipe for STDOUT is not inherited.
  if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
    std::cerr << "Stdout SetHandleInformation" << std::endl;

  // Create a pipe for the child process's STDIN
  if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
    std::cerr << "Stdin CreatePipe" << std::endl;
  // Ensure the write handle to the pipe for STDIN is not inherited.
  if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
    std::cerr << "Stdin SetHandleInformation" << std::endl;

  PROCESS_INFORMATION piProcInfo;
  STARTUPINFO siStartInfo;

  // Set up members of the PROCESS_INFORMATION structure.
  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

  // Set up members of the STARTUPINFO structure.
  // This structure specifies the STDIN and STDOUT handles for redirection.
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
  siStartInfo.cb = sizeof(STARTUPINFO);
  siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
  siStartInfo.hStdInput = g_hChildStd_IN_Rd;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  std::string prog = program;
  BOOL processStarted =
      CreateProcess(nullptr,
                    prog.data(),     // command line
                    nullptr,         // process security attributes
                    nullptr,         // primary thread security attributes
                    TRUE,            // handles are inherited
                    0,               // creation flags
                    nullptr,         // use parent's environment
                    nullptr,         // use parent's current directory
                    &siStartInfo,    // STARTUPINFO pointer
                    &piProcInfo);    // receives PROCESS_INFORMATION

  if (!processStarted) {
    throw Error("BlackBoxExec", "Unable to start program `" + program + "'");
  }

  CloseHandle(piProcInfo.hThread);
  // Stop ReadFile from blocking
  CloseHandle(g_hChildStd_OUT_Wr);
  // Just close the child's in pipe here
  CloseHandle(g_hChildStd_IN_Rd);

  pipe_send = g_hChildStd_IN_Wr;
  pipe_receive = g_hChildStd_OUT_Rd;
#else
  const int READ = 0;
  const int WRITE = 1;
  int child_in[2];
  int child_out[2];
  pipe(child_in);
  pipe(child_out);

  if (int childPID = fork()) {
    close(child_in[READ]);
    close(child_out[WRITE]);

    pipe_send = child_in[WRITE];
    pipe_receive = child_out[READ];
    return;
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  dup2(child_in[READ], STDIN_FILENO);
  dup2(child_out[WRITE], STDOUT_FILENO);
  close(child_in[WRITE]);
  close(child_out[READ]);

  int status = execlp(program.c_str(), program.c_str(),
                      (char *)nullptr); // execlp only returns if an error
  assert(status == -1);
  throw Error("BlackBoxExec", "Unable to start program `" + program + "'");
#endif
};

BlackBoxExec::~BlackBoxExec() {
#ifdef _WIN32
  CloseHandle(pipe_send);
  CloseHandle(pipe_receive);
#else
  close(pipe_send);
  close(pipe_receive);
#endif
}

void BlackBoxExec::run(const std::vector<int> &int_in,
                       const std::vector<double> &float_in,
                       std::vector<int> &int_out,
                       std::vector<double> &float_out) {
  // Construct program input
  std::stringstream out;
  for (int i : int_in) {
    out << i << " ";
  }
  out << "E ";
  for (int f : float_in) {
    out << f << " ";
  }
  out << "E\n";
  std::string out_buf = out.str();
#ifdef _WIN32
  // Write to process input pipe
  BOOL success =
      WriteFile(pipe_send, out_buf.c_str(), out_buf.size(), nullptr, nullptr);
  assert(success);

  // Read output from process by pipe
  char c[2] = {0, 0};
  std::string in_buffer;
  while (c[0] != '\n') {
    DWORD count = 0;
    BOOL success = ReadFile(pipe_receive, c, sizeof(c) - 1, &count, NULL);
    if (!success) {
      throw Error(
          "BlackBoxExec",
          "Reading blackbox process output from pipe resulted did not succeed");
    } else if (count == 0) {
      throw Error("BlackBoxExec",
                  "Blackbox process provided an incomplete response");
    }
    assert(count == 1);
    in_buffer += c[0];
  }
#else
  // Write to process input pipe
  ssize_t bytes_written = write(pipe_send, out_buf.c_str(), out_buf.size());
  assert(bytes_written = out_buf.size());

  // Read from process output pip
  char c = 0;
  std::string in_buffer;
  while (c != '\n') {
    ssize_t bytes = read(pipe_receive, &c, 1);
    if (bytes == -1) {
      throw Error(
          "BlackBoxExec",
          "Reading blackbox process output from pipe resulted in error no. " +
              std::to_string(errno));
    } else if (bytes == 0) {
      throw Error("BlackBoxExec",
                  "Blackbox process provided an incomplete response");
    }
    assert(bytes == 1);
    in_buffer += c;
  }
#endif
  // Parse given line
  std::istringstream ss(std::move(in_buffer));
  for (size_t i = 0; i < int_out.size(); ++i) {
    ss >> int_out[i];
    if (ss.fail()) {
      throw Error("BlackBoxExec", "Failed to read output integer " +
                                      std::to_string(i) +
                                      " from blackbox process output, " +
                                      std::to_string(int_out.size()) +
                                      " integer values where expected.");
    }
  }
  char E;
  ss >> E;
  assert(E == 'E');
  for (size_t i = 0; i < float_out.size(); ++i) {
    ss >> float_out[i];
    if (ss.fail()) {
      throw Error("BlackBoxExec", "Failed to read output float " +
                                      std::to_string(i) +
                                      " from blackbox process output, " +
                                      std::to_string(float_out.size()) +
                                      " floating point values where expected.");
    }
  }
  ss >> E;
  assert(E == 'E');
  std::string rem(ss.str().substr(ss.tellg()));
  assert(std::all_of(rem.begin(), rem.end(), [](char c) {
    return c == '\n' || c == ' ' || c == '\t' || c == '\r';
  }));
}

ExecStatus BlackBox::propagate(Space &home, const ModEventDelta &) {
  if (int_input.assigned()
#ifdef GECODE_HAS_FLOAT_VARS
      && float_input.assigned()
#endif
  ) {
    std::vector<int> int_in(int_input.size());
    std::vector<int> int_out(int_output.size());
    // std::cerr << "Black Box Fn input: ";
    for (int i = 0; i < int_in.size(); i++) {
      // std::cerr << int_input[i].val() << " ";
      int_in[i] = int_input[i].val();
    }
    std::vector<double> float_in(float_input.size());
    std::vector<double> float_out(float_output.size());
#ifdef GECODE_HAS_FLOAT_VARS
    for (int i = 0; i < float_in.size(); i++) {
      // std::cerr << float_input[i].val() << " ";
      float_in[i] = float_input[i].val().med();
    }
#endif
    // std::cerr << std::endl;

    black_box()->run(int_in, float_in, int_out, float_out);

    // std::cerr << "Black Box Fn output: ";
    for (int i = 0; i < int_out.size(); i++) {
      // std::cerr << int_output[i] << " ";
      GECODE_ME_CHECK(int_output[i].eq(home, int_out[i]));
    }
#ifdef GECODE_HAS_FLOAT_VARS
    for (int i = 0; i < float_out.size(); i++) {
      // std::cerr << float_output[i] << " ";
      GECODE_ME_CHECK(float_output[i].eq(home, float_out[i]));
    }
#endif
    // std::cerr << std::endl;

    return home.ES_SUBSUMED(*this);
  }
  return ES_FIX;
}

void blackbox(Home home, const IntVarArgs &int_in, const IntVarArgs &int_out,
#ifdef GECODE_HAS_FLOAT_VARS
              const FloatVarArgs &float_in, const FloatVarArgs &float_out,
#endif
              const std::string &mode, const std::string &instantiation) {
  ViewArray<Int::IntView> int_input(home, int_in);
  ViewArray<Int::IntView> int_output(home, int_out);
#ifdef GECODE_HAS_FLOAT_VARS
  ViewArray<Float::FloatView> float_input(home, float_in);
  ViewArray<Float::FloatView> float_output(home, float_out);
#endif

  if (home.failed())
    return;
  PostInfo pi(home);
  ExecStatus es = BlackBox::post(home, int_input, int_output,
#ifdef GECODE_HAS_FLOAT_VARS
                                 float_input, float_output,
#endif
                                 mode, instantiation);
  GECODE_ES_FAIL(es);
}

} // namespace FlatZinc
} // namespace Gecode
