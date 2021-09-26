# Operating Systems Assignment 1: shell

# [original repo here ](https://gitlab.science.ru.nl/OperatingSystems/assignment1.git)
^ click link to see setup instructions for linux/macOS/Windows

# bugs
- ~~WSL2 permission errors (device specific I guess) when writing?? Or is the implementation of opening a fh (and creating etc ) wrong?~~

- `cat > bob.txt` 'works' now, but the typed content isn't written into the file (?? not saved??)
	- figure out: which flags, when? `O_WRONLY | O_TRUNC | O_CREAT | O_EXCL` (and others)
	- what 'mode' should be passed to `open(fn, flasgs, mode)` ? currently `0644`	

