#ifdef _WIN32 // lowercasing header names for MinGW compatibility
	#include "stdafx.h"
	#include <windows.h>
	#include <tchar.h> 
	#include <psapi.h>
	#include <winbase.h> 
	#include <tlhelp32.h>
#else
	#include <errno.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <cstdio>
	#include <dirent.h>
	#include <algorithm>
	#include <cstdlib>
	#include <fstream>
	#include <regex>
	#include <sys/ptrace.h>
	#include <sys/wait.h>
#endif //_WIN32

#include <iostream>
#include <vector>
#include <iomanip>
#include <string>
#include <random>
#include <cstring>
#include <cstdint>
#include <thread>
#include <chrono>

//Random vars for the random number generator
static std::random_device rd;
static std::mt19937_64 gen(rd());

size_t randomLong(size_t max)
{
	std::uniform_int_distribution<size_t> dis;
	size_t rand = dis(gen);

	//Mod of max
	rand = rand%max;
	return rand;
}

size_t randomLong()
{
	std::uniform_int_distribution<size_t> dis;
	size_t rand = dis(gen);
	return rand;
}

void printError(const char * msg)
{
	int eNum;

#ifdef _WIN32
	eNum = GetLastError();
#else
	eNum = errno;
#endif // _WIN32
	
	// Display the message
	std::cerr << "WARNING: " << msg << "failed with error " << eNum << '(' << strerror(eNum) << ')' << std::endl;
}

//This structure acts like a tuple but I know exactly whats in it and can edit it at any time
struct range
{
	uintptr_t start;
	uintptr_t end;
};

#ifdef _WIN32
std::vector<PROCESSENTRY32> GetProcessList()
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	std::vector<PROCESSENTRY32> processes;
	// Take a snapshot of all processes in the system.
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE)
	{
		printError(TEXT("CreateToolhelp32Snapshot (of processes)"));
		return processes;
	}

	// Set the size of the structure before using it.
	pe32.dwSize = sizeof(PROCESSENTRY32);

	// Retrieve information about the first process,
	// and exit if unsuccessful
	if (!Process32First(hProcessSnap, &pe32))
	{
		printError(TEXT("Process32First")); // show cause of failure
		CloseHandle(hProcessSnap);          // clean the snapshot object
		return processes;
	}

	// Now walk the snapshot of processes, and
	// display information about each process in turn
	do
	{
		processes.push_back(pe32);
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	return processes;
}

std::vector<range> getValidAddresses(PROCESSENTRY32 process)
{
	SYSTEM_INFO sys_Info;
	GetSystemInfo(&sys_Info);
	LPVOID minAddr = sys_Info.lpMinimumApplicationAddress;
	LPVOID maxAddr = sys_Info.lpMaximumApplicationAddress;
	uintptr_t minAddrVal = static_cast<uintptr_t>minAddr;
	uintptr_t maxAddrVal = static_cast<uintptr_t>maxAddr;
	std::vector<range> output;

	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, process.th32ProcessID);
	//Varibles for VirtualQueryEx
	MEMORY_BASIC_INFORMATION memBasicInfo;
	SIZE_T bytesRead = 0; //Number of bytes read with ReadProcessMemory
	uintptr_t previousMinAddr = minAddrVal;//Here to stop adress getter from pre-maturly 
	while (minAddrVal < maxAddrVal)
	{
		VirtualQueryEx(processHandle, minAddr, &memBasicInfo, sizeof(MEMORY_BASIC_INFORMATION));

		//If the memory chunk is accessible
		if (memBasicInfo.Protect == PAGE_READWRITE && memBasicInfo.State == MEM_COMMIT)
		{
			range r;
			r.start = static_cast<uintptr_t>(memBasicInfo.BaseAddress);
			r.end = static_cast<uintptr_t>(memBasicInfo.BaseAddress) + static_cast<size_t>(memBasicInfo.RegionSize);
			output.push_back(r);
		}

		minAddrVal += static_cast<size_t>memBasicInfo.RegionSize;
		/*
		The seperate address values fixes the strange nature of the LPVOID pointer where it dosent act like a proper pointer
		It insted does something where I cant even predict where the next address should be, but casting it from a size_t seems to do the trick.
		Also the size_t is used for 2 reasons,
		1, when something is in the full 64 bit range, it dosent use 2^64/2 which is what it will use when dealing with negatives
		2, I am always counting up, so if it becomes low again, I know it has ended.
		*/
		//Checks if the min address is less than the previous min address. that always leads to loops and thats no good.
		//Call it a cheap bugfix, but it fixes the damn bug ⌐_⌐
		if (minAddrVal < previousMinAddr)
			break;

		previousMinAddr = minAddrVal;
		minAddr = reinterpret_cast<LPVOID>(minAddrVal);
	}
	CloseHandle(processHandle);

	return output;
}
#else // NOTE: Does this work on Mac as well?
std::vector<pid_t> GetProcessList()
{
	std::vector<pid_t> processes;
	dirent * dp;
	DIR * procs = opendir("/proc");
	
	if (!procs)
		std::cerr << "Unable to open /proc" << std::endl;
	else
	{
		while ((dp = readdir(procs)) != nullptr)
		{
			pid_t pid = atoi(dp->d_name);
			if (pid <= 0) continue;
			processes.push_back(pid);
		}
	}
	
	return processes;
}

std::vector<range> getValidAddresses(pid_t process)
{
	std::vector<range> ranges;
	std::ifstream maps_file(std::string("/proc/" + std::to_string(process) + "/maps"));
	std::string line;
	
	while (std::getline(maps_file, line))
	{
		std::smatch m;
		if (std::regex_match(line, m, std::regex("([-lrw])([0-9A-Fa-f]+)-([0-9A-Fa-f]+)")))
			if (m[2] == "r") // readable region?
			{
				range r;
				r.start = std::stoull(m[1], 0, 16);
				r.end = std::stoull(m[2], 0, 16);
				ranges.push_back(r);
			}
	}
	
	maps_file.close();
	return ranges;
}
#endif // _WIN32

//Really simple script for really simple people (like me)
size_t totalLength(std::vector<range> & pages)
{
	size_t totalLength = 0;
	for (auto ran: pages)
		totalLength += ran.end - ran.start;
	return totalLength;
}

uintptr_t memoryMap(std::vector<range> pages, size_t position)
{

	if (pages.size())
	{
		size_t prevVal = 0;
		size_t val = 0;
		for (size_t i = 0; i < pages.size(); i++)
		{
			val += pages[i].end - pages[i].start;
			if (position <= val)
				return pages[i].start + (position - prevVal);
			else
				prevVal = val;
		}
	}
	return 0;
}

int main(int argc, char ** argv)
{
	auto processes = GetProcessList();
	for (unsigned i = 0; i < processes.size(); i++)
#ifdef _WIN32
		std::cout << i << ' ' << processes[i].szExeFile << std::endl;
#else
	{
		std::ifstream cmd_file(std::string("/proc/"+std::to_string(processes[i])+"/cmdline"));
		std::string line;
		std::getline(cmd_file, line, ' ');
		std::cout << processes[i] << '\t' << line << std::endl;
		cmd_file.close();
	}
#endif // _WIN32

	int input;
	std::cin >> input;

	std::vector<range> ranges = getValidAddresses(processes[input]);
	for (auto ran: ranges)
		std::cout << std::right << std::setfill('0') << std::setw(8) << std::hex << ran.start << " - " << //Left
			std::right << std::setfill('0') << std::setw(8) << std::hex << ran.end << " " << //Right 
			std::endl;

#ifdef _WIN32
	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, processes[input].th32ProcessID);
#else
	ptrace(PTRACE_ATTACH, processes[input], nullptr, nullptr);
#endif // _WIN32

	while (true)
	{
		size_t totLen = totalLength(ranges);
		size_t output;
		int exitCode = 0;
#ifdef _WIN32
		GetExitCodeProcess(processHandle, reinterpret_cast<LPDWORD>(&exitCode));
		if (exitCode == 259)
#else
		wait(&exitCode);
		if (WIFEXITED(exitCode)) break;
		else
#endif // _WIN32
		{
			//Gets a random value and mapps it to the correct memory address
			size_t uInput = randomLong(totLen);
			uintptr_t mappedAddr = memoryMap(ranges, uInput);

			//Just too lazy to make a random char, so I used the random long
			size_t first = (((randomLong(totLen)) / 1024)) * 1024;
			size_t last = ((randomLong(totLen)) / 1024) * 1024;

			//Swaps if first is greater than last
			if (last < first)
			{
				size_t temp = first;
				first = last;
				last = temp;
			}
			char val = randomLong();
			char changeVal = randomLong();
			size_t prevPrec = 0;
			std::cout << "First: " << std::hex << first << std::endl;
			std::cout << "Last : " << std::hex << last << std::endl;
			std::cout << std::dec << val << std::endl;
			std::cout << std::dec << 0 << "/" << last - first << " " << 0 << "%" << std::endl;
			size_t bytesChanged = 0;
			for (size_t i = first; i < last; i += 1024)
			{
#ifdef _WIN32
				GetExitCodeProcess(processHandle, reinterpret_cast<LPDWORD>(&exitCode));
				if (exitCode == 259)
#else
				wait(&exitCode);
				if (WIFEXITED(exitCode)) break;
#endif // _WIN32
				{
					size_t prec = ((i - first) * 100) / (last - first);

					if (prec > prevPrec)
					{
						std::cout << std::dec << i - first << "/" << last - first << " " << prec << "%" << std::endl;
						prevPrec = prec;
					}
					//Maps address to valid address
					uintptr_t addr = memoryMap(ranges, i);

					char outVal[1024];

					//Reads byte in memory
#ifdef _WIN32
					if (ReadProcessMemory(processHandle, reinterpret_cast<LPVOID>(addr), outVal, 1024, &output))
#else
					if (ptrace(PTRACE_PEEKDATA, processes[input], addr, nullptr) != -1)
#endif // _WIN32
					{
						for (size_t j = 0; j < output; j++)
						{
							char cChar = static_cast<char>(outVal[j]);
							//Checks if value is equal to val
							if (cChar == val)
							{
								//Writes byte to memory if successful
								if (static_cast<bool>(randomLong()))
								{
#ifdef _WIN32
									WriteProcessMemory(processHandle, reinterpret_cast<LPVOID>(addr), reinterpret_cast<LPCVOID>(&changeVal), 4, &output);
#else
									ptrace(PTRACE_POKEDATA, processes[input], addr+4, changeVal); 
#endif // _WIN32
									std::cout << std::right << std::setfill('0') << std::setw(8) << std::hex << addr << " " << changeVal << std::endl;
									bytesChanged++;
								}
							}
							if (bytesChanged >= 100)
								goto done;
						}
					}
					else
						std::cerr << "Error, could not read value at address " << std::hex << addr << std::endl << "Error code :" << std::dec
#ifdef _WIN32
							<< GetLastError()
#else
							<< errno
#endif // _WIN32
							<< std::endl;
				}
#ifdef _WIN32
				else
					break;
#endif // _WIN32
			}
done:
			std::cout << "Done. Bytes changed: " << std::dec << bytesChanged << std::endl;
#ifndef _WIN32
			ptrace(PTRACE_CONT, processes[input], nullptr, nullptr);
#endif // _WIN32
			//Waits for user
			std::string uwait;
			std::cin >> uwait;
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
#ifdef _WIN32
		else
		{
			std::cout << "Process ended: " << std::dec << exitCode << std::endl;
			std::string end;
			std::cin >> end;
			break;
		}
#endif // _WIN32
	}
#ifdef _WIN32
	CloseHandle(processHandle);
#endif // _WIN32

	return 0;
}
