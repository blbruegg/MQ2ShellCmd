// MQ2ShellCmd.cpp : Defines the entry point for the DLL application.
//

// This plugin allows you to issue a shell command from within EQ. (A shell command 
// just meaning something you could run from the command prompt and it would work.)
//
// It keeps track of the process by letting you assign a name to it and allows you 
// to report on the status.  If you run two processes with the same name, it does 
// not kill the previous process and only the new process will be tracked.

#include <iostream>
#include <windows.h>
#include <map>
#include <tchar.h>
#include "../MQ2Plugin.h"

PreSetup("MQ2ShellCmd");

namespace KnightlyShellCmd {
	bool boolDebug = false;
	class SpawnedProcess {
		public:
			std::string strCommand;
			std::string strFullCommand;
			//std::wstring wstrFullCommand; // Only needed for Unicode
			std::string strStatus;
			STARTUPINFO siStartupInfo = {};
			PROCESS_INFORMATION piProcessInfo = {};
			DWORD dwExitCode = 0;
	};

	std::map<std::string, SpawnedProcess> ProcessTracker;

	// Log Functions we'll be using
	class Log {
	public:
		// Message is for logging a standard message.
		// All other logging calls go through this base.
		static void Message(std::string strMessage) {
			CHAR pcharMessage[MAX_STRING];
			strMessage = "\ay[\agMQ2ShellCmd\ay]\aw ::: \ao" + strMessage;
			strcpy_s(pcharMessage, strMessage.c_str());
			WriteChatf(pcharMessage);
		}

		// Warning is for logging warnings
		static void Warning(std::string strWarning) {
			strWarning = "\ayWARNING: " + strWarning;
			Message(strWarning);
		}

		// Error is for logging errors
		static void Error(std::string strError) {
			strError = "\arERROR: " + strError;
			Message(strError);
		}

		// Debug is for logging debug messages and only
		// works if boolDebug is TRUE.
		static void Debug(std::string strDebug) {
			strDebug = "\amDEBUG: " + strDebug;
			if (boolDebug) {
				Message(strDebug);
			}
		}

		static void ShowHelp() {
			Message("\ayUsage:");
			Message("\ay     /cmd <CommandName> Command");
			Message("\ayWhere:");
			Message("\ay     <CommandName> is just an arbitrary name used to reference the command you ran.");
			Message("\ay     Command is the actual command you want to run");
			Message("\ayExample:");
			Message("\ay     /cmd TestCommand notepad.exe \"C:\\Test.txt\"");
			Message(" ");
			Message("\ayAvailable TLOs:");
			Message("\ay     ${shellcmd.command[CommandName]} -- String - Tells you what command was run (w/ escaped backslashes)");
			Message("\ay     ${shellcmd.full[CommandName]} -- String - Tells you the full command (w/ escaped backslashes)");
			Message("\ay     ${shellcmd.status[CommandName]} -- String - Current status - Either Starting, Failed, Running, Unknown or Exit + code");
			Message("\ay     ${shellcmd.kill[CommandName]} -- Bool - Kills the cmd process that was spawned and clears memory.  Returns Success/Fail + Exit Code");
			Message("\ayExample:");
			Message("\ay     /echo ${shellcmd.status[TestCommand]}");
		}
	};


	// Process Handling Class
	class ProcessHandling {
		public:
			//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
			// From: https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
			static std::string GetLastErrorAsString()
			{
				//Get the error message, if any.
				DWORD errorMessageID = ::GetLastError();
				if (errorMessageID == 0)
				{
					return std::string(); //No error message has been recorded
				}

				LPSTR messageBuffer = nullptr;
				size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

				std::string message(messageBuffer, size);

				//Free the buffer.
				LocalFree(messageBuffer);

				return message;
			}

			// Basically just makes sure the process exits and then handles
			// closing ...well, the handles
			static bool UpdateStatus(std::string strProcessName) {
				// Check if we even have a process named this
				if (KnightlyShellCmd::ProcessTracker.count(strProcessName) == 1)
				{
					// If the status is "Success" or "Running" then get the details.  Otherwise the details aren't likely to change
					if (KnightlyShellCmd::ProcessTracker[strProcessName].strStatus == "Success" || KnightlyShellCmd::ProcessTracker[strProcessName].strStatus == "Running")
					{
						// Get Exit Code information
						if (GetExitCodeThread(KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo.hThread, &KnightlyShellCmd::ProcessTracker[strProcessName].dwExitCode) == false)
						{
							KnightlyShellCmd::Log::Error("Failed to get exit code for process named:  " + strProcessName);
							KnightlyShellCmd::ProcessTracker[strProcessName].strStatus = "Unknown";
							// Close the handles for this process since we don't know anything about it.
							CloseHandle(KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo.hProcess);
							CloseHandle(KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo.hThread);
						}
						// If we found a code...
						else {
							// Is the process still running?
							if (KnightlyShellCmd::ProcessTracker[strProcessName].dwExitCode == STILL_ACTIVE)
							{
								KnightlyShellCmd::Log::Debug("Process still running: " + strProcessName);
								KnightlyShellCmd::ProcessTracker[strProcessName].strStatus = "Running";
							}
							// If it's not running, it must have exited
							else {
								KnightlyShellCmd::Log::Debug("Process exited: " + strProcessName);
								// Set the status on the map so we don't keep asking for it.
								KnightlyShellCmd::ProcessTracker[strProcessName].strStatus = "Exit " + std::to_string(KnightlyShellCmd::ProcessTracker[strProcessName].dwExitCode);
								// Close the handles since the process has exited
								CloseHandle(KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo.hProcess);
								CloseHandle(KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo.hThread);
							}
						}
					}
					return true;
				}
				else {
					return false;
				}
			}
	};

	// String Manipulation functions we'll be using.
	class StringManip {
		public:
			static std::string ReplaceAll(std::string strOriginal, std::string strToReplace, std::string strReplaceWith) {
				size_t index = 0;
				index = strOriginal.find(strToReplace, index);
				while (index != std::string::npos) {
					strOriginal.replace(index, strToReplace.length(), strReplaceWith);
					// Skip what we just replaced
					index += strReplaceWith.length();
					index = strOriginal.find(strToReplace, index);
				}
				return strOriginal;
			}
	};
}

// Define the Cmd Command
PLUGIN_API VOID CmdCommand(PSPAWNINFO pSpawn, PCHAR szLine)
{
	CHAR szParam1[MAX_STRING] = { 0 };
	CHAR szParam2[MAX_STRING] = { 0 };

	std::string strLine = szLine; 
	std::string strCmdCommand;
	// Put the first parameter in szParam1, include quotes
	GetArg(szParam1, szLine, 1, 1);
	GetArg(szParam2, szLine, 2, 1);

	// If the first parameter is "help" or empty then show the help info
	if (szParam1 && (!strcmp(szParam1, "help") || strlen(szParam1) == 0)) {
		KnightlyShellCmd::Log::ShowHelp();
	}
	else {
		// Check to make sure we have at least 2 parameters (if not we don't have enough to process a command)
		if (szParam2 && strlen(szParam2) > 0) {
			char* szComSpec = nullptr;
			size_t iSize = 0;
			if (_dupenv_s(&szComSpec, &iSize, "ComSpec") == 0 && szComSpec != nullptr)
			{
				std::string strProcessName = szParam1;
				// Setup Process Information (Start by warning if we're overwriting something that hasn't exited)
				if (KnightlyShellCmd::ProcessTracker.count(strProcessName) == 1 && KnightlyShellCmd::ProcessTracker[strProcessName].strStatus.substr(0, 4) != "Exit") {
					KnightlyShellCmd::Log::Warning(strProcessName + " reference exists and last status is not Exit - overwriting reference.");
				}
				// The status is Starting
				KnightlyShellCmd::ProcessTracker[strProcessName].strStatus = "Starting";
				// The shell command is the remainder of the line after the first paramater (and space)
				KnightlyShellCmd::ProcessTracker[strProcessName].strCommand = strLine.substr((std::string(szParam1) + " ").length());
				ZeroMemory(&KnightlyShellCmd::ProcessTracker[strProcessName].siStartupInfo, sizeof(KnightlyShellCmd::ProcessTracker[strProcessName].siStartupInfo));
				KnightlyShellCmd::ProcessTracker[strProcessName].siStartupInfo.cb = sizeof(KnightlyShellCmd::ProcessTracker[strProcessName].siStartupInfo);
				// Use Show Window Flags when starting the process
				KnightlyShellCmd::ProcessTracker[strProcessName].siStartupInfo.dwFlags = STARTF_USESHOWWINDOW;
				// Set the process to show the window, but minimize it and make sure it doesn't steal focus.
				KnightlyShellCmd::ProcessTracker[strProcessName].siStartupInfo.wShowWindow = SW_SHOWMINNOACTIVE;
				ZeroMemory(&KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo, sizeof(KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo));
				KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand = "\"" + std::string(szComSpec) + "\" /C title MQ2ShellCmd: " + strProcessName + "&& " + KnightlyShellCmd::ProcessTracker[strProcessName].strCommand;
				// Resize wstrFullCommand to fit the string we just compiled (Only needed for Unicode)
				// KnightlyShellCmd::ProcessTracker[strProcessName].wstrFullCommand.resize(MultiByteToWideChar(CP_ACP, 0, KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand.c_str(), (int)KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand.length() + 1, 0, 0));
				// Convert the full string to unicode and store it in wstrFullCommand. (Only needed for Unicode)
				//MultiByteToWideChar(CP_ACP, 0, KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand.c_str(), (int)KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand.length() + 1, &KnightlyShellCmd::ProcessTracker[strProcessName].wstrFullCommand[0], MultiByteToWideChar(CP_ACP, 0, KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand.c_str(), (int)KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand.length() + 1, 0, 0));

				if (!CreateProcess(NULL, // Application Name - Null says use command line processor
					(LPSTR)KnightlyShellCmd::ProcessTracker[strProcessName].strFullCommand.c_str(), // Command line to run
					NULL,           // Process Attributes - handle not inheritable
					NULL,           // Thread Attributes - handle not inheritable
					FALSE,          // Set handle inheritance to FALSE
					CREATE_NEW_CONSOLE, // Creation Flags - Create a new console window instead of running in the existing console
					NULL,           // Use parent's environment block
					NULL,           // Use parent's starting directory 
					&KnightlyShellCmd::ProcessTracker[strProcessName].siStartupInfo,		// Pointer to STARTUPINFO structure
					&KnightlyShellCmd::ProcessTracker[strProcessName].piProcessInfo)		// Pointer to PROCESS_INFORMATION structure
					)
				{
					KnightlyShellCmd::Log::Error("CreateProcess failed (" + KnightlyShellCmd::ProcessHandling::GetLastErrorAsString() + ")");
					KnightlyShellCmd::ProcessTracker[strProcessName].strStatus = "Failed";
				}
				else {
					KnightlyShellCmd::ProcessTracker[strProcessName].strStatus = "Success";
				}
			}
			else {
				KnightlyShellCmd::Log::Error("Could not find an appropriate command processor.");
			}
		}
		else {
			KnightlyShellCmd::Log::Error("Missing parameters.");
			KnightlyShellCmd::Log::ShowHelp();
		}
	}

}

class MQ2ShellCmdType *pShellCmdType = nullptr;
class MQ2ShellCmdType : public MQ2Type {
private:
	CHAR _szBuffer[MAX_STRING];
public:
	enum Members {
		Command,
		command,
		Full,
		full,
		Status,
		status,
		Kill,
		kill
	};

	MQ2ShellCmdType() : MQ2Type("ShellCmd") {
		TypeMember(Command);
		TypeMember(command);
		TypeMember(Full);
		TypeMember(full);
		TypeMember(Status);
		TypeMember(status);
		TypeMember(Kill);
		TypeMember(kill);
	}

	bool GetMember(MQ2VARPTR VarPtr, char* Member, char* Index, MQ2TYPEVAR &Dest) {
		_szBuffer[0] = '\0';
		// Command Name
		CHAR szCommandParam1[MAX_STRING] = { 0 };
		GetArg(szCommandParam1, Index, 1, 1);

		PMQ2TYPEMEMBER pMember = MQ2ShellCmdType::FindMember(Member);
		if (!pMember) return FALSE;

		// Check if we even have a process named this
		if (KnightlyShellCmd::ProcessHandling::UpdateStatus(szCommandParam1))
		{
			KnightlyShellCmd::Log::Debug("Found the process: " + std::string(szCommandParam1));
			// Find out what we're supposed to do
			switch ((Members)pMember->ID) {
				case Command:
				case command:
					// Command tells you what command was run and returns a string
					Dest.Type = pStringType;
					// Copy the command into the buffer, but limit it to the size of the buffer
					strncpy_s(_szBuffer, KnightlyShellCmd::StringManip::ReplaceAll(KnightlyShellCmd::ProcessTracker[szCommandParam1].strCommand, "\\", "\\\\").c_str(), _TRUNCATE);
					Dest.Ptr = &_szBuffer[0];
					return TRUE;
				case Full:
				case full:
					// Full tells you the modified command that was run and returns a string
					Dest.Type = pStringType;
					// Copy the command into the buffer, but limit it to the size of the buffer.  Escape the string so it will display
					strncpy_s(_szBuffer, KnightlyShellCmd::StringManip::ReplaceAll(KnightlyShellCmd::ProcessTracker[szCommandParam1].strFullCommand, "\\", "\\\\").c_str(), _TRUNCATE);
					Dest.Ptr = &_szBuffer[0];
					return TRUE;
				case Status:
				case status:
					KnightlyShellCmd::Log::Debug("Status TLO was requested for: " + std::string(szCommandParam1));
					// Status tells you the current status of the command and returns a string
					Dest.Type = pStringType;
					// The status is the status... (limit it to the size of the buffer)
					strncpy_s(_szBuffer, KnightlyShellCmd::ProcessTracker[szCommandParam1].strStatus.c_str(), _TRUNCATE);
					Dest.Ptr = &_szBuffer[0];
					return TRUE;
				case Kill:
				case kill:
					KnightlyShellCmd::Log::Debug("Issuing Kill Command");
					Dest.Type = pBoolType;
					bool bProcessTerm = TerminateProcess(KnightlyShellCmd::ProcessTracker[szCommandParam1].piProcessInfo.hProcess, 0);
					// If we closed the process or the process has already closed then close the handles
					if (bProcessTerm || KnightlyShellCmd::ProcessTracker[szCommandParam1].strStatus.substr(0, 4) == "Exit")
					{
						KnightlyShellCmd::Log::Debug("Closing Handles for killed process");
						// Close the handles
						CloseHandle(KnightlyShellCmd::ProcessTracker[szCommandParam1].piProcessInfo.hProcess);
						CloseHandle(KnightlyShellCmd::ProcessTracker[szCommandParam1].piProcessInfo.hThread);
						// Remove the reference
						KnightlyShellCmd::ProcessTracker.erase(szCommandParam1);
					}
					Dest.Int = bProcessTerm;
					return TRUE;
			}
			// If we made it here return false
			return FALSE;
		}
		// Otherwise we don't have a Process tracker for this.
		else {
			KnightlyShellCmd::Log::Warning("No process found.  Did you name the command '" + std::string(szCommandParam1) + "'?");
			Dest.Type = pBoolType;
			Dest.Int = FALSE;
			return TRUE;
		}
	}

	bool FromData(MQ2VARPTR &VarPtr, MQ2TYPEVAR &Source) { return FALSE; }
	bool FromString(MQ2VARPTR &VarPtr, char* Source) { return FALSE; }
};

BOOL ShellCmdData(PCHAR szIndex, MQ2TYPEVAR &Dest)
{
	Dest.DWord = 1;
	Dest.Type = pShellCmdType;
	return TRUE;
}

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID)
{
	DebugSpewAlways("Initializing MQ2ShellCmd");
	// Add /cmd
	AddCommand("/cmd", CmdCommand);
	// Add a data type to handle results
	pShellCmdType = new MQ2ShellCmdType;
	AddMQ2Data("shellcmd", ShellCmdData);
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
	DebugSpewAlways("Shutting down MQ2ShellCmd");

	// Remove /sqlite
	RemoveCommand("/cmd");
	// Remove data type
	RemoveMQ2Data("shellcmd");
	delete pShellCmdType;
}