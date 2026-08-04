#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <unordered_map>

#define ENUM_TO_STRING(value) #value;

enum Command {
    detect,
    setvcp,
    setvcpserial,
    unknown
};

struct Settings {
    bool help{ false };
    bool verbose{ false };
    Command command = unknown;
    unsigned int i2c_subaddress{ 0x51 };
    unsigned int input{ 0 };
    unsigned int monitor{ 0 };
    unsigned int display{ 0 };
    unsigned int serial{ 0 };
};

static const std::unordered_map<Command, const char*> command_to_string{
    {detect, "detect"},
    {setvcp, "setvcp"},
    {setvcpserial, "setvcpserial"}
};

Settings parse_settings(int, const char**);
void print_help();

#endif // !SETTINGS_H