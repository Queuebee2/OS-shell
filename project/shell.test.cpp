#include <gtest/gtest.h>
#include <stdlib.h>
#include <fcntl.h>

using namespace std;

// shell to run tests on
#define SHELL "../build/shell -t"
//#define SHELL "/bin/sh"

// declarations of methods you want to test (should match exactly)
vector<string> splitString(const string& str, char delimiter = ' ');

namespace {

void Execute(std::string command, std::string expectedOutput);
void Execute(std::string command, std::string expectedOutput, std::string expectedOutputFile, std::string expectedOutputFileContent);

TEST(Shell, splitString) {
	std::vector<std::string> expected;

	expected = {};
	EXPECT_EQ(expected, splitString(""));
	EXPECT_EQ(expected, splitString(" "));
	EXPECT_EQ(expected, splitString("  "));

	expected = {"foo"};
	EXPECT_EQ(expected, splitString("foo"));
	EXPECT_EQ(expected, splitString(" foo"));
	EXPECT_EQ(expected, splitString("foo "));
	EXPECT_EQ(expected, splitString(" foo "));
	EXPECT_EQ(expected, splitString("  foo  "));

	expected = {"foo", "bar"};
	EXPECT_EQ(expected, splitString("foo bar"));
	EXPECT_EQ(expected, splitString(" foo  bar"));
	EXPECT_EQ(expected, splitString("  foo   bar  "));

	expected = {"cmd1", "arg1", "<", "inputfile", "|", "cmd2", "arg2", ">", "outputfile"};
	EXPECT_EQ(expected, splitString("cmd1 arg1 < inputfile | cmd2 arg2 > outputfile"));
}

TEST(Shell, ReadFromFileUnix) {
	Execute("cat < 1", "line 1\nline 2\nline 3\nline 4");
}

TEST(Shell, ReadFromFileWindows) {
	Execute("cat < 1", "line 1\r\nline 2\r\nline 3\r\nline 4");
}

TEST(Shell, ReadFromAndWriteToFileUnix) {
	Execute("cat < 1 > ../foobar", "", "../foobar", "line 1\nline 2\nline 3\nline 4");
}

TEST(Shell, ReadFromAndWriteToFileWindows) {
	Execute("cat < 1 > ../foobar", "", "../foobar", "line 1\r\nline 2\r\nline 3\r\nline 4");
}

TEST(Shell, ReadFromAndWriteToFileChainedUnix) {
	Execute("cat < 1 | head -n 3 > ../foobar", "", "../foobar", "line 1\nline 2\nline 3\n");
	Execute("cat < 1 | head -n 3 | tail -n 1 > ../foobar", "", "../foobar", "line 3\r\n");
}

TEST(Shell, ReadFromAndWriteToFileChainedWindows) {
	Execute("cat < 1 | head -n 3 > ../foobar", "", "../foobar", "line 1\r\nline 2\r\nline 3\r\n");
	Execute("cat < 1 | head -n 3 | tail -n 1 > ../foobar", "", "../foobar", "line 3\r\n");
}

TEST(Shell, WriteToFile) {
	Execute("ls -1 > ../foobar", "", "../foobar", "1\n2\n3\n4\n");
}

TEST(Shell, Execute) {
	//Execute("uname", "Linux\n");
	Execute("ls", "1\n2\n3\n4\n");
	Execute("ls -1", "1\n2\n3\n4\n");
}

TEST(Shell, ExecuteChained) {
	Execute("ls -1 | head -n 2", "1\n2\n");
	Execute("ls -1 | head -n 2 | tail -n 1", "2\n");
}

/*==================================================*/

TEST(Shell, RemovingFiles){
    Execute("rm doesntExist | ls", "1\n2\n3\n4\n");
    Execute("touch ../test-dir2/newFile | rm ../test-dir2/newFile | ls", "1\n2\n3\n4\n");
}

TEST(Shell, creatingFiles){
	Execute("touch ../test-dir2/newFile | ls ../test-dir2", "newFile\n");
	Execute("touch ../test-dir2/newFile | touch ../test-dir2/newFile | ls ../test-dir2", "newFile\n");
}

TEST(Shell, exitBehaviour){
    Execute("touch ../test-dir2/newFile | exit", "Bye!\n");
    Execute("exit | touch ../test-dir2/newFile", "Bye!\n");
    Execute("touch ../test-dir2/newFile | exit | touch ../test-dir2/newFile2 ", "Bye!\n");
}

TEST(Shell, backgroundCommands){
    Execute("time (sleep 5  | sleep 5 &)","0.00s user 0.00s system 0\% cpu 5.004 total");
}

TEST(Shell, goodErrors){
	Execute("","No input command was given\nmainloop received error:\n22 : Invalid argument");
	Execute("nonExistingCmd", "encountered bad command: nonExistingCmd");
	Execute("pwd > 1", "opening file error for output\n File exists");
}

/*==================================================*/

//////////////// HELPERS

std::string filecontents(const std::string& str) {
	std::string retval;
	int fd = open(str.c_str(), O_RDONLY);
	struct stat st;
	if (fd >= 0 && fstat(fd, &st) == 0) {
		long long size = st.st_size;
		retval.resize(size_t(size));
		char *current = const_cast<char*>(retval.c_str());
		ssize_t left = size;
		while (left > 0) {
			ssize_t bytes = read(fd, current, size_t(left));
			if (bytes == 0 || (bytes < 0 && errno != EINTR))
				break;
			if (bytes > 0) {
				current += bytes;
				left -= bytes;
			}
		}
	}
	if (fd >= 0)
		close(fd);
	return retval;
}

void filewrite(const std::string& str, std::string content) {
	int fd = open(str.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return;
	while (content.size() > 0) {
		ssize_t written = write(fd, content.c_str(), content.size());
		if (written == -1 && errno != EINTR) {
			std::cout << "error writing file '" << str << "': error " << errno << std::endl;
			break;
		}
		if (written >= 0)
			content = content.substr(size_t(written));
	}
	close(fd);
}

void Execute(std::string command, std::string expectedOutput) {
	char buffer[512];
	std::string dir = getcwd(buffer, sizeof(buffer));
	filewrite("input", command);
	std::string cmdstring = std::string("cd ../test-dir; " SHELL " < '") +  dir + "/input' > '" + dir + "/output' 2> /dev/null";
	system(cmdstring.c_str());
	std::string got = filecontents("output");
	EXPECT_EQ(expectedOutput, got);
}

void Execute(std::string command, std::string expectedOutput, std::string expectedOutputFile, std::string expectedOutputFileContent) {
	char buffer[512];
	std::string dir = getcwd(buffer, sizeof(buffer));
	std::string expectedOutputLocation = "../test-dir/" + expectedOutputFile;
	unlink(expectedOutputLocation.c_str());
	filewrite("input", command);
	std::string cmdstring = std::string("cd ../test-dir; " SHELL " < '") + dir + "/input' > '" + dir + "/output' 2> /dev/null";
	int rc = system(cmdstring.c_str());
	EXPECT_EQ(0, rc);
	std::string got = filecontents("output");
	EXPECT_EQ(expectedOutput, got) << command;
	std::string gotOutputFileContents = filecontents(expectedOutputLocation);
	EXPECT_EQ(expectedOutputFileContent, gotOutputFileContents) << command;
	unlink(expectedOutputLocation.c_str());
}

}
