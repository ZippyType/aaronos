# AaronOS Agent Instructions

You are the specialized AI engine for **AaronOS**. Your primary job is the development, maintenance, and expansion of the AaronOS kernel and its ecosystem.

## Core Identity & Constraints
- **Project Name:** AaronOS
- **Kernel Integrity:** `kernel.c` must always be higher then the previous after editing. Do not truncate or simplify the core logic.

## Operational Workflow (Mandatory)
You must follow this three-step process for every request:

1. **PLAN FIRST:** Analyze the request and outline exactly what changes will be made across which files. List the logic and any potential impacts.
2. **CONFIRMATION:** Stop and wait for the user to say "proceed," "go," or give a confirmation nod before touching any code.
3. **EXECUTE:** Only after confirmation, perform the file edits and provide the updated full files.

## Technical Standards
- Maintain high standards for C code within the kernel.
- Ensure all newly added commands are registered in the internal help system.
- Make sure all code and commands work in QMEU CLI (no x11 display) mode. It should be configured to ouput to a serial port too.
## After testing and code edits
1. Ask the user if he wants to Commit changes to Github.
2. If the user agrees, then write a commit message and tell the user the commit message.
3. Final confirmation. 
4. Add all files (git add .) then commit with the commit message.

## How to run AaronOS.
- here is an explenation:
(2. The No-Graphic Mode

If AaronOS is configured to output to a serial port (it should be), you can use the -nographic flag. This disables all graphical output and redirects the serial port to your current terminal’s input/output.
```
Bash
qemu-system-x86_64 -hda aaronos.img -nographic
```
Note: To exit this mode, press Ctrl+A then X.)
## ABOUT THE CHAT
- Always add the full chat history to chat.json.
- Full chat history. Like this: 
```
chathistory {
chat1 {
Model {
 <model name>}
 user {
 first thing}
 }
 chat2 {
    user {
        hi
    }
    ai {
        whats up
    }
 }
 etc.
}
```
That is the way how to store chat.json.
Make sure chat.json is in the gitignore.

## About subagents to make your job easier
- You may use/make subagents. Here is how to do it:
1. ALWAYS READ " https://opencode.ai/docs/agents/ ". That is everything to know.
2. Make the subagent (if you need to.) run ``` opencode agent list ``` to see all agents. Then make an agent if that is required with ``` opencode agent create ```. Follow all on-screen instructions.

##  IMPORTANT
- When you are done with a prompt/task, do the following:
1. A check in the code to see if there are any errors:
2. Run subagents if applicable:
3. Run the code-checker subagent (required):
4. test with qmeu and test compiling
5. Ask user for github stuff
6. End the message.