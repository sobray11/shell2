//
//  main.cpp
//  ProcessShell
//
//  Created by Scott Obray on 2/13/17.
//  Copyright © 2017 Scott Obray. All rights reserved.
//

#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <algorithm>


std::chrono::duration<double> timeSpent;
double totalTime = 0.0;
std::vector<std::string> commandHistory;

std::string inFile;
std::string outFile;
bool inFilebool = false;
bool outFilebool = false;


void executeCommand(std::vector<char*> &args);
void parseLine(std::string line, std::vector<std::string> &argsString);


void ptime()
{
    std::cout << "Time spent executing child processes: "
    << std::chrono::duration_cast<std::chrono::seconds>(timeSpent).count()
    << " seconds, " << std::chrono::duration_cast<std::chrono::milliseconds>(timeSpent).count()
    << " miliseconds, and " << std::chrono::duration_cast<std::chrono::microseconds>(timeSpent).count()
    << " microscends." << std::endl;
}

void history(std::string temp)
{
    if (!temp.compare("history"))
    {
        for (int i = 0; i < (int)commandHistory.size(); i++)
        {
            std::cout << commandHistory[i] << std::endl;
        }
        return;
    }
    
    std::string historyNumber;
    std::vector<std::string> argsString;
    std::vector<char*> args;
    
    //Parse hsitroy command to single number
    for (int i = 2; i < (int)temp.size(); i++)
    {
        historyNumber.push_back(temp[i]);
    }
    int value = atoi(historyNumber.c_str());
    if (value > (int)commandHistory.size())
    {
        std::cout << "Not enough entries in history list.\n";
        return;
    }
    
    parseLine(commandHistory[value], argsString);

    //convert argsstring to char *
    for (int i = 0; i < (int) argsString.size(); i++)
    {
        args.push_back((char*)argsString[i].c_str());
    }
    executeCommand(args);
}

void executeCommand(std::vector<char*> &args)
{
    auto start = std::chrono::steady_clock::now();
    auto pid = fork();
    if (pid < 0)
    {
        perror("error");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        execvp(args[0], args.data());
        perror("error");
        exit(EXIT_FAILURE);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        auto end = std::chrono::steady_clock::now();
        timeSpent = end - start;
        totalTime += timeSpent.count();
    }
}

void parseLine(std::string line, std::vector<std::string> &argsString)
{
    auto cur = line.begin();
    while (cur!= line.end())
    {
        auto next = std::find(cur, line.end(), ' ');
        argsString.emplace_back(cur, next);
        cur = std::find(cur, line.end(), ' ');
        if (cur == line.end())
        {
            break;
        }
        ++cur;
    }
}

void findCommands(std::string inputLine, std::vector<std::string> &commands)
{
    commands.clear();
    std::vector<char> key {'|','<','>'};
    
    auto startPosition = inputLine.begin();
    while (startPosition != inputLine.end())
    {
        auto endPosition = std::find_first_of(startPosition, inputLine.end(), key.begin(), key.end());
        
        //If there is an input file, set filename and skip over name
        if (inputLine[std::distance(inputLine.begin(), endPosition)] == '<')
        {
            inFilebool = true;
            commands.emplace_back(startPosition, endPosition);
            commandHistory.emplace_back(startPosition, endPosition);
            auto endFileNamePos = std::find_first_of(endPosition + 1, inputLine.end(), key.begin(), key.end());
            inFile = inputLine.substr(std::distance(inputLine.begin(),endPosition + 2), std::distance(endPosition + 2,endFileNamePos));
            if (inFile[inFile.size()-1] == ' ')
                inFile.pop_back();
            endPosition = endFileNamePos;
            
        }
        //If there is and output file, set output name and skip over
        else if (inputLine[std::distance(inputLine.begin(), endPosition)] == '>')
        {
            outFilebool = true;
            commands.emplace_back(startPosition, endPosition);
            commandHistory.emplace_back(startPosition, endPosition);
            auto endFileNamePos = std::find_first_of(endPosition + 1, inputLine.end(), key.begin(), key.end());
            outFile = inputLine.substr(std::distance(inputLine.begin(),endPosition+2), std::distance(endPosition + 2,endFileNamePos));
            if (outFile[outFile.size()-1] == ' ')
                outFile.pop_back();
            endPosition = endFileNamePos;
        }
        //Otherwise add entire command to commands vector
        else
        {
            commands.emplace_back(startPosition, endPosition);
            commandHistory.emplace_back(startPosition, endPosition);
        }
        //Increment past next delimeter
        startPosition = endPosition;
        if (startPosition == inputLine.end())
            break;
        ++startPosition;
        if (startPosition == inputLine.end())
            break;
        ++startPosition;
        
    }
//        std::cout << "Commands\n";
//        for (size_t i = 0; i < commands.size(); i++)
//        {
//            std::cout << commands[i] << std::endl;
//        }
//        std::cout << "Infile: " << inFile << ". \noutfile: " << outFile << "." << std::endl;
    
}

void pipeCommands(std::vector<std::string> &commands)
{
    int oldP[2];
    int newP[2];
    pipe (oldP);
    for (size_t i = 0; i < commands.size(); i++)
    {
        pipe(newP);
        auto pid = fork();
        if (pid < 0)
        {
            perror("error");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) //If child
        {
            dup2(oldP[0], 0);
            close(oldP[0]);
            close(oldP[1]);
            //if there is another command
            if (i + 1 < commands.size())
            {
                close(newP[0]);
                dup2(newP[1], 1);
                close(newP[1]);
                
            }
            std::vector<char*> args;
            std::vector<std::string> argsString;
            parseLine(commands[0], argsString);
            //Convert argString vector to char* vector
            for (int i = 0; i < (int) argsString.size(); i++)
            {
                args.push_back((char*)argsString[i].c_str());
            }
            execvp(args[0], args.data());
            perror("error");
            exit(EXIT_FAILURE);
        }
        else //not child
        {
            close(oldP[0]);
            close(oldP[1]);
            //If there is another command
            if (i + 1 < commands.size())
            {
                oldP[0] = newP[0];
                oldP[1] = newP[1];
            }
            int status;
            waitpid(pid, &status, 0);
        }
        close (oldP[0]);
        close (oldP[1]);
    }
   

}

int main()
{
    
    while(true)
    {
        std::string inputLine;
        std::vector<std::string> commands;
        std::cout << "[cmd]: ";
        std::getline(std::cin, inputLine);
        findCommands(inputLine, commands);

        
        if (!inputLine.compare("exit"))
        {
            std::cout << "Exiting Program\n";
            return 0;
        }
        else if (!inputLine.compare("ptime"))
        {
            ptime();
        }
        else if (!inputLine.compare("history") || inputLine[0] == '^')
        {
            history(inputLine);
        }
        else if (commands.size() == 1)
        {
            std::vector<char*> args;
            std::vector<std::string> argsString;
            parseLine(commands[0], argsString);
            //Convert argString vector to char* vector
            for (int i = 0; i < (int) argsString.size(); i++)
            {
                args.push_back((char*)argsString[i].c_str());
            }
            executeCommand(args);
        }
        else
        {
            pipeCommands(commands);
        }
    }
    
    
    
    
//        int p[2];
//        pipe(p);
//        
//        std::vector<char*> args;
//        std::vector<std::string> argsString;
//        
//        parseLine(commands[0], argsString);
//    
//        for (int i = 0; i < (int) argsString.size(); i++)
//        {
//            args.push_back((char*)argsString[i].c_str());
//        }
//    
//        auto pid = fork();
//        if (pid < 0)
//        {
//            perror("error");
//            exit(EXIT_FAILURE);
//        }
//        if (pid == 0)
//        {
//            dup2(p[1], STDOUT_FILENO);
//            close(p[0]);
//            execvp(args[0], args.data());
//            perror("error");
//            exit(EXIT_FAILURE);
//        }
//        
//        std::vector<char*> args2;
//        std::vector<std::string> argsString2;
//        
//        parseLine(commands[1], argsString2);
//        
//        for (int i = 0; i < (int) argsString2.size(); i++)
//        {
//            args2.push_back((char*)argsString2[i].c_str());
//        }
//        
//        pid = fork();
//        if (pid < 0)
//        {
//            perror("error");
//            exit(EXIT_FAILURE);
//        }
//        if (pid == 0)
//        {
//            dup2(p[0], STDIN_FILENO);
//            close(p[1]);
//            execvp(args2[0], args2.data());
//            perror("error");
//            exit(EXIT_FAILURE);
//        }
//        else
//        {
//            int status;
//            waitpid(pid, &status, 0);
//        }
//    }

    //findCommands("A < infilename | B | C | D > outfilename");
    
//    while (running)
//    {
//        std::string line;
//        std::vector<std::string> argsString;
//        std::vector<char*> args;
//        
//        std::cout << "[cmd]: ";
//        std::getline(std::cin, line);
//        
//        commandHistory.push_back(line);
//        
//        if (!line.compare("exit"))
//        {
//            running = false;
//            std::cout << "Exiting Program\n";
//            return 0;
//        }
//        else if (!line.compare("ptime"))
//        {
//            ptime();
//        }
//        else if (!line.compare("history") || line[0] == '^')
//        {
//            history(line);
//        }
//        else
//        {
//            parseLine(line, argsString);
//            //Convert argString vector to char* vector
//            for (int i = 0; i < (int) argsString.size(); i++)
//            {
//                args.push_back((char*)argsString[i].c_str());
//            }
//            executeCommand(args);
//        }
//    }
}
