#include "stdafx.h"

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <tchar.h> 
#include <psapi.h>
#include <WinBase.h> 
#include <tlhelp32.h>
#include <tchar.h>
#include <vector>
#include <iomanip>
#include <string>
#include <fstream>
#include <cstdlib>
#include <random>
#include <cstdint>

typedef unsigned long ulong;//I used 'ulong' instead of 'unsigned long' because I couldn't be arsed to keep typing unsigned before long so i replaced unsigned with a u
/* The name of the process */
using namespace std;

//Random vars for the random number generator
random_device rd;
mt19937_64 gen(rd());

//This structure acts like a tuple but I know exactly whats in it and can edit it at any time
struct range
{
	ulong start;
	ulong end;
};

ulong randomLong(ulong max)
{
	//Code for random long

	uniform_int_distribution<ulong> dis;
	ulong rand = dis(gen);

	//Mod of max
	rand = rand%max;
	return rand;
}
ulong randomLong()
{
	//Code for random long

	uniform_int_distribution<ulong> dis;
	ulong rand = dis(gen);
	return rand;
}
void printError(TCHAR* msg)
{
	DWORD eNum;
	TCHAR sysMsg[256];
	TCHAR* p;

	eNum = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, eNum,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		sysMsg, 256, NULL);

	// Trim the end of the line and terminate it with a null
	p = sysMsg;
	while ((*p > 31) || (*p == 9))
		++p;
	do { *p-- = 0; } while ((p >= sysMsg) &&
		((*p == '.') || (*p < 33)));

	// Display the message
	_tprintf(TEXT("\n  WARNING: %s failed with error %d (%s)"), msg, eNum, sysMsg);
}

vector<PROCESSENTRY32> GetProcessList()
{
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;
	vector<PROCESSENTRY32> processes;
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

vector<range> getValidAddresses(PROCESSENTRY32 process)
{

	SYSTEM_INFO sys_Info;
	GetSystemInfo(&sys_Info);
	LPVOID minAddr = sys_Info.lpMinimumApplicationAddress;
	LPVOID maxAddr = sys_Info.lpMaximumApplicationAddress;
	ulong minAddrVal = (ulong)minAddr;
	ulong maxAddrVal = (ulong)maxAddr;
	vector<range> output;

	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, process.th32ProcessID);
	//Varibles for VirtualQueryEx
	MEMORY_BASIC_INFORMATION memBasicInfo;
	SIZE_T bytesRead = 0; //Number of bytes read with ReadProcessMemory
	ulong previousMinAddr = minAddrVal;//Here to stop adress getter from pre-maturly 
	while (minAddrVal < maxAddrVal)
	{
		VirtualQueryEx(processHandle, minAddr, &memBasicInfo, sizeof(MEMORY_BASIC_INFORMATION));

		//If the memory chunk is accessible
		if (memBasicInfo.Protect == PAGE_READWRITE && memBasicInfo.State == MEM_COMMIT)
		{
			range r;
			r.start = (ulong)memBasicInfo.BaseAddress;
			r.end = (ulong)memBasicInfo.BaseAddress + (ulong)memBasicInfo.RegionSize;
			output.push_back(r);
		}

		minAddrVal += (ulong)memBasicInfo.RegionSize;
		/*
		The seperate address values fixes the strange nature of the LPVOID pointer where it dosent act like a proper pointer
		It insted does something where I cant even predict where the next address should be, but casting it from a ulong seems to do the trick.
		Also the ulong is used for 2 reasons,
		1, when something is in the full 64 bit range, it dosent use 2^64/2 which is what it will use when dealing with negatives
		2, I am always counting up, so if it becomes low again, I know it has ended.
		*/
		//Checks if the min address is less than the previous min address. that always leads to loops and thats no good.
		//Call it a cheap bugfix, but it fixes the damn bug ⌐_⌐
		if (minAddrVal < previousMinAddr)
		{
			break;
		}

		previousMinAddr = minAddrVal;
		minAddr = (LPVOID)minAddrVal;
	}
	CloseHandle(processHandle);

	return output;
}

//Really simple script for really simple people (like me)
ulong totalLength(vector<range> pages)
{
	ulong totalLength = 0;
	for each(range ran in pages)
	{
		totalLength += ran.end - ran.start;
	}
	return totalLength;
}

ulong memoryMap(vector<range> pages, ulong position)
{

	if (pages.size())
	{
		ulong prevVal = 0;
		ulong val = 0;
		for (unsigned int i = 0; i < pages.size(); i++)
		{
			val += pages[i].end - pages[i].start;
			if (position <= val)
			{
				return pages[i].start + (position - prevVal);
			}
			else
			{
				prevVal = val;
			}
		}
	}
	return 0;
}

int main(void)
{
	vector<PROCESSENTRY32> processes = GetProcessList();
	for (unsigned int i = 0; i < processes.size(); i++)
	{
		cout << i << " " << processes[i].szExeFile << endl;
	}

	int input;
	cin >> input;

	vector<range> ranges = getValidAddresses(processes[input]);
	for each (range ran in ranges)
	{
		cout << right << setfill('0') << setw(8) << hex << ran.start << " - " << //Left
			right << setfill('0') << setw(8) << hex << ran.end << " " << //Right 
			endl;
	}

	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, processes[input].th32ProcessID);


	while (true)
	{
		ulong totLen = totalLength(ranges);
		SIZE_T output;
		int exitCode = 0;
		GetExitCodeProcess(processHandle, (LPDWORD)&exitCode);
		if (exitCode == 259)
		{
			//Gets a random value and mapps it to the correct memory address
			ulong uInput = randomLong(totLen);
			ulong mappedAddr = memoryMap(ranges, uInput);

			//Just too lazy to make a random char, so I used the random loang
			ulong first = (((randomLong(totLen)) / 1024)) * 1024;
			ulong last = ((randomLong(totLen)) / 1024) * 1024;

			//Swaps if first is greater than last
			if (last < first)
			{
				ulong temp = first;
				first = last;
				last = temp;
			}
			char val = randomLong();
			char changeVal = randomLong();
			ulong prevPrec = 0;
			cout << "First: " << hex << first << endl;
			cout << "Last : " << hex << last << endl;
			cout << dec << val << endl;
			cout << dec << 0 << "/" << last - first << " " << 0 << "%" << endl;
			ulong bytesChanged = 0;
			for (ulong i = first; i < last; i += 1024)
			{

				GetExitCodeProcess(processHandle, (LPDWORD)&exitCode);
				if (exitCode == 259)
				{

					ulong prec = ((i - first) * 100) / (last - first);

					if (prec > prevPrec)
					{
						cout << dec << i - first << "/" << last - first << " " << prec << "%" << endl;
						prevPrec = prec;
					}
					//Mapps address to valid address
					ulong addr = memoryMap(ranges, i);

					void* outVal[1024];

					//Reads byte in memory
					if (ReadProcessMemory(processHandle, (LPVOID)addr, outVal, 1024, &output))
					{
						for (ulong j = 0; j < output; j++)
						{
							char cChar = (char)outVal[j];
							//Checks if value is equal to val
							if (cChar == val)
							{
								//Writes byte to memory if successfull
								if ((bool)randomLong())
								{
									WriteProcessMemory(processHandle, (LPVOID)addr, (LPCVOID)&changeVal, 4, &output);
									cout << right << setfill('0') << setw(8) << hex << addr << " " << changeVal << endl;
									bytesChanged++;
								}
							}
							if (bytesChanged >= 100)
							{
								goto done;
							}
						}
					}
					else
					{
						cerr << "Error, could not read value at address " << hex << addr << endl << "Error code :" << dec << GetLastError() << endl;
					}
				}
				else
				{
					break;
				}
			}
			done:;
			cout << "Done. Bytes changed: " << dec << bytesChanged << endl;

			//Waits for user
			string wait;
			cin >> wait;
			Sleep(250);
		}
		else
		{
			cout << "Process ended: " << dec << exitCode << endl;
			string end;
			cin >> end;
			break;
		}
	}
	CloseHandle(processHandle);

	return 0;
}