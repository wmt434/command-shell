#ifndef __SHELL_H__
#define __SHELL_H__

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

//command class is to deal with input command, with private member
//name as the input command.
//argc is the number of argv in the command.
class command {
 private:
  std::string name;
  int argc;

 public:
  command() : name(""), argc(0) {}
  command(std::string s) : name(s), argc(0) {}
  char ** getArgv();
  char * getFilename(char * env);
  std::string checkFilename(std::string file, char * env);
  std::string destFile(std::string mark);
  std::string destOutfile();
  std::string getname() { return name; }
  int getArgc() { return argc; }
  int checkQ();
  friend class basic_shell;
  friend class improved_shell;
  friend class advanced_shell;
  friend class final_shell;
};

//parent class for all versions of shell.
class myshell {
 public:
  myshell() {}
  void statusHandler(int status);
  //virtual void execute() = 0;
  virtual void runShell() = 0;
  virtual ~myshell() {}
};

//for step 1
class basic_shell : public myshell {
 public:
  basic_shell() : myshell() {}
  void execute(std::string name);
  virtual void runShell();
  ~basic_shell() {}
};

//for step 2, env is the ECE551PATH environment variable to search
//Initialize env when the shell starts.
class improved_shell : public myshell {
 protected:
  char * env;

 public:
  improved_shell() : myshell(), env(NULL) { env = strdup(getenv("PATH")); }
  void execute(char * filename, char ** argv);
  virtual void runShell();
  ~improved_shell() { free(env); }
};

//for step 3, localEnvp is the local environment variables (and their names)
//After set, the variables are stored in localEnvp.
//After export, the variables are added to environ.
//Take special care of ECE551PATH, if it is changed in this process, env changes
//immediately.
class advanced_shell : public improved_shell {
 protected:
  std::map<std::string, std::string> localEnvp;

 public:
  advanced_shell() : improved_shell() {
    localEnvp.insert(std::make_pair("ECE551PATH", std::string(env)));
    setenv("ECE551PATH", env, 1);
  }
  int checkenv(command commcheck);
  void changeDir(char ** argv);
  std::pair<std::string, std::string> splitEnv(std::string name);
  void setEnvp(std::string name);
  void exportEnvp(std::string name);
  void revEnvp(std::string name);
  std::string replaceVar(std::string input);
  void runShell();
  void checkpath();
  ~advanced_shell() { localEnvp.clear(); }
};

//for step 4. I only worked on the first part of step 4.
class final_shell : public advanced_shell {
 public:
  void execute(char * filename,
               char ** argv,
               std::string infile,
               std::string outfile,
               std::string errorfile);
  void runShell();
  ~final_shell() {}
};

#endif
