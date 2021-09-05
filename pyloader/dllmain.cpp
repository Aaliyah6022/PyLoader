//---------------------------------------------------------
// PyLoader for Python 2.7
// Purpose: 	Inject python code into processes that are using python

#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iterator>
#include <streambuf>
#include <iomanip>

//---------------------------------------------------------
// App info

#define APP_NAME "PyLoader v1.0.1 (Python 2.7)"
const char *defaultfile = "metin2.py";

template <typename T>
T GetFunctionPtr(HMODULE module, const char* name) {
	T func = (T)GetProcAddress(module, name);
	if (NULL == func) {
		std::cout << "An error occured! Could not find " << name << "!"
			<< std::endl;
	}

	return func;
}

#define GET_FUNC_PTR(module, name) \
  name = GetFunctionPtr<PTR_##name>(module, #name)

//---------------------------------------------------------
// Typedefs

typedef int(*PTR_PyRun_SimpleStringFlags)(const char* command, void* flags);
PTR_PyRun_SimpleStringFlags PyRun_SimpleStringFlags = NULL;

typedef void* (*PTR_PyRun_String)(const char* str,
	int start,
	void* globals,
	void* locals);
PTR_PyRun_String PyRun_String = NULL;

// Thread stuff
typedef void(*PTR_PyEval_InitThreads)();
PTR_PyEval_InitThreads PyEval_InitThreads = NULL;

// Global Interpreter Lock (GIL)
enum PyGILState_STATE { PyGILState_LOCKED, PyGILState_UNLOCKED };

typedef enum PyGILState_STATE(*PTR_PyGILState_Ensure)(void);
PTR_PyGILState_Ensure PyGILState_Ensure = NULL;

typedef void(*PTR_PyGILState_Release)(enum PyGILState_STATE);
PTR_PyGILState_Release PyGILState_Release = NULL;

//#define Py_file_input 257
//#define Py_eval_input 258

HMODULE g_PythonDLL = NULL;


LPDWORD g_ThreadID = NULL;

void ShowCommands() {
	std::cout << "------------------------------------------------- \n"
		"Commands: \n"
		"load <filename>      Loads python script.\n"
		"help                 Displays commands.  \n"
		"------------------------------------------------- "
		<< std::endl;
}


bool file_exists(const char *fileName) {
	std::ifstream infile(fileName);
	return infile.good();
}

bool run_Py() {
	std::stringstream stream;
	stream << "exec(compile('import uiCommon;import zlib', '', 'single'))";
	PyGILState_STATE state = PyGILState_Ensure();
	int r = PyRun_SimpleStringFlags(stream.str().c_str(), NULL);
	PyGILState_Release(state);
	if (r == 0) {//loaded successfully
		return TRUE;
	}
	return FALSE;
}

DWORD WINAPI LoaderMain(LPVOID lpParam) {
	//check python functions/default file before loading console window
	if (!(GET_FUNC_PTR(g_PythonDLL, PyRun_SimpleStringFlags))) { return FALSE; }
	if (!(GET_FUNC_PTR(g_PythonDLL, PyRun_String))) { return FALSE; }
	// GIL
	if (!(GET_FUNC_PTR(g_PythonDLL, PyGILState_Ensure))) { return FALSE; }
	if (!(GET_FUNC_PTR(g_PythonDLL, PyGILState_Release))) { return FALSE; }

	while (true) {//run until import time is available
		if (run_Py() == TRUE) {
			break;
		}
	}

	if (file_exists(defaultfile)) {
		std::stringstream stream;
		stream << "exec(compile(open('" << defaultfile << "').read(), '" << defaultfile << "', 'exec'))";

		PyGILState_STATE state = PyGILState_Ensure();
		int r = PyRun_SimpleStringFlags(stream.str().c_str(), NULL);
		PyGILState_Release(state);
		if (r == 0) {//loaded successfully
			return TRUE;//dont open console if default file was loaded
		}
	}

	if (AllocConsole()) {
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		SetConsoleTitleA(APP_NAME);
		// SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN
		//  | FOREGROUND_BLUE | FOREGROUND_RED | BACKGROUND_BLUE);

		std::cout << APP_NAME << " by midas643" << std::endl;

		ShowCommands();

		while (true) {
			std::string in;
			std::getline(std::cin, in);

			int spaceFound = in.find(" ", 0);

			std::string cmdName = in.substr(0, spaceFound);
			std::string cmdParams;

			if (spaceFound != -1) {
				cmdParams = in.substr(spaceFound + 1, in.length() - spaceFound);
			}

			if (cmdName == "load") {
				if (!cmdParams.empty()) {
					std::cout << "Loading file " << cmdParams << std::endl;

					// std::ifstream t(cmdParams);
					// std::string str((std::istreambuf_iterator<char>(t)),
					//	std::istreambuf_iterator<char>());

					// PyThreadState *_save;
					//_save = PyEval_SaveThread();

					// PyEval_RestoreThread(_save);

					std::stringstream stream;
					stream << "exec(compile(open('" << cmdParams.c_str() << "').read(), '"
						<< cmdParams.c_str() << "', 'exec'))";

					PyGILState_STATE state = PyGILState_Ensure();
					int r = PyRun_SimpleStringFlags(stream.str().c_str(), NULL);
					PyGILState_Release(state);
					if (r == 0) {
						std::cout << "Python script loaded!" << std::endl;
					}
					else {
						std::cout << "Error occured while running python script. Check "
							"syserr.txt for more info."
							<< std::endl;
					}
				}
				else {
					std::cout << "File name required!" << std::endl;
				}
			}
			else if (cmdName == "help") {
				std::cout << "TODO..." << std::endl;
			}
			else {
				std::cout << "Unknown command! Use 'help' to display commands."
					<< std::endl;
			}
		}
	}

	return TRUE;
}

BOOL APIENTRY DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
	switch (dwReason) {
	case DLL_PROCESS_ATTACH: {
		DisableThreadLibraryCalls((HMODULE)hDllHandle);

		if (!(g_PythonDLL = GetModuleHandleA("python27.dll"))) {
			MessageBoxA(NULL, "Could not load python27.dll!", "Error",
				MB_ICONERROR);
			return FALSE;
		}

		PyEval_InitThreads = (PTR_PyEval_InitThreads)GetProcAddress(
			g_PythonDLL, "PyEval_InitThreads");
		if (!PyEval_InitThreads) {
			MessageBoxA(NULL,
				"An error occured! Could not find PyEval_InitThreads!",
				"Error", MB_ICONERROR);
			return FALSE;
		}

		// Must be called in main thread
		PyEval_InitThreads();

		CreateThread(NULL, 0, LoaderMain, 0, 0, g_ThreadID);

		break;
	}
	case DLL_PROCESS_DETACH: {
		break;
	}
	}

	return TRUE;
}