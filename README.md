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


after adding `cout.clear()` to `normal`
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