#include <iostream>
#include <fstream>

int amountDigitsDeterminer(int number);
char* intToASCII(int number);
int getCharArrayLength(const char* array);
char* createLogMsg(int computeTime, int waitTime);

int main()
{
	std::ofstream fileOut("log.txt");
	if (!fileOut.is_open()) {
		std::cerr << "Error opening file";
		exit(-1);
		}

	int computeTime = 2456;
	int waitTime = 123;
	char* logMsg = createLogMsg(computeTime, waitTime);
	fileOut << logMsg;
	fileOut.close();
	delete[] logMsg;
	return 0;
}

int amountDigitsDeterminer(int number)
{
	if (number == 0)
	{
		return 1;
	}
	
	int amountDigits = 0;
	while (number != 0)
	{
		number /= 10;
		amountDigits++;
	}
	return amountDigits;
}

char* intToASCII(int number)
{
    const int amountDigits = amountDigitsDeterminer(number);
    char* myASCIIchar = new char[amountDigits + 1]; // +1 for null terminator
    for (int i = amountDigits - 1; i >= 0; i--)
    {
        myASCIIchar[i] = static_cast<char>((number % 10) + '0');
        number /= 10;
    }
    myASCIIchar[amountDigits] = '\0';
    return myASCIIchar;
}

int getCharArrayLength(const char* array)
{
    int length = 0;
    while (array[length] != '\0')
    {
        length++;
    }
    return length;
}

char* createLogMsg(int computeTime, int waitTime)
{
    char* computeChar = intToASCII(computeTime);
    int computeCharLength = getCharArrayLength(computeChar);    
    char* waitChar = intToASCII(waitTime);
    int waitCharLength = getCharArrayLength(waitChar);
    
    // Allocate enough space for both strings, a tab, a newline, and the null terminator
    char* logMsg = new char[computeCharLength + waitCharLength + 3];

	// Combine both char arrays into log message
    for (int i = 0; i < computeCharLength; i++)
    {
        logMsg[i] = computeChar[i];
    }
    logMsg[computeCharLength] = '\t';
    for (int i = 0; i < waitCharLength; i++)
    {
        logMsg[computeCharLength + 1 + i] = waitChar[i];
    }
    logMsg[computeCharLength + 1 + waitCharLength] = '\n';
    logMsg[computeCharLength + 1 + waitCharLength + 1] = '\0';
    
    delete[] computeChar;
    delete[] waitChar;
    return logMsg;
}