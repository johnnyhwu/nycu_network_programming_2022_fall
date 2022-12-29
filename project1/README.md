# Project 1: Create a NPShell

## Introduction
In this project, you are asked to design a shell named npshell. The npshell should support the
following features:

1. Execution of commands 
2. Ordinary Pipe
3. Numbered Pipe
4. File Redirection

## File Structure
- **./cmds** : some cpp file provided by TA
- **./demo** : npshell should be put in this directory
   - **./demo/bin** : some program provided by TA, the other (ex. `ls`, `cat`) is copied from `/bin` in linux
   - **./demo/test.html** : sample html file to test npshell
- **./demo_hard** : contain some hard test cases and script to test npshell automatically
- **./src** : source code for npshell
   - **./src/cmder.[cpp|h]** : main part of code to parse and execute command
   - **./src/userio.[cpp|h]** : input/output from/to console
- **./main.cpp** : entry point of npshell
- **./spec.pdf** : specification and objective of this project

## Prepare Program in /demo/bin
For non-linux program, please complie cpp files in `./cmds`, and move it to `./demo/bin`.

For example:
```
cd cmds
g++ noop.cpp noop
cp noop ../demo/bin
```

For linux program such as `ls` or `cat`, you can simply copy them from `/bin` in your machine:

For example:
```
cp /bin/ls ./demo/bin
```

## Build npshell
Simply execute `make` command, it will compile cpp files in `./src` and link all object files to generate `npshell` which is put in `./demo`:
```
make
```

Execute `npshell`:
```
cd demo
./npshell
```

## Clean Project
Use `make clean` to remove object files and `npshell` program:
```
make clean
```

## Usage of npshell (Simple Test Case)

For the meaning of these usage and the required function of npshell, please refer to **Section 3** in project specification (`./spec.pdf`). You should sequentially execute all test cases to get expected result.

### Test Case 1
```
% printenv PATH
bin:.
% setenv PATH bin
% printenv PATH
bin
```

### Test Case 2
```
% ls
bin  npshell  test.html
% ls bin
cat  date  ls  manyblessings  noop  number  removetag  removetag0  wc
```

### Test Case 3
```
% cat test.html > test1.txt
% cat test1.txt
<!test.html>
<TITLE>Test</TITLE>
<BODY>This is a <b>test</b> program
for ras.
</BODY>
% removetag test.html

Test
This is a test program
for ras.

% removetag test.html > test2.txt
% cat test2.txt

Test
This is a test program
for ras.

% removetag0 test.html
Error: illegal tag "!test.html"

Test
This is a test program
for ras.

% removetag0 test.html > test2.txt
Error: illegal tag "!test.html"
% cat test2.txt

Test
This is a test program
for ras.

```

### Test Case 4
```
% removetag test.html | number
   1 
   2 Test
   3 This is a test program
   4 for ras.
   5 
```

### Test Case 5
```
% removetag test.html |1
% number
   1 
   2 Test
   3 This is a test program
   4 for ras.
   5 
```

### Test Case 6
```
% removetag test.html |2
% ls
bin  npshell  test1.txt  test2.txt  test.html
% number
   1 
   2 Test
   3 This is a test program
   4 for ras.
   5 
```

### Test Case 7
```
% removetag test.html |2
% removetag test.html |1
% number
   1 
   2 Test
   3 This is a test program
   4 for ras.
   5 
   6 
   7 Test
   8 This is a test program
   9 for ras.
  10 
```

### Test Case 8
```
% removetag test.html |2
% removetag test.html |1
% number |1
% number
   1    1 
   2    2 Test
   3    3 This is a test program
   4    4 for ras.
   5    5 
   6    6 
   7    7 Test
   8    8 This is a test program
   9    9 for ras.
  10   10 
```

### Test Case 9
```
% removetag test.html | number |1
% number
   1    1 
   2    2 Test
   3    3 This is a test program
   4    4 for ras.
   5    5 
```

### Test Case 10
```
% removetag test.html |2 removetag test.html |1
% number |1 number
   1    1 
   2    2 Test
   3    3 This is a test program
   4    4 for ras.
   5    5 
   6    6 
   7    7 Test
   8    8 This is a test program
   9    9 for ras.
  10   10 
```

### Test Case 11
```
% ls |2
% ls
bin  npshell  test1.txt  test2.txt  test.html
% number > test3.txt
% cat test3.txt
   1 bin
   2 npshell
   3 test1.txt
   4 test2.txt
   5 test.html
```

### Test Case 12
```
% removetag0 test.html |1
% Error: illegal tag "!test.html"
number
   1 
   2 Test
   3 This is a test program
   4 for ras.
   5 
```

:bomb: As you see, there is an error in this test case. The correct one should be:

```
% removetag0 test.html |1
Error: illegal tag "!test.html"
% number
   1 
   2 Test
   3 This is a test program
   4 for ras.
   5 
```

### Test Case 13
```
% removetag0 test.html !1
% number
   1 Error: illegal tag "!test.html"
   2 
   3 Test
   4 This is a test program
   5 for ras.
   6 
```

### Test Case 14
```
% ctt
Unknown command: [ctt].
% ctt -n
Unknown command: [ctt].
% ctt | ls
Unknown command: [ctt].
bin  npshell  test1.txt  test2.txt  test3.txt  test.html
% ls | ctt
Unknown command: [ctt].
% ls |2
% ctt
Unknown command: [ctt].
% cat
bin
npshell
test1.txt
test2.txt
test3.txt
test.html
% exit
```

## Usage of npshell (Hard Test Case)
In order to test the completeness and robutness of npshell, TA provides some hard test cases and script to test npshell. Please move npshell program to `./demo_hard` and execute:
```
cd demo_hard
./demo.sh npshell
```

## Limitation of Implementation
- You can only use C/C++ to do this project. Other third-party libraries are NOT allowed
- In this project, system() function is NOT allowed.
- Except for the three built-in commands (setenv, printenv, and exit), you MUST use the exec family of functions to execute commands.
- You MUST create unnamed pipes to implement ordinary pipe and numbered pipe. Storing data into temporary files is NOT allowed for ordinary pipe and numbered pipe.
- You should handle the forked processes properly, or there might be zombie processes.
- You should set the environment variable PATH to bin/ and ./ initially.
- You should NOT manage environment variables by yourself. Functions like getenv() and setenv() are allowed.
- The commands noop, number, removetag, and removetag0 are offered by TA.
- The executables ls and cat are usually placed in the folder /bin/ in UNIX-like systems. You can copy them to your working directory.


