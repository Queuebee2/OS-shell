/**
* Shell
* Operating Systems
* v20.08.28
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

#include <sys/stat.h> // for open()
#include <fcntl.h>

#include <vector>

// thanks to https://stackoverflow.com/a/14256296/6934388
#define DEBUGMODE 1
#define DEBUG(x) do {  if (DEBUGMODE) { std::cerr << x << std::endl; } } while (0)
#define DEBUG2(x) do { if (DEBUGMODE) { std::cerr << #x << ": " << x << std::endl; } } while (0)

using namespace std;

struct Command
{
	vector<string> parts = {};
};

struct Expression
{
	vector<Command> commands;
	string inputFromFile;
	string outputToFile;
	bool background = false;
};

const int CHANGED_DIR_FLAG = 65;

// Parses a string to form a vector of arguments. The seperator is a space char (' ').
vector<string> splitString(const string& str, char delimiter = ' ')
{
	vector<string> retval;
	for (size_t pos = 0; pos < str.length();)
	{
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
int execvp(const vector<string>& args)
{
	// build argument list
	const char** c_args = new const char* [args.size() + 1];
	for (size_t i = 0; i < args.size(); ++i)
	{
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
int executeCommand(const Command& cmd)
{
	auto& parts = cmd.parts;
	if (parts.size() == 0)
		return EINVAL;

	// execute external commands
	int retval = execvp(parts);
	return retval;
}

// shows a user prompt in the terminal showing the current working dir and a $
void displayPrompt()
{
	char buffer[512];
	char* dir = getcwd(buffer, sizeof(buffer));
	if (dir)
	{
		cout << "\e[32m" << dir << "\e[39m"; // the strings starting with '\e' are escape codes, that the terminal application interpets in this case as "set color to green"/"set color to default"
	}
	cout << "$ ";
	flush(cout);
}

// gets the current input from the commandline
// (and shows prompt if showPrompt==True)
string requestCommandLine(bool showPrompt)
{
	if (showPrompt)
	{
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
Expression parseCommandLine(string commandLine)
{
	Expression expression;
	vector<string> commands = splitString(commandLine, '|');
	for (size_t i = 0; i < commands.size(); ++i)
	{
		string& line = commands[i];
		vector<string> args = splitString(line, ' ');
		if (i == commands.size() - 1 && args.size() > 1 && args[args.size() - 1] == "&")
		{
			expression.background = true;
			args.resize(args.size() - 1);
		}
		if (i == commands.size() - 1 && args.size() > 2 && args[args.size() - 2] == ">")
		{
			expression.outputToFile = args[args.size() - 1];
			args.resize(args.size() - 2);
		}
		if (i == 0 && args.size() > 2 && args[args.size() - 2] == "<")
		{
			expression.inputFromFile = args[args.size() - 1];
			args.resize(args.size() - 2);
		}
		expression.commands.push_back({ args });
	}
	return expression;
}

// handle a 'cd' command, only handles a single viable path.
int handleChangeDirectory(Command cmd)
{
	if (cmd.parts.size() != 2)
	{
		cout << "Wrong amount of arguments for cd " << endl;
		cout << "Usage: cd <path>" << endl;
		return CHANGED_DIR_FLAG;
	}

	int errcode = chdir(cmd.parts.at(1).c_str());
	if (errcode != 0)
	{
		cerr << "chdir failed " << errcode << endl;
		switch (errcode)
		{
		case -1: // 'Unknown error'
			cerr << "the given path was probably invalid." << endl;
			break;
		default:
			cerr << strerror(errcode) << endl;
		}
	}
	return CHANGED_DIR_FLAG;
}

// handle exit and chande dir.
int handleInternalCommands(Expression& expression)
{
	for (const auto& command : expression.commands)
	{
		for (const auto& part : command.parts)
		{
			if (part.compare("exit") == 0)
			{
				cout << "Bye!" << endl;
				flush(cout);
				exit(0);
			}
		}
		if (command.parts[0].compare("cd") == 0)
		{
			return handleChangeDirectory(command);
		}
	}
	return 0;
}

// handles only 1 simple command and exactly 0 pipes.
int executeSingleCommandSimple(Expression& expression)
{

	pid_t cpid = fork();
	if (cpid == 0)
	{
		int rc = executeCommand(expression.commands[0]);
		// if code reaches here, error has occurred. handle and abort.
		cerr << "executeSingleCommandSimple errored:\n"
			<< strerror(rc) << endl;
		abort();
	}
	waitpid(cpid, NULL, 0);
	return 0;
}

// handles expression with exactly 1 pipe
// see demoTwoCommands() for docstrings
int executeDualCommandSimple(Expression& expression)
{
	int channel[2];

	if (pipe(channel) != 0)
	{
		cout << "Failed to create pipe\n";
	}

	pid_t child1 = fork();
	if (child1 == 0)
	{
		dup2(channel[1], STDOUT_FILENO);
		close(channel[1]);
		close(channel[0]);
		Command cmd = expression.commands[0];
		executeCommand(cmd);
		abort();
	}

	pid_t child2 = fork();
	if (child2 == 0)
	{
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

int executeManyCommandsSinglePipe(Expression& expression)
{
	int LAST = expression.commands.size() - 1;
	int pipefd[2];

	pid_t cpid;
	pid_t cpids[expression.commands.size()];

	int input = STDIN_FILENO;

	for (int i = 0; i < expression.commands.size(); i++)
	{
		if (i != LAST)
		{
			// if theres more processes to 
			// be started, we need pipes to
			// redirect their I/O fds
			pipe(pipefd);
		}

		cpid = fork();
		if (cpid == 0)
		{
			if (input != STDIN_FILENO)
			{
				// (skips first child)
				// replace childs stdin with output from
				// previous pipe
				dup2(input, STDIN_FILENO);
				// clear resources
				close(input);
			}
			if (i != LAST)
			{	// last child should output
				// to stdout, so should not replace
				// its stdout with any pipe.
				// redirect their Stdout
				// input of next pipe
				dup2(pipefd[1], STDOUT_FILENO);
				// close remaining resources
				close(pipefd[1]);
				close(pipefd[0]);
			}

			// (ERROR LOGGING?)
			// cerr << getpid() << " executing ";
			// for (auto part : expression.commands[i].parts)
			// {
			// 	cerr << part << " ";
			// }
			// cerr << endl;
			executeCommand(expression.commands[i]);
			exit(1); // todo error handle.
		}
		else
		{
			// parent
			if (i != LAST)
			{
				// parent resets the output of
				// the pipe that has been made
				// to be input of next child
				dup2(pipefd[0], input);
			}
			// now we need to clear the remaining open
			// pipe ends for the parent
			close(pipefd[0]);
			close(pipefd[1]);
		}
		// administration
		// question: should/could be skipped if exp.background=true?
		cpids[i] = cpid;
	}

	// wait for children to finish their processing
	// (skips if expression.background=true)
	if (!expression.background)
	{
		for (auto p : cpids)
		{
			//cerr << "waiting for " << p << endl;
			if (p != 0)
			{
				waitpid(p, NULL, 0);
			}
			// else
			// {
			// 	cerr << "Encountered pid = 0 error" << endl;
			// }
		}
	}
}

int executeExpression(Expression& expression)
{
	// Check for empty expression
	if (expression.commands.size() == 0)
		return EINVAL;

	// Handle internal commands (like 'cd' and 'exit')
	int status = handleInternalCommands(expression);
	if (status == CHANGED_DIR_FLAG) // cd happened
	{
		return 0;
	}

	executeManyCommandsSinglePipe(expression);


	return 0;
}

int normal(bool showPrompt)
{
	while (cin.good())
	{
		string commandLine = requestCommandLine(showPrompt);
		Expression expression = parseCommandLine(commandLine);
		int rc = executeExpression(expression);
		if (rc != 0)
			cerr << strerror(rc) << endl;
	}
	return 0;
}

// framework for executing "date | tail -c 5" using raw commands
// two processes are created, and connected to each other
int demoTwoCommands(bool showPrompt)
{
	// create communication channel shared between the two processes
	int channel[2];

	// create a pipe
	// channel[0] refers to the read end of the pipe.
	// channel[1] refers to the write end of the pipe.
	if (pipe(channel) != 0)
	{
		cout << "Failed to create pipe\n";
	}

	pid_t child1 = fork();
	if (child1 == 0)
	{
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
	if (child2 == 0)
	{
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

int demoThreeCommands(bool showPrompt)
{
	cout << "Demo with three commands. \n";
	cout << "executing: date | tail -c 15 | tail c 5 \n";

	// fd[0] refers to the read end of the pipe.
	// fd[1] refers to the write end of the pipe.
	int fd[2]; // fd == filedescriptor == int
	int fd2[2];

	if (pipe(fd) != 0)
	{
		cout << "Failed to create pipe!\n";
	}

	if (pipe(fd2) != 0)
	{
		cout << "Failed to create pipe!\n";
	}

	pid_t cpid_1 = fork();
	if (cpid_1 == 0)
	{ // child 1 reads input from the 'date' command
		Command cmd = { {string("date")} };
		// set stdout to write end of pipe
		dup2(fd[1], STDOUT_FILENO);

		close(fd[0]);
		close(fd[1]); // WHY
		close(fd2[0]);
		close(fd2[1]);

		executeCommand(cmd);
	}

	pid_t cpid_2 = fork();
	if (cpid_2 == 0)
	{ // child 2 reads input from the previous child
	 // and into the next pipe for the next child
		Command cmd = { {string("tail"), string("-c"), string("15")} };

		dup2(fd[0], STDIN_FILENO);
		dup2(fd2[1], STDOUT_FILENO);

		close(fd[0]);
		close(fd[1]);
		close(fd2[0]);
		close(fd2[1]);
		executeCommand(cmd);
	}

	pid_t cpid_3 = fork();
	if (cpid_3 == 0)
	{ // last child receives output from child 2 from the
	 // read-end of the second pipe, fd2[0]
		Command cmd = { {string("tail"), string("-c"), string("7")} };
		dup2(fd2[0], STDIN_FILENO);
		close(fd[0]);
		close(fd[1]);
		close(fd2[0]);
		close(fd2[1]);
		executeCommand(cmd);
	}

	close(fd[0]);
	close(fd[1]);
	close(fd2[0]);
	close(fd2[1]);

	waitpid(cpid_1, NULL, 0);
	waitpid(cpid_2, NULL, 0);
	waitpid(cpid_3, NULL, 0);

	flush(cout);
	cout << "DEMO DONE" << endl;
	return 0;
}

int shell(bool showPrompt)
{
	// main shell loop
	return normal(showPrompt);

	// // testArea
	// Command cmdDate = {{string("date")}};
	// Command cmdTail1 = {{string("tail"), string("-c"), string("15")}};
	// Command cmdTail2 = {{string("tail"), string("-c"), string("7")}};
	// Command cmdTail3 = {{string("tail"), string("-c"), string("3")}};
	// Expression a, b, c;
	// a.commands = {{{cmdDate}}};

	// executeManyCommands2(a); // date

	// b.commands = {{{cmdDate}, {cmdTail1}, {cmdTail2}, {cmdTail3}}};

	// executeManyCommands2(b);

	// c.commands = {{
	//  {cmdDate},
	//  {cmdTail1},
	// }};

	// executeManyCommands2(c);

	// executeManyCommands2(c);

	/// available demo's
	/// return demoTwoCommands(showPrompt);
	// return demoThreeCommands(showPrompt);
}
