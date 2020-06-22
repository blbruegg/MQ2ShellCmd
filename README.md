# MQ2ShellCmd
This plugin allows you to issue a shell command from within EQ. (A shell command  just meaning 
something you could run from the command prompt and it would work.)  The process that is spawned
is separate from the EQ process and can run independently after EQ closes.

The plugin keeps track of the process by letting you assign a name to it and allows you to report 
on the status.  If you run two processes with the same name, but the first process has not been
killed or exited, it will give you a warning but otherwise it will still run.  What happens in
that case is that the plugin only keeps track of the latest process with that name.  The name you
give it is arbitrary and only used for how you refer to the process.

You can use the TLOs to "kill" the process if it is stuck or you just want to force it to end, but 
this only works for the first process that was spawned, not the entire process tree.  For example, 
if you run notepad using this plugin, you'll see a command prompt window and notepad will also pop
up.  Issuing a kill command to that will only close the command prompt window itself and not the
notepad window.  This is intentional.  Kill is really designed to either clear the tracking of
an already exited process or force close a shell script.

The process runs in the background and minimized so it should not steal focus, but I have chosen 
to still show the window so that you can interact with it without having to go into task manager 
and find a background process.  That can be annoying if you spawn multiple processes, but I think 
it's the best option for full visibility.

## Troubleshooting
If you feel like it's not doing what you expect, the best way to troubleshoot is to open up a
command prompt (as admin if you run EQ that way) and run the command that you're trying to get 
to execute.  Make sure it works there and it should work in EQ.

If you want to go further, you can use the "command" TLO to see the command you issued and if you
want to directly replicate what the plugin is doing you can use the "full" TLO to see the full
command since the plugin tacks on COMSPEC to your command.  Both of these TLOs are "escaped"
meaning that all backslashes have been changed to double backslashes for display only.  That means
if you try to compare "C:\Test" to something in a macro you need to use "C:\\Test" instead.  But
if you're just echo'ing then you'll see it as it is stored.

## Reasoning
I was originally going to write a PushOver plugin for sending messages to my phone, but I already 
have a lot of powershell scripts that work with PushOver and, in consideration, I realized that 
being able to execute any shell command would be much more useful while accomplishing the same
goal.

The plugin can be used with other plugins and within macros to do whatever (including my use case)
without requiring any additional configuration.  Since you can pass anything you want to it (like
MQ2 variables) you can accomplish pretty much anything that you need an external program to do.

## Help
```
Usage:
     /cmd <CommandName> Command
Where:
     <CommandName> is just an arbitrary name used to reference the command you ran.
     Command is the actual command you want to run
Example:
     /cmd TestCommand notepad.exe "C:\Test.txt\"

Available TLOs:
     ${shellcmd.command[CommandName]} -- String - Tells you what command was run (w/ escaped backslashes)
     ${shellcmd.full[CommandName]} -- String - Tells you the full command (w/ escaped backslashes)
     ${shellcmd.status[CommandName]} -- String - Current status - Either Starting, Failed, Running, Unknown or Exit + code
     ${shellcmd.kill[CommandName]} -- Bool - Kills the cmd process that was spawned and clears memory.  Returns Success/Fail + Exit Code
Example:
     /echo ${shellcmd.status[TestCommand]}
```