/**
* Shell
* Operating Systems
* version many
*
* This is a fork and adaptation of https://gitlab.science.ru.nl/operatingsystems/assignment1/-/blob/master/project/shell.cpp
*/

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <signal.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h> // for open()
#include <fcntl.h>

#include <vector>

// thanks to https://stackoverflow.com/a/14256296/6934388
#define DEBUGMODE 0
#define DEBUGs(x) do { if (DEBUGMODE>0) { std::cerr << x << std::endl; } } while (0)
#define DEBUG(x)  do { if (DEBUGMODE>1) { std::cerr << x << std::endl; } } while (0)
#define DEBUG2(x) do { if (DEBUGMODE>1) { std::cerr << #x << ": " << x << std::endl; } } while (0)

// 🥚
#define SHELLTHEME 0

using namespace std;

// Command structure holds command and arguments
// e.g. with input  'tail -c 10', parts will be {"tail", "-c" , "10"}
struct Command
{
	vector<string> parts = {};
};

// Experssion structure holds:
// - names of input/output files
// - whether the expression should be executed in the background
// - vector of all commands 
//   e.g if input is 'ls -l | head', commands will be {Command, Command};
//   (expands to) {{"ls", "-l"}, {"head"}}
struct Expression
{
	vector<Command> commands;
	string inputFromFile;
	string outputToFile;
	bool background = false;
};

// int checked when changing directories.
const int CHANGED_DIR_FLAG = 65;

// Parses a string to form a vector of arguments. The seperator is a space char (' ').
vector<string> splitString(const string& str, char delimiter = ' ') {
	vector<string> retval;
	for (size_t pos = 0; pos < str.length();) {
		// look for the next space
		size_t found = str.find(delimiter, pos);
		// if no space was found, this is the last word
		if (found == string::npos)
		{
			retval.push_back(str.substr(pos));
			break;
		}
		// filter out consequetive spaces
		if (found != pos)
			retval.push_back(str.substr(pos, found - pos));
		pos = found + 1;
	}
	return retval;
}

// wrapper around the C execvp so it can be called with C++ strings (easier to work with)
// always start with the command itself
// always terminate with a NULL pointer
// DO NOT CHANGE THIS FUNCTION UNDER ANY CIRCUMSTANCE
int execvp(const vector<string>& args) {
	// build argument list
	const char** c_args = new const char* [args.size() + 1];
	for (size_t i = 0; i < args.size(); ++i) {
		c_args[i] = args[i].c_str();
	}
	c_args[args.size()] = nullptr;
	// replace current process with new process as specified
	int retval = ::execvp(c_args[0], const_cast<char**>(c_args));
	// if we got this far, there must be an error
	// in case of failure, clean up memory (this won't overwrite errno normally, but let's be sure)
	int err = errno;
	delete[] c_args;
	errno = err;
	return retval;
}

// Executes a command with arguments. In case of failure, returns error code.
// should not return as it runs execvp, which replaces the process
int executeCommand(const Command& cmd) {
	auto& parts = cmd.parts;
	if (parts.size() == 0)
		return EINVAL;

	// execute external commands
	int retval = execvp(parts);
	return retval;
}

// easter egg (#define SHELLTHEME 1) custom prompt coloring (warning: disgusting code)
void displayCustomPrompt(char* dir) {
	string user_path(dir);

	vector<string> directories = splitString(user_path, '/');
	int dir_amt = directories.size();
	int YELLOW = 33;
	int SEPCOLOR = YELLOW;
	int OMEGACOLOR = 35;
	int k;
	for (int i = 0; i < dir_amt; i++) {
		k = (i % 6) + 31;
		if (k == SEPCOLOR) {
			k++;
		}
		if (i < 2 || i >(dir_amt - 3)) {
			cout << "\e[" << k << "m" << directories[i];
		}
		else {
			cout << "\e[";
		}
		if (i == 0 || i == 1 || i == dir_amt - 3 || i == dir_amt - 2) {
			cout << "\e[" << SEPCOLOR << "m" << "->";
		}
	}
	cout << "\e[" << SEPCOLOR << "m Ω " << "\e[" << OMEGACOLOR << "m";
}

void displayStandardPrompt(char* dir) {
	if (dir) {
		cout << "\e[32m" << dir << "\e[39m";
	}
	cout << "$ ";
}

// shows a user prompt in the terminal showing the current working dir and a $
void displayPrompt() {
	char buffer[512];
	char* dir = getcwd(buffer, sizeof(buffer));

#if SHELLTHEME == 1
	displayCustomPrompt(dir);
#else 
	displayStandardPrompt(dir);
#endif

	flush(cout);
}

// gets the current input from the commandline
// (and shows prompt if showPrompt==True)
string requestCommandLine(bool showPrompt) {
	if (showPrompt) {
		displayPrompt();
	}
	string retval;
	getline(cin, retval);
	return retval;
}

// note: For such a simple shell, there is little need for a full blown parser (as in an LL or LR capable parser).
// Here, the user input can be parsed using the following approach.
// First, divide the input into the distinct commands (as they can be chained, separated by `|`).
// Next, these commands are parsed separately. The first command is checked for the `<` operator, and the last command for the `>` operator.
Expression parseCommandLine(string commandLine) {
	Expression expression;
	vector<string> commands = splitString(commandLine, '|');
	for (size_t i = 0; i < commands.size(); ++i) {
		string& line = commands[i];
		vector<string> args = splitString(line, ' ');
		if (i == commands.size() - 1 && args.size() > 1 && args[args.size() - 1] == "&") {
			expression.background = true;
			args.resize(args.size() - 1);
		}
		if (i == commands.size() - 1 && args.size() > 2 && args[args.size() - 2] == ">") {
			expression.outputToFile = args[args.size() - 1];
			args.resize(args.size() - 2);
		}
		if (i == 0 && args.size() > 2 && args[args.size() - 2] == "<") {
			expression.inputFromFile = args[args.size() - 1];
			args.resize(args.size() - 2);
		}
		expression.commands.push_back({ args });
	}
	return expression;
}


// Change current directory to the users $HOME directory
int goHome() {
	char* home_dir;

	home_dir = getenv("HOME");
	if (home_dir != NULL) {
		if( chdir(home_dir) < 0){
			cerr << "Error when changing to home directory" <<endl;
			cerr << strerror(errno) << endl;
		}
		return CHANGED_DIR_FLAG;
	}
	else {
		cerr << "no $HOME directory found" << endl;
		return -1;
	}
}

// Handle a 'cd' command, only handles a single viable path.
int handleChangeDirectory(Command cmd) {
	// only cd, we chdir to $HOME.
	if (cmd.parts.size() == 1) {
		return goHome();
	}
	// we won't handle multiple arguments. Just a single directory.
	else if (cmd.parts.size() != 2) {
		cerr << "Wrong amount of arguments for cd " << endl;
		cerr << "Usage: cd <path>" << endl;
		return CHANGED_DIR_FLAG;
	}
	// ~ indicates user $HOME directory, for this shell. So go home.
	else if (cmd.parts.at(1).compare("~") == 0) {
		return goHome();
	}
	// last case, try to go to the specified directory.
	else if ((chdir(cmd.parts.at(1).c_str())) < 0) {
		cerr << "cd error:" << endl;
		cerr << strerror(errno) << endl;
	}
	return CHANGED_DIR_FLAG;
}


// Handle exit and chande dir.
int handleInternalCommands(Expression& expression) {
	for (const auto& command : expression.commands) {
		for (const auto& part : command.parts) {
			if (part.compare("exit") == 0) {
				cout << "Bye!" << endl;
				flush(cout);
				exit(0);
			}
		}
		if (command.parts[0].compare("cd") == 0) {
			return handleChangeDirectory(command);
		}
	}
	return 0;
}


// Execute an expression
// - check for inputfile, get/set corresponding input filedescriptor
// - create pipes to connect child processes from fork()
// - get outputfile file descriptors (in overwrite mode!) when needed
// - execute commands with execvp in a child process
// - wait for child process pids at the end if this is not a background expression.
int executeCommands(Expression& expression) {
	int AMT_COMMANDS = expression.commands.size();
	int LAST = AMT_COMMANDS - 1;

	int pipefd[2];
	int inputfd, outputfd;

	int FileInputModeFlag = O_RDONLY;
	int FileOutputModeFlag = O_WRONLY | O_CREAT | O_EXCL | O_TRUNC;

	mode_t writePermissions = 0644;

	pid_t cpid;
	pid_t cpids[AMT_COMMANDS];

	// If an input file is given, create a filedescriptor and set it as input
	if (expression.inputFromFile.empty() == 0) {
		if ((inputfd = open(expression.inputFromFile.c_str(), FileInputModeFlag)) < 0) {
			// handle errors
			cerr << "fail when opening filedescriptor for " << expression.inputFromFile.c_str() << endl;
			cerr << strerror(errno) << endl;
			return -1;
		}
		DEBUGs("Created inputfd " << inputfd << " for " << expression.inputFromFile.c_str());
	}
	// Otherwise, we set the first inputfd to 0
	else {
		inputfd = STDIN_FILENO;
	}

	for (int i = 0; i < AMT_COMMANDS; i++) {
		if (i != LAST) {
			// if there are more processes to be started, 
			// we create a pipe to redirect their I/Os
			if (pipe(pipefd) != 0) {
				cerr << "Failed to create pipe!\n";
				cerr << strerror(errno) << endl;
				abort();
			}
		}

		// create child process. 
		if ((cpid = fork()) < 0) {
			cerr << strerror(cpid) << "\nfork failed" << endl;
			cerr << strerror(errno) << endl;
			abort();
		}


		if (cpid == 0) {
			// child part of loop 
			DEBUG("cpid " << getpid() << " started with input: " << inputfd);
			if (inputfd != STDIN_FILENO) {
				// replace stdin of child process with the output from previous pipe
				// or in case of an inputfile, set that as the stdin.
				if (dup2(inputfd, STDIN_FILENO) < 0) {
					cerr << "dup2(inputfd, STDIN) failed: (inputfd :" << inputfd << ")" << endl;
					cerr << strerror(errno) << endl;
					abort();
				}
				// clear resources
				if (close(inputfd) < 0) {
					cerr << "fail when closing inputfd : " << inputfd << endl;
					cerr << strerror(errno) << endl;
					abort();
				}
			}

			if (i != LAST) {
				// (skips last child)
				// child processes should redirect their stdout
				// to the input end of the next pipe
				if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
					cerr << "dup2(fd[1], STDOUT) failed: (fd[1] :" << inputfd << ")" << endl;
					cerr << strerror(errno) << endl;
					abort();
				}
				if (close(pipefd[1]) < 0) {
					cerr << "fail when closing pipefd[1]: " << pipefd[1] << endl;
					cerr << strerror(errno) << endl;
					abort();
				}
			}
			// if an outputfile is given
			else if (i == LAST && (expression.outputToFile.empty() == 0)) {
				// open a fd for the output file in write or read/write mode
				// not sure if its the right option to do this here, instead of
				// above the whole for-loop
				// O_WRONLY | O_TRUNC | O_CREAT | O_EXCL
				if ((outputfd = open(expression.outputToFile.c_str(), FileOutputModeFlag, writePermissions)) < 0) {
					cerr << "opening file error for " << expression.outputToFile.c_str() << endl;
					cerr << strerror(errno) << endl; // errorcode should be better described with this!
					abort();
				}
				DEBUGs("Created outputfd: " << outputfd);
				if (dup2(outputfd, STDOUT_FILENO) < 0) {
					cerr << "dup2(outputfd, STDOUT) failed: (outputfd :" << outputfd << ")" << endl;
					cerr << strerror(errno) << endl;
					abort();
				}
				if (close(outputfd) < 0) {
					cerr << "fail when closing outputfd: " << outputfd << endl;
					cerr << strerror(errno) << endl;
					abort();
				}
			}

			// Execute the commands! This uses the c++ wrapper for execvp
			// we expect the child not to return from this, as the process
			// should be replaced by execvp.
			int errcode = executeCommand(expression.commands[i]);
			if (errcode != 0) {
				cerr << "Process (pid: " << getpid() << ") encountered a bad command: ";
				for (auto part : expression.commands[i].parts) {
					cerr << part << " ";
				}
				cerr << endl;
				cerr << strerror(errno) << endl;
			}
			abort();
		}
		// parent part of the loop
		else {

			// make the new input the output of the pipe we have
			if (i != LAST) {
				inputfd = pipefd[0];

				// close the write end of this pipe
				if (close(pipefd[1]) < 0) {
					cerr << i << " fail when closing fd in parent: " << pipefd[1] << endl;
					cerr << strerror(errno) << endl;
					abort();
				}
			}

			// administration
			// question: should/could be skipped if exp.background=true?
			cpids[i] = cpid;
		}
	}

	// wait for children to finish their processing
	// (skips if expression.background=true)
	if (!expression.background) {
		for (auto cpid : cpids) {
			DEBUG("waiting for pid: " << cpid);
			if (cpid != 0) {
				// when to use WNOHANG instead of NULL?
				if (waitpid(cpid, NULL, 0) < 0 ){
					cerr << "waitpid error for "<< cpid << endl;
					cerr << strerror(errno);
				}
			}
			else {
				cerr << "Encountered pid = 0 error" << endl;
				return 1;
			}
		}
		DEBUG("waited for all pid, returning");
	}

	return 0;
}

int executeExpression(Expression& expression) {
	// Check for empty expression
	if (expression.commands.size() == 0) {
		cerr << "No input command was given" << endl;
		return EINVAL;
	}

	// // Handle internal commands (like 'cd' and 'exit')
	int status = handleInternalCommands(expression);
	if (status == CHANGED_DIR_FLAG) {
		return 0; // cd happened
	}

	int rc = executeCommands(expression);
	if (rc != 0) {
		cerr << "executeCommands failed!" << endl;
		cerr << strerror(rc) << endl;
		cerr << strerror(errno) << endl;
	}
	return 0;

}

int normal(bool showPrompt) {
	while (cin.good()) {
		string commandLine = requestCommandLine(showPrompt);
		Expression expression = parseCommandLine(commandLine);
		int rc = executeExpression(expression);

		if (rc != 0) {
			cerr << "mainloop received error:\n";
			cerr << rc << " : " << strerror(rc) << endl;
		}
	}
	return 0;
}

// unused commands below.

// deprecated
// handles only 1 simple command and exactly 0 pipes.
int executeSingleCommandSimple(Expression& expression) {
	pid_t cpid = fork();
	if (cpid == 0) {
		int rc = executeCommand(expression.commands[0]);
		// if code reaches here, error has occurred. handle and abort.
		cerr << "executeSingleCommandSimple errored:\n"
			<< strerror(rc) << endl;
		abort();
	}
	waitpid(cpid, NULL, 0);
	return 0;
}


// deprecated
// handles expression with exactly 1 pipe
// see demoTwoCommands() for docstrings
int executeDualCommandSimple(Expression& expression) {
	int channel[2];

	if (pipe(channel) != 0) {
		cerr << "Failed to create pipe\n";
	}

	pid_t child1 = fork();
	if (child1 == 0) {
		dup2(channel[1], STDOUT_FILENO);
		close(channel[1]);
		close(channel[0]);
		Command cmd = expression.commands[0];
		executeCommand(cmd);
		abort();
	}

	pid_t child2 = fork();
	if (child2 == 0) {
		dup2(channel[0], STDIN_FILENO);
		close(channel[1]);
		close(channel[0]);
		Command cmd = expression.commands[1];
		executeCommand(cmd);
		abort();
	}

	close(channel[0]);
	close(channel[1]);

	waitpid(child1, nullptr, 0);
	waitpid(child2, nullptr, 0);
	return 0;
}

// DEMO COMMAND
// framework for executing "date | tail -c 5" using raw commands
// two processes are created, and connected to each other
int demoTwoCommands(bool showPrompt) {
	// create communication channel shared between the two processes
	int channel[2];

	// create a pipe
	// channel[0] refers to the read end of the pipe.
	// channel[1] refers to the write end of the pipe.
	if (pipe(channel) != 0) {
		cerr << "Failed to create pipe\n";
	}

	pid_t child1 = fork();
	if (child1 == 0) {
		// redirect standard output (STDOUT_FILENO) to the input of the shared communication channel
		dup2(channel[1], STDOUT_FILENO);
		// free non used resources (because otherwise, they stay opened)
		close(channel[1]);
		close(channel[0]);

		Command cmd = { {string("date")} };
		executeCommand(cmd);

		abort(); // if the executable is not found, we should abort.
		   // ( otherwise this child will go live it's own unintended life )
	}

	pid_t child2 = fork();
	if (child2 == 0) {
		// redirect the output of the shared communication channel to the standard input (STDIN_FILENO).
		dup2(channel[0], STDIN_FILENO);
		close(channel[1]);
		close(channel[0]);

		Command cmd = { {string("tail"), string("-c"), string("5")} };
		executeCommand(cmd);
		abort();
	}

	// don't forget to close the pipes in the parent process!
	close(channel[0]);
	close(channel[1]);

	// wait on ALL child processes to finish
	waitpid(child1, nullptr, 0);
	waitpid(child2, nullptr, 0);
	return 0;
}


// DEMO COMMAND
// note : not much error handling here!
int demoThreeCommandsOnePipe(bool showPrompt) {
	cout << "Demo with three commands. \n";
	cout << "executing: date | tail -c 15 | tail c 5 \n";

	// fd[0] refers to the read end of the pipe.
	// fd[1] refers to the write end of the pipe.
	int fd[2]; // fd == filedescriptor == int
	int input = STDIN_FILENO;

	if (pipe(fd) != 0) {
		cout << "Failed to create pipe!\n";
	}

	pid_t cpid_1 = fork();
	if (cpid_1 == 0) {
		// child 1 reads input from the 'date' command
		Command cmd = { {string("date")} };
		// set stdout to write end of pipe
		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]);
		close(fd[1]); // WHY

		executeCommand(cmd);
	}

	cout << input << "  was input  1" << endl;

	// keep previous pipe end
	input = fd[0];

	close(fd[1]); // THIS IS IMPORTANT.
	pipe(fd);     // next pipe

	pid_t cpid_2 = fork();
	if (cpid_2 == 0) {
		// child 2 reads input from the previous child
		// and into the next pipe for the next child
		Command cmd = { {string("tail"), string("-c"), string("15")} };

		dup2(input, STDIN_FILENO);
		dup2(fd[1], STDOUT_FILENO);
		close(input);
		close(fd[0]);
		close(fd[1]);
		executeCommand(cmd);
	}

	input = fd[0];
	close(fd[1]);

	pid_t cpid_3 = fork();
	if (cpid_3 == 0) {
		// last child receives output from child 2 from the
		// read-end of the second pipe, fd2[0]
		Command cmd = { {string("tail"), string("-c"), string("7")} };
		dup2(input, STDIN_FILENO);
		close(input);
		close(fd[0]);
		close(fd[1]);
		executeCommand(cmd);
	}

	close(input);
	close(fd[0]);
	close(fd[1]);

	waitpid(cpid_1, NULL, 0);
	waitpid(cpid_2, NULL, 0);
	waitpid(cpid_3, NULL, 0);

	flush(cout);
	cout << "DEMO DONE" << endl;
	return 0;
}

int shell(bool showPrompt) {
	// main shell loop
	return normal(showPrompt);

	/// available demo's
	/// return demoTwoCommands(showPrompt);
	/// return demoThreeCommandsOnePipe(showPrompt);
}