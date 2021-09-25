# Operating Systems Assignment 1: shell

# [original repo here ](https://gitlab.science.ru.nl/OperatingSystems/assignment1.git)
^ click link to see setup instructions for linux/macOS/Windows

# bugs
note the adapted `normal` function to better understand upcoming errors.
```cpp
int normal(bool showPrompt)
{
	while (cin.good())
	{	
		string commandLine = requestCommandLine(showPrompt);
		Expression expression = parseCommandLine(commandLine);
		int rc = executeExpression(expression);

		if (rc != 0){
			cerr << "mainloop received error:\n";
			cerr << rc << " : " << strerror(rc) << endl;
		}
	}
	cerr << "cin.notsogoodanymore()\n";
	return 0;
}
```

After running any command, the prompt gets 'filled' with 'Invalid argument'
```bash
/home/queue$ pwd
waiting for pid: cpid 1377013770 started with input: 0

/home/queue
waited for all pid, returning
/home/queue$ mainloop received error
22 : Invalid argument
cin.notsogoodanymore()
queue@iError:~$ 
```
note that `queue@iError:~$ ` is the prompt of my default system shell, not our custom one.


After adding `cout.clear()` to `normal`...
```cpp
int normal(bool showPrompt)
{
	while (cin.good())
	{	
		cout.clear();
        ...
```

I get this error...
```bash
$  awef awef
mainloop received error
22 : Invalid argument
cin.notsogoodanymore()
[1] + Done                       "/usr/bin/gdb" --interpreter=mi --tty=${DbgTerm} 0<"/tmp/Microsoft-MIEngine-In-lm02nazq.4du" 1>"/tmp/Microsoft-MIEngine-Out-bgaxwo1i.wzy"
```
fun.

vscode error whilst (TRYING TO DO SOME) debugging
```
Unable to open 'fork.c': Unable to read file 'vscode-remote://wsl+ubuntu-20.04/build/glibc-eX1tMB/glibc-2.31/sysdeps/nptl/fork.c' (Error: Unable to resolve non-existing file 'vscode-remote://wsl+ubuntu-20.04/build/glibc-eX1tMB/glibc-2.31/sysdeps/nptl/fork.c').

and 

Unable to write file 'vscode-remote://wsl+ubuntu-20.04/build/glibc-eX1tMB/glibc-2.31/sysdeps/nptl/fork.c' (NoPermissions (FileSystemError): Error: EACCES: permission denied, mkdir '/build')

```

this time, very slooowly executed with debug steps
```
/home/queue$ pwd
cpid 14163 started with input: 0
/home/queue$ 
waiting for pid: 14163
waited for all pid, returning
/home/queue$  ef afwae fawe f f\ea| fe waf e| feaw 
mainloop received error
22 : Invalid argument
cin.notsogoodanymore()






but then i get the same vscode/wsl errors again so i cant continue

 [1] + Done                       "/usr/bin/gdb" --interpreter=mi --tty=${DbgTerm} 0<"/tmp/Microsoft-MIEngine-In-wqodz2ir.c45" 1>"/tmp/Microsoft-MIEngine-Out-3t0thnky.rmu"
```