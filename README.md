Changes / Modifications :

This modified xv6 folder is a customized version of the original operating system, designed to include a new feature: a simplified version of strace. The modifications span across multiple directories, including kernel, user, and mkfs, along with updates to the main Makefile. In the kernel folder, key files such as syscall.c and sysproc.c have been updated to handle the new system call for tracing. Changes were also made to proc.c to manage process-specific tracing flags and to trap.c to enable system call tracing at the trap level. A new file, trace.h, was added to define important constants and helper functions for strace. Additional updates were made to defs.h and exec.c to ensure the tracing functionality works smoothly across different process and system states.

In the user folder, several new programs have been added to test and demonstrate strace. For example, strace_testing.c shows how system calls are traced, and memory_leak2.c is used to test memory-related system calls. Existing programs like sh.c (the shell) were modified to include tracing features, allowing users to enable and manage strace dynamically. Supporting files like user.h and usys.S were also updated to define and implement the necessary interfaces for the tracing system. You’ll also find compiled versions of these programs, such as strace_testing.o and memory_leak2.o, in the user directory.

The mkfs folder hasn’t been changed significantly but remains an essential part of the build process to create the filesystem for xv6. The root Makefile was updated to make sure all the new and modified files are compiled and linked correctly. This ensures that everything works together seamlessly.

To run this enhanced version of xv6, go to the root directory (/workspaces/xv6-public) and use the command “make clean && make && make qemu-nox” . This will clean up old builds, compile everything, and start the system in QEMU without the graphical interface. These changes bring a powerful debugging feature, strace, into xv6, making it easier to trace system calls and understand what’s happening under the hood without needing to modify or inspect the source code of running programs.





How to run and expected behaviour:
4.1.1 - Get familiar with Linux strace
Command: strace echo "Hello, Vamshi!"
Expected Behavior: Displays all system calls made by the echo command, including calls like execve, write, brk, etc., along with their return values.
Command: strace -c echo "Hello, Vamshi!"
Expected Behavior: Summarizes the system calls, showing the count of each call, the total time spent, and any errors encountered.

4.1.2 - Explanation of System Calls
No command to run; this section explains the functionality of system calls like execve, brk, access, etc., based on the output from 4.1.1.
4.2.1 - strace on
Command: Get into xv6 shell and type strace on.
Expected Behavior: Enables tracing for all commands executed in the shell. System calls will be logged and displayed for each command executed after enabling tracing.
4.2.2 - strace off
Command: Get into xv6 shell and type strace off.
Expected Behavior: Disables tracing. System calls will no longer be logged or displayed for commands executed after disabling tracing.
4.2.3 - strace run
Command: Get into xv6 shell and type strace run echo "Hello, Vamshi!".
Expected Behavior: Traces only the echo command, showing system calls made during its execution. Tracing will stop automatically after the command completes.
4.2.4 - strace dump
Command: Get into xv6 shell and type strace dump.
Expected Behavior: Outputs the contents of a circular buffer containing the most recent system calls logged during tracing. This requires tracing to be active before the dump.
4.2.5 - Trace Child Processes
Command: Run a test program (e.g., strace_testing.c) with multiple fork() calls while tracing is enabled. Get into xv6 shell and type strace_testing
Expected Behavior: Displays system calls made by the parent and all forked child processes, with their respective pid values.
4.3.1 - Option: -e <system call name>
Command: Get into xv6 shell and type strace -e write echo "Hello, Vamshi!".
Expected Behavior: Displays only write system calls made during the execution of echo.
4.3.2 - Option: -s <system call name>
Command: Get into xv6 shell and type strace -s cat no.txt.
Expected Behavior: Displays only successful system calls (those with non-negative return values) made during the execution of cat no.txt.
4.3.3 - Option: -f <system call name>
Command: Get into xv6 shell and type strace -f cat no.txt.
Expected Behavior: Displays only failed system calls (those with a return value of -1) made during the execution of cat no.txt.
4.3.4 - Combined Options (-s -e, -f -e)
Command: Get into xv6 shell and type strace -s -e write echo "Hello, Vamshi!" .
Expected Behavior: Displays only successful write system calls during the execution of echo.
Command: Get into xv6 shell and type strace -f -e exec cat no.txt .
Expected Behavior: Displays only failed exec system calls during the execution of cat no.txt.
4.3.5 - Implement -c Option
4.4 - Write Output of strace to File
4.5 - Application of strace
Command: Run the memory_leak.c program with strace enabled. Get into xv6 shell and type memory_leak
Expected Behavior: Displays the sbrk system calls made by the program, showing increasing return values with each memory allocation. When memory is exhausted, sbrk will return -1, indicating failure.





