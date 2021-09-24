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

// Parses a string to form a vector of arguments. The seperator is a space char (' ').
vector<string> splitString(const string &str, char delimiter = ' ')
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
int execvp(const vector<string> &args)
{
	// build argument list
	const char **c_args = new const char *[args.size() + 1];
	for (size_t i = 0; i < args.size(); ++i)
	{
		c_args[i] = args[i].c_str();
	}
	c_args[args.size()] = nullptr;
	// replace current process with new process as specified
	int retval = ::execvp(c_args[0], const_cast<char **>(c_args));
	// if we got this far, there must be an error
	// in case of failure, clean up memory (this won't overwrite errno normally, but let's be sure)
	int err = errno;
	delete[] c_args;
	errno = err;
	return retval;
}

// Executes a command with arguments. In case of failure, returns error code.
// should not return as it runs execvp, which replaces the process
int executeCommand(const Command &cmd)
{
	auto &parts = cmd.parts;
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
	char *dir = getcwd(buffer, sizeof(buffer));
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
		string &line = commands[i];
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
		expression.commands.push_back({args});
	}
	return expression;
}

int executeExpression(Expression &expression)
{
	// Check for empty expression
	if (expression.commands.size() == 0)
		return EINVAL;

	// Handle intern commands (like 'cd' and 'exit')

	// External commands, executed with fork():
	// Loop over all commandos, and connect the output and input of the forked processes

	// For now, we just execute the first command in the expression. Disable.
	executeCommand(expression.commands[0]);

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
		cout << "failed to create pipe\n";
	}

	pid_t child1 = fork();
	if (child1 == 0)
	{
		// redirect standard output (STDOUT_FILENO) to the input of the shared communication channel
		dup2(channel[1], STDOUT_FILENO);
		// free non used resources (because otherwise, they stay opened)
		close(channel[1]);close(channel[0]);

		Command cmd = {{string("date")}};
		executeCommand(cmd);

		abort(); // if the executable is not found, we should abort. 
		// ( otherwise this child will go live it's own unintended life )
	}

	pid_t child2 = fork();
	if (child2 == 0)
	{
		// redirect the output of the shared communication channel to the standard input (STDIN_FILENO).
		dup2(channel[0], STDIN_FILENO);
		close(channel[1]);close(channel[0]);

		Command cmd = {{string("tail"), string("-c"), string("5")}};
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

int shell(bool showPrompt)
{

	/// return normal(showPrompt);

	return step1(showPrompt);
}
