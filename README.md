# Operating Systems Assignment 1: shell

# [original repo here ](https://gitlab.science.ru.nl/OperatingSystems/assignment1.git)
^ click link to see setup instructions for linux/macOS/Windows

# bugs
No major known bugs. I hope.

# features
- `cd` and `cd ~` should lead to user `$HOME` directory
- reading/writing output when running this shell from within WSL2 will yield nice carriage returns before the newlines. ⌨

# how to use
Click [the link](https://gitlab.science.ru.nl/OperatingSystems/assignment1.git) in the top of this readme and follow the instructions there!!

# easter egg 🥚
Define `SHELLTHEME` in `shell.cpp` as 1 for a custom prompt
```cpp
#define SHELLTHEME 1
```