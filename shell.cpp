#include "shell.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

//check whether ch is a valid character of var
int checkchar(char ch) {
  if ((ch <= 'z' && ch >= 'a') || (ch >= 'A' && ch <= 'Z') || (ch <= '9' && ch >= '0') ||
      (ch == '_')) {
    return 1;
  }
  else {
    return 0;
  }
}

//check wheter var is a valid name
int checkVar(std::string var) {
  for (size_t i = 0; i < var.length(); i++) {
    if (!checkchar(var[i])) {
      return 0;
    }
  }
  return 1;
}

//convert a vector of string to char**
char ** vector2char(std::vector<std::string> src) {
  char ** dest = (char **)malloc(sizeof(*dest) * (src.size() + 1));
  for (size_t i = 0; i < src.size(); ++i) {
    dest[i] = strdup(src[i].c_str());
  }
  dest[src.size()] = NULL;
  return dest;
}

int command::checkQ() {
  size_t place = name.find("\"");
  int cnt = 0;
  while (place != std::string::npos) {
    if (name[place - 1] == '\\') {
      int cnt_escape = 1;
      while (place - cnt_escape != 0 && name[place - cnt_escape] == '\\') {
        cnt_escape++;
      }
      cnt_escape--;
      if (cnt_escape % 2 != 0) {
        //It's a \".
        place = name.find("\"", place + 1);
        continue;
      }
    }
    cnt++;
    place = name.find("\"", place + 1);
  }
  if (cnt % 2 == 0)
    return 1;
  else
    return 0;
}
//This function is for redirection, check the file after the mark.
//Mark could be "<" and "2>". Check whether it has an argument for redirection,
//and check the same mark cannot show up twice.
std::string command::destFile(std::string mark) {
  if (name.find(mark) == std::string::npos) {
    return std::string();
  }
  size_t start = name.find_first_not_of(" ", name.find(mark) + mark.length());
  if (start == std::string::npos) {
    std::cerr << "No arguments for redirection" << std::endl;
    return std::string();
  }
  if (name.find(mark, start) != std::string::npos) {
    std::cerr << "Invalid redirection!" << std::endl;
    return std::string();
  }
  size_t end = start;
  while (end < name.length() && name[end] != '>' && name[end] != '<' &&
         name[end] != ' ') {
    end++;
  }
  if (end != name.length()) {
    return name.substr(start, end - start);
  }
  else {
    return name.substr(start);
  }
}

//Get the filename of ">". It needs special care, because it is the ending
//of another mark "2>".
std::string command::destOutfile() {
  std::string mark = ">";
  size_t markIndex = name.find(mark);
  if (markIndex == std::string::npos) {
    return std::string();
  }
  if (name[markIndex - 1] == '2') {
    markIndex = name.find(mark, markIndex + 1);
    if (markIndex == std::string::npos) {
      return std::string();
    }
  }
  size_t start = name.find_first_not_of(" ", markIndex + 1);
  if (start == std::string::npos) {
    std::cerr << "No arguments for redirection" << std::endl;
    return std::string();
  }
  size_t doublename = name.find(mark, start);
  if (doublename != std::string::npos && name[doublename - 1] != '2') {
    std::cerr << "Invalid redirection!" << std::endl;
    return std::string();
  }
  size_t end = start;
  while (end < name.length() && name[end] != '>' && name[end] != '<' &&
         name[end] != ' ') {
    end++;
  }
  if (end != name.length()) {
    return name.substr(start, end - start);
  }
  else {
    return name.substr(start);
  }
}

//This function is used to take care of escape sign \\.
std::string replace(std::string s, std::string src, std::string dest) {
  size_t start = 0;
  while (1) {
    start = s.find(src, start);
    if (start == std::string::npos) {
      break;
    }
    else {
      s.replace(start, src.length(), dest);
      ++start;
    }
  }
  return s;
}

/*Get the argv of execve().
Take care of:
1. with quatation marks not match;
2. commands start with space;
3. commands end with and without spaces;
4. assume that " cannot become the start of a command.
5. \" as argument
*/
char ** command::getArgv() {
  std::vector<std::string> vector_argv;
  size_t start = 0;
  size_t end = 0;
  char ** argv;
  if (!checkQ()) {
    std::cerr << "Quatations don't match" << std::endl;
    argc = -1;  // set argc to represent error in argv.
    return NULL;
  }
  start = name.find_first_not_of(" ", start);  //get the place of command name
  end = name.find(" ", start);                 //get the start place of the arguments
  if (end != std::string::npos) {
    vector_argv.push_back(name.substr(start, end - start));
  }
  else {
    vector_argv.push_back(name.substr(start));
    argv = vector2char(vector_argv);
    argc = 1;
    return argv;
  }
  start = end;
  std::string split;
  while (1) {
    start = name.find_first_not_of(" ", start);
    if (start == std::string::npos) {
      break;
    }
    //Take care of "arguments"
    if (name[start] == '"') {
      ++start;  //Get rid of "
      end = name.find('"', start);
      while (name[end - 1] == '\\' && (name[end - 2] != '\\')) {
        end = name.find('"', end + 1);
      }
      if (end == std::string::npos) {
        std::cerr << "Quatations don't match" << std::endl;
        argc = -1;  // set argc to represent error in argv.
        return NULL;
      }
    }
    else {
      end = name.find(' ', start + 1);
    }
    if (end == std::string::npos) {
      split = name.substr(start);
    }
    else {
      split = name.substr(start, end - start);
    }
    split = replace(split, "\\\\", "\\");
    split = replace(split, "\\\"", "\"");
    vector_argv.push_back(split);
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  argv = vector2char(vector_argv);
  argc = vector_argv.size();
  return argv;
}

//Check whether the filename of execve is valid.
//Need to search in the ECE551PATH environment and match.
std::string command::checkFilename(std::string src, char * env) {
  if (src.find("/") != std::string::npos) {
    std::ifstream f(src.c_str());
    if (f.fail()) {
      std::cerr << "Command " << src << " not found" << std::endl;
      return "";
    }
    else {
      f.close();
      return src;
    }
  }
  std::istringstream ss(env);
  std::string path;  //The env to search
  while (std::getline(ss, path, ':')) {
    DIR * dir = opendir(path.c_str());
    if (dir) {
      struct dirent * drnt = readdir(dir);
      while (drnt != NULL) {
        if (strcmp(drnt->d_name, src.c_str()) == 0) {
          std::string filestring = path + "/" + src;
          if (closedir(dir) != 0) {
            std::cerr << "Failed to close file" << std::endl;
            exit(EXIT_FAILURE);
          }
          return filestring;
        }
        drnt = readdir(dir);
      }
      if (closedir(dir) != 0) {
        std::cerr << "Failed to close file" << std::endl;
        exit(EXIT_FAILURE);
      }
    }
    else {
      std::cerr << "Can't open the directory" << std::endl;
      return "";
    }
  }
  std::cerr << "Command " << src << " not found" << std::endl;
  return "";
}

//Split the command and get the filename, and check whether it's valid.
char * command::getFilename(char * env) {
  char * filename = NULL;
  std::string src;  //The command we want to search
  size_t start = 0;
  start = name.find_first_not_of(" ", start);  //get the start place of command name
  //command is empty
  if (start == std::string::npos) {
    return NULL;
  }
  size_t end = name.find(" ", start);
  if (end == std::string::npos) {
    src = name.substr(start);
  }
  else {
    src = name.substr(start, end - start);
  }
  std::string file = checkFilename(src, env);
  if (file.length() != 0) {
    filename = strdup(file.c_str());
  }
  return filename;
}

void myshell::statusHandler(int status) {
  if (WIFEXITED(status)) {
    if (WEXITSTATUS(status) == 0) {
      std::cout << "Program was successful" << std::endl;
    }
    else {
      std::cout << "Program failed with code " << WEXITSTATUS(status) << std::endl;
    }
  }
  else if (WIFSIGNALED(status)) {
    std::cout << "Terminated by signal 11" << std::endl;
  }
}

//Simply use system function for the step1.
void basic_shell::execute(std::string name) {
  int status = system(name.c_str());
  statusHandler(status);
}

//Run the shell for step1.
void basic_shell::runShell() {
  std::string comm;
  while (1) {
    std::cout << "ffosh$ ";
    std::getline(std::cin, comm);
    if (!std::cin) {
      if (std::cin.eof()) {
        return;
      }
      else {
        std::cerr << "Cannot read from stdin!";
        exit(EXIT_FAILURE);
      }
    }
    if (comm == "exit") {
      break;
    }
    if (comm.length() == 0) {
      continue;
    }
    command temp(comm);
    execute(temp.getname());
  }
}

//Use fork(), execve(), waitpid() for step2.
void improved_shell::execute(char * filename, char ** argv) {
  pid_t cpid, w;
  int status;
  cpid = fork();
  if (cpid == -1) {
    perror("Fork");
    exit(EXIT_FAILURE);
  }
  if (cpid == 0) {
    extern char ** environ;
    execve(filename, argv, environ);
  }
  else {
    w = waitpid(cpid, &status, 0);
    if (w == -1) {
      perror("waitpid");
      exit(EXIT_FAILURE);
    }
    statusHandler(status);
  }
}

//Run the shell for step2.
void improved_shell::runShell() {
  std::string comm;
  while (1) {
    std::cout << "ffosh$ ";
    std::getline(std::cin, comm);
    if (!std::cin) {
      if (std::cin.eof()) {
        return;
      }
      else {
        std::cerr << "Cannot read from stdin!";
        exit(EXIT_FAILURE);
      }
    }
    if (comm == "exit") {
      break;
    }
    command temp(comm);
    char * filename = temp.getFilename(env);
    if (filename != NULL) {
      char ** argv = temp.getArgv();
      if (temp.argc == -1) {
        free(filename);
        continue;
      }
      execute(filename, argv);
      free(filename);
      for (int i = 0; i < temp.getArgc(); i++) {
        free(argv[i]);
      }
      free(argv);
    }
  }
}
//For step3, when we meet a $ sign we need to search it in environment, and replace it if it is a variable in the environment.
std::string advanced_shell::replaceVar(std::string input) {
  size_t startvar = input.find("$");
  while (startvar != std::string::npos) {
    int len = 1;
    while (startvar + len < input.length() && checkchar(input[startvar + len])) {
      std::map<std::string, std::string>::iterator it;
      std::string temp = input.substr(startvar + 1, len);
      it = localEnvp.find(temp);
      if (it != localEnvp.end()) {
        input.replace(startvar, len + 1, it->second);
        break;
      }
      len++;
    }
    startvar = input.find("$", startvar + len);
  }
  return input;
}

//Check whether the command is set, export, or rev, to take care of the environment variables.
int advanced_shell::checkenv(command commcheck) {
  std::string exe;
  std::string name = commcheck.getname();
  size_t start = name.find_first_not_of(" ", 0);
  if (start == std::string::npos) {
    return -1;
  }
  size_t end = name.find(" ", start);
  if (end != std::string::npos) {
    exe = name.substr(start, end - start);
  }
  else {
    exe = name.substr(start);
  }
  if (strcmp(exe.c_str(), "set") == 0) {
    setEnvp(name.substr(start));
    return 1;
  }
  if (strcmp(exe.c_str(), "export") == 0) {
    exportEnvp(name.substr(start));
    return 1;
  }
  if (strcmp(exe.c_str(), "rev") == 0) {
    revEnvp(name.substr(start));
    return 1;
  }
  if (strcmp(exe.c_str(), "cd") == 0) {
    char ** argv = commcheck.getArgv();
    changeDir(argv);
    for (int i = 0; i < commcheck.getArgc(); i++) {
      free(argv[i]);
    }
    free(argv);
    return 1;
  }

  return 0;
}

//Change the current directory. Take care of special commands like cd ~, and cd.
//Deal with invalid command, like too many arguments, and invalid path.
void advanced_shell::changeDir(char ** argv) {
  int change;
  if (argv[1] == NULL || (strcmp(argv[1], "~") == 0)) {
    change = chdir(getenv("HOME"));
  }
  else if (argv[2] != NULL) {
    std::cerr << "Too much arguments" << std::endl;
    return;
  }
  else {
    change = chdir(argv[1]);
  }
  if (change != 0) {
    std::cerr << "Cannot change dir" << std::endl;
  }
}

//split the command to match set var name command, and return <var, name> pair.
//Check whether the var is a valid name.
//Take care if there is no var, or no name in the command.
std::pair<std::string, std::string> advanced_shell::splitEnv(std::string name) {
  std::string var;
  std::string value;
  size_t var_start = name.find_first_not_of(" ", 3);
  if (var_start == std::string::npos) {
    std::cerr << "No var in set command" << std::endl;
    return std::make_pair("", "");
  }
  size_t var_end = name.find(" ", var_start);
  if (var_end == std::string::npos) {
    std::cerr << "No name for var in set command" << std::endl;
    return std::make_pair("", "");
  }
  var = name.substr(var_start, var_end - var_start);
  if (!checkVar(var)) {
    std::cerr << "Invalid var" << std::endl;
    return std::make_pair("", "");
  }

  size_t value_start = name.find_first_not_of(" ", var_end);
  if (value_start == std::string::npos) {
    std::cerr << "No name for var in set command" << std::endl;
    return std::make_pair("", "");
  }

  value = name.substr(value_start);
  return std::make_pair(var, value);
}

//For set, insert the <var, name> pair to the local environment.
//If it already exists, remove it first and insert the new one.
//Take special care of "ECE551PATH" variable, if it is changed,
//change it of the shell immediately.
void advanced_shell::setEnvp(std::string name) {
  std::pair<std::string, std::string> enVariable = splitEnv(name);
  if (enVariable.first.length() != 0) {
    std::map<std::string, std::string>::iterator it;
    it = localEnvp.find(enVariable.first);
    if (it != localEnvp.end()) {
      localEnvp.erase(it);
    }
    localEnvp.insert(enVariable);
  }
  if (enVariable.first == "ECE551PATH") {
    free(env);
    env = NULL;
    env = strdup(enVariable.second.c_str());
  }
}

//export var. If it is in the local environment, setenv and make it
//accessible to other programs. If not, error.
void advanced_shell::exportEnvp(std::string name) {
  std::string var;
  size_t var_start = name.find_first_not_of(" ", 6);
  if (var_start == std::string::npos) {
    std::cerr << "No var in export command" << std::endl;
    return;
  }
  size_t var_end = name.find(" ", var_start);
  if (var_end == std::string::npos) {
    var = name.substr(var_start);
  }
  else {
    if (name.find_first_not_of(" ", var_end) != std::string::npos) {
      std::cerr << "Too much arguments for export command" << std::endl;
    }
    var = name.substr(var_start, var_end - var_start);
  }
  std::map<std::string, std::string>::iterator it;
  it = localEnvp.find(var);
  if (it == localEnvp.end()) {
    std::cerr << "Didn't find the var" << std::endl;
    return;
  }
  else {
    setenv(var.c_str(), it->second.c_str(), 1);
  }
}

//Reverse the name for the var. Search the var first in the local environment.
//If found, reverse it in place. Then check it in the environ.
//If found, update it.
void advanced_shell::revEnvp(std::string name) {
  std::string var;
  size_t var_start = name.find_first_not_of(" ", 3);
  if (var_start == std::string::npos) {
    std::cerr << "No var in rev command" << std::endl;
    return;
  }
  size_t var_end = name.find(" ", var_start);
  if (var_end == std::string::npos) {
    var = name.substr(var_start);
  }
  else {
    if (name.find_first_not_of(" ", var_end) != std::string::npos) {
      std::cerr << "Too much arguments for rev command" << std::endl;
    }
    var = name.substr(var_start, var_end - var_start);
  }
  std::map<std::string, std::string>::iterator it;
  it = localEnvp.find(var);
  if (it == localEnvp.end()) {
    std::cerr << "Didn't find the var" << std::endl;
    return;
  }
  std::reverse(it->second.begin(), it->second.end());
  if (getenv(var.c_str()) != NULL) {
    setenv(var.c_str(), it->second.c_str(), 1);
  }
}

//Run the shell for step 3.
void advanced_shell::runShell() {
  std::string comm;
  while (1) {
    char * currPath = get_current_dir_name();
    std::cout << "ffosh:" << currPath << " $ ";
    free(currPath);
    currPath = NULL;
    std::getline(std::cin, comm);
    if (!std::cin) {
      if (std::cin.eof()) {
        return;
      }
      else {
        std::cerr << "Cannot read from stdin!";
        exit(EXIT_FAILURE);
      }
    }
    if (comm == "exit")
      break;
    comm = replaceVar(comm);
    command temp(comm);
    int check_result = checkenv(temp);
    if (check_result == 0) {
      char * filename = temp.getFilename(env);
      if (filename != NULL) {
        char ** argv = temp.getArgv();
        if (temp.argc == -1) {
          continue;
        }
        execute(filename, argv);
        free(filename);
        for (int i = 0; i < temp.getArgc(); i++) {
          free(argv[i]);
        }
        free(argv);
      }
    }
  }
}

//This is the execute function for the step4. But I only did the first step.
//Check whether it has redirection. If does, add it between fork() and execve().
void final_shell::execute(char * filename,
                          char ** argv,
                          std::string infile,
                          std::string outfile,
                          std::string errorfile) {
  pid_t cpid, w;
  int status;
  cpid = fork();
  if (cpid == -1) {
    perror("Fork");
    exit(EXIT_FAILURE);
  }
  if (cpid == 0) {
    if (infile.length() != 0) {
      int in = open(infile.c_str(), O_RDONLY);
      if (in < 0) {
        std::cerr << "Couldn't read the file " << infile << std::endl;
      }
      else {
        int dupReturn = dup2(in, 0);
        if (dupReturn == -1) {
          std::cerr << "Failed to redirect stdin" << std::endl;
          exit(EXIT_FAILURE);
        }
        close(in);
      }
    }

    if (outfile.length() != 0) {
      int out = open(outfile.c_str(), O_WRONLY | O_CREAT, S_IRWXU);
      if (out < 0) {
        std::cerr << "Couldn't open the file " << outfile.c_str() << std::endl;
      }
      else {
        int dupReturn = dup2(out, 1);
        if (dupReturn == -1) {
          std::cerr << "Failed to redirect stdout" << std::endl;
          exit(EXIT_FAILURE);
        }
        close(out);
      }
    }

    if (errorfile.length() != 0) {
      int erro = open(errorfile.c_str(), O_WRONLY | O_CREAT, S_IRWXU);
      if (erro < 0) {
        std::cerr << "Couldn't open the file" << std::endl;
      }
      else {
        int dupReturn = dup2(erro, 2);
        if (dupReturn == -1) {
          std::cerr << "Failed to redirect stdin" << std::endl;
          exit(EXIT_FAILURE);
        }
        close(erro);
      }
    }
    extern char ** environ;
    execve(filename, argv, environ);
  }
  else {
    w = waitpid(cpid, &status, 0);
    if (w == -1) {
      perror("waitpid");
      exit(EXIT_FAILURE);
    }
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == 0) {
        std::cout << "Program was successful" << std::endl;
      }
      else {
        std::cout << "Program failed with code " << WEXITSTATUS(status) << std::endl;
      }
    }
    else if (WIFSIGNALED(status)) {
      std::cout << "Terminated by signal 11" << std::endl;
    }
  }
}

//If the filename is NULL, skip this command;
//If the command set/export/rev environment variables, go to checkenv();
//If there is error in redirection, skip this command.
//If there is redirection, the filename and argv should be string before the first redirection sign.
void final_shell::runShell() {
  std::string comm;
  while (1) {
    char * currPath = get_current_dir_name();
    std::cout << "ffosh:" << currPath << " $ ";
    free(currPath);
    currPath = NULL;
    std::getline(std::cin, comm);
    if (!std::cin) {
      if (std::cin.eof()) {
        return;
      }
      else {
        std::cerr << "Cannot read from stdin!";
        exit(EXIT_FAILURE);
      }
    }
    if (comm == "exit") {
      break;
    }
    comm = replaceVar(comm);
    command temp(comm);
    int check_result = checkenv(temp);
    if (check_result == 0) {
      size_t inMark =
          (comm.find("<") == std::string::npos) ? comm.length() : comm.find("<");
      size_t outMark = comm.find(">");
      while (outMark != std::string::npos && comm[outMark - 1] == '2') {
        outMark = comm.find(">", outMark + 1);
      };
      outMark = (outMark == std::string::npos) ? comm.length() : outMark;
      size_t errorMark =
          (comm.find("2>") == std::string::npos) ? comm.length() : comm.find("2>");
      size_t endmark = inMark < outMark ? inMark : outMark;
      endmark = endmark < errorMark ? endmark : errorMark;
      std::string infile = "";
      std::string outfile = "";
      std::string errorfile = "";
      if (inMark != comm.length()) {
        infile = temp.destFile("<");
        if (infile.length() == 0) {
          continue;
        }
      }
      if (outMark != comm.length()) {
        outfile = temp.destOutfile();
        if (outfile.length() == 0) {
          continue;
        }
      }
      if (errorMark != comm.length()) {
        errorfile = temp.destFile("2>");
        if (errorfile.length() == 0) {
          continue;
        }
      }
      if (endmark != comm.length()) {
        temp = command(comm.substr(0, endmark));
      }
      char * filename = temp.getFilename(env);
      if (filename != NULL) {
        char ** argv = temp.getArgv();
        {
          if (temp.argc == -1) {
            free(filename);
            continue;
          }
        }
        execute(filename, argv, infile, outfile, errorfile);
        free(filename);
        for (int i = 0; i < temp.getArgc(); i++) {
          free(argv[i]);
        }
        free(argv);
      }
    }
  }
}

int main() {
  final_shell shell;
  shell.runShell();
  return 0;
}
