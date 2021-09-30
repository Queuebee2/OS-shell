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
#define DEBUGMODE 0
#define DEBUGs(x) do {  if (DEBUGMODE>1) { std::cerr << x << std::endl; } } while (0)
#define DEBUG(x) do {  if (DEBUGMODE>1) { std::cerr << x << std::endl; } } while (0)
#define DEBUG2(x) do { if (DEBUGMODE>1) { std::cerr << #x << ": " << x << std::endl; } } while (0)

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
			cerr << "cd error:" << endl;
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


int executeManyCommandsSinglePipe(Expression& expression)
{
	int AMT_COMMANDS = expression.commands.size();
	int LAST = AMT_COMMANDS - 1;
	mode_t writePermissions = 0644;
	int pipefd[2];
	int inputfd, outputfd;

	// I don't know c++, so i dont know a 'better' way to do this right now.
	int FileInputModeFlag;
	int FileOutputModeFlag;
	// 
	// if ((expression.inputFromFile.empty() == 0) &&
	// 	(expression.outputToFile.empty() == 0) &&
	// 	(expression.inputFromFile.compare(expression.outputToFile) == 0))
	// {	// if both input and output files are the same
	// DEBUGs("Created read/write permission for file: "<< expression.outputToFile.c_str()l )
	// 	FileInputModeFlag = O_RDWR;
	// 	FileOutputModeFlag = O_RDWR;
	// }
	// else {
	FileInputModeFlag = O_RDONLY;
	FileOutputModeFlag = O_WRONLY | O_CREAT | O_EXCL | O_TRUNC;
	// }

	pid_t cpid;
	pid_t cpids[AMT_COMMANDS];

	if (expression.inputFromFile.empty() == 0) {

		// if an input file is given, create a filedescriptor and set it as input
		if ((inputfd = open(expression.inputFromFile.c_str(), FileInputModeFlag)) < 0) {
			cerr << "creating a file to read from failed!" << endl;
			cerr << strerror(errno) << endl; // errorcode should be better described with this
			return -1;
		}
		DEBUGs("Created inputfd " << inputfd << " for " << expression.inputFromFile.c_str());
		// TODO : what other flags are NEEDED, which ones COULD we handle?
	}
	else {
		inputfd = STDIN_FILENO;

	}

	for (int i = 0; i < AMT_COMMANDS; i++)
	{
		if (i != LAST)
		{
			// if there are more processes to 
			// be started, we a pipe to
			// redirect their I/O fds
			if (pipe(pipefd) != 0)
			{
				// todo (?) better error handling
				cerr << "Failed to create pipe!\n";
				abort();
			}
		}

		cpid = fork();
		if (cpid < 0) {
			cerr << strerror(cpid) << "\nfork failed" << endl;
			abort();
		}

		if (cpid == 0)
		{

			DEBUG("cpid " << getpid() << " started with input: " << inputfd);
			if (inputfd != STDIN_FILENO)
			{
				// (skips first child)
				// replace childs stdin with output from
				// previous pipe
				dup2(inputfd, STDIN_FILENO);
				// clear resources
				close(inputfd);
			}

			if (i != LAST)
			{	// last child should output
				// to stdout, so should not replace
				// its stdout with any pipe.
				// redirect their Stdout
				// input of next pipe
				dup2(pipefd[1], STDOUT_FILENO);
				close(pipefd[1]);
			} else 
			// if an outputfile is given
			if (i == LAST && expression.outputToFile.empty() == 0) {
				// open a fd for the output file in write or read/write mode
				// not sure if its the right option to do this here, instead of
				// above the whole for-loop
				// TODO, should this be A FULL path? or RELATIVE?
				// TODO: Fix this, cat > bob.txt runs but it doesnt SAVE.
				outputfd = open(expression.outputToFile.c_str(), FileOutputModeFlag,  0644);
				DEBUGs("Created outputfd: " << outputfd);
				// cerr << FileOutputModeFlag << endl;
				// cerr << expression.outputToFile.c_str() << endl;
				// cerr << outputfd;
				dup2(outputfd, STDOUT_FILENO);
				close(outputfd);
			} 

			// close remaining resources
			// close(inputfd);? why isnt this neededw
			close(pipefd[1]);
			close(pipefd[0]);

			int errcode = executeCommand(expression.commands[i]);
			if (errcode != 0) {
				cerr << getpid() << " encountered bad command: ";
				for (auto part : expression.commands[i].parts)
				{
					cerr << part << " ";
				}
				cerr << endl;
			}
			abort();

		}
		else
			// parent part of the loop
		{
			// make the new input the output of the pipe we have
			inputfd = pipefd[0];
			// close the write end of this pipe
			close(pipefd[1]);
			// administration
			// question: should/could be skipped if exp.background=true?
			cpids[i] = cpid;
		}
	}

	// wait for children to finish their processing
	// (skips if expression.background=true)
	if (!expression.background)
	{
		// waitpid(-1, NULL, 0); can we use this instead?
		for (auto p : cpids)
		{
			DEBUG("waiting for pid: " << p);
			if (p != 0)
			{
				waitpid(p, NULL, 0);
			}
			else
			{
				cerr << "Encountered pid = 0 error" << endl;
				return 1;
			}
		}
		DEBUG("waited for all pid, returning");
	}

	return 0;
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

	int rc = executeManyCommandsSinglePipe(expression);
	if (rc != 0) {
		cerr << "executeCommandSingePipe failed" << endl;
		cerr << strerror(rc) << endl;
	}
	return 0;

}

int normal(bool showPrompt)
{
	while (cin.good())
	{
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
		cerr << "Failed to create pipe\n";
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
		cerr << "Failed to create pipe\n";
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

int demoThreeCommandsOnePipe(bool showPrompt)
{
	cout << "Demo with three commands. \n";
	cout << "executing: date | tail -c 15 | tail c 5 \n";

	// fd[0] refers to the read end of the pipe.
	// fd[1] refers to the write end of the pipe.
	int fd[2]; // fd == filedescriptor == int
	int fd2[2]; //unused
	int input = STDIN_FILENO;

	if (pipe(fd) != 0)
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

		executeCommand(cmd);
	}

	cout << input << "  was input  1" << endl;


	// keep previous pipe end
	input = fd[0];
	// instead of line above would this be better??
	// input = dup(fd[0]);
	// close(fd[0]);

	close(fd[1]); // THIS )*(#$@U&(*@Y(FY(YHF(Y#(*FY(* YH#(*YF((&T&^(*T@(*&)ER(&!%@^RE&(!@VBE(&^!@%E(&^!@VRBE*^%@!%E^&(!@%G*#^@!%#!*@^#V&@!(^V%#(!&^@V#%^F&!(@^#$%V(&!@^%#GD(B&^!@G#%(I&!@F^G#%D(!@#^B*F^&!@%#VF%$(&%!@R$#*^&!@%G%#DB(&!@^%#$VB(!@&&^G#TBDUI!@^%#RVB@!&^O#BRI*^%!@#CV(&^ !@V%#)(&!@^%#S!@(&BV%#(@!^ &#V%()!@&^#G%BS(@!&#%^BIQ&@^#TRNCF!@#(*^%$#TF!^T(@FDC$D(^!@TRFC$(^@Y!TCV$)(U@Y!TFV$@!(UYTFV$)(!@&^T$F@!&)(^F$@!(VF$(@!UYG$FVOU@F!YGV$YUF!@UYGFO$U*OYGF@!*)UYGI$*)YG!@*)YGT$!@*)YU$&*(UY@!(&Y_$(*&Y!@_(_&*$Y!@(*_&Y$(*_&Y@!(_*&Y$(_!@*&Y$(_*&Y!@(_*&Y@!$(*_Y&(_*Y&@!$(_*Y@!$(*Y@!(*Y$(Y*@!$GU&O*&^O*&!@^&$*^O*$&^V@!)*&!^@$*)&!@^$)*&@!^)$*@!&^$)*&!@$^)!@*B&$^)!@*&^$

	pipe(fd); // next pipe

	pid_t cpid_2 = fork();
	if (cpid_2 == 0)
	{ // child 2 reads input from the previous child
	 // and into the next pipe for the next child
		Command cmd = { {string("tail"), string("-c"), string("15")} };

		dup2(input, STDIN_FILENO);
		dup2(fd[1], STDOUT_FILENO);
		close(input);
		close(fd[0]);
		close(fd[1]);
		close(fd2[0]);
		close(fd2[1]);
		executeCommand(cmd);
	}

	input = fd[0];
	close(fd[1]);

	pid_t cpid_3 = fork();
	if (cpid_3 == 0)
	{ // last child receives output from child 2 from the
	 // read-end of the second pipe, fd2[0]
		Command cmd = { {string("tail"), string("-c"), string("7")} };
		dup2(input, STDIN_FILENO);
		close(input);
		close(fd[0]);
		close(fd[1]);
		close(fd2[0]);
		close(fd2[1]);
		executeCommand(cmd);
	}

	close(input);
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
	//
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
	/// return demoThreeCommandsOnePipe(showPrompt);
}
