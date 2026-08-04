#include "settings.h"
#include "adl.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace std;

struct DisplayLocation {
    int adapterIndex;
    int displayIndex;
};

#pragma region setvcp_command

#define SETWRITESIZE 8
#define SET_VCPCODE_SUBADDRESS 1
#define SET_VCPCODE_OFFSET 4
#define SET_HIGH_OFFSET 5
#define SET_LOW_OFFSET 6
#define SET_CHK_OFFSET 7
#define VCP_CODE_SWITCH_INPUT 0xF4

unsigned char ucSetCommandWrite[SETWRITESIZE] = {
    0x6e, 0x51, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00
};

int vWriteI2c(char* sendBuffer, int sendLength, int adapterIndex, int displayIndex)
{
    int receiveLength = 0;
    return adlprocs.ADL_Display_DDCBlockAccess_Get(
        adapterIndex, displayIndex, 0, 0,
        sendLength, sendBuffer, &receiveLength, nullptr);
}

void vSetVcpCommand(
    unsigned int subaddress,
    unsigned char vcpCode,
    unsigned int value,
    int adapterIndex,
    int displayIndex)
{
    unsigned char checksum = 0;

    ucSetCommandWrite[SET_VCPCODE_SUBADDRESS] =
        static_cast<unsigned char>(subaddress);
    ucSetCommandWrite[SET_VCPCODE_OFFSET] = vcpCode;
    ucSetCommandWrite[SET_LOW_OFFSET] =
        static_cast<unsigned char>(value & 0xFF);
    ucSetCommandWrite[SET_HIGH_OFFSET] =
        static_cast<unsigned char>((value >> 8) & 0xFF);

    for (int i = 0; i < SET_CHK_OFFSET; i++) {
        checksum ^= ucSetCommandWrite[i];
    }

    ucSetCommandWrite[SET_CHK_OFFSET] = checksum;

    int result = vWriteI2c(
        reinterpret_cast<char*>(ucSetCommandWrite),
        SETWRITESIZE,
        adapterIndex,
        displayIndex);

    if (result != ADL_OK) {
        cerr << "Error: DDC write failed with ADL code "
             << result << endl;
    }

    Sleep(250);
}

#pragma endregion

#pragma region edid_and_detection

unsigned int get_edid_serial(int adapterIndex, int displayIndex)
{
    ADLDisplayEDIDData edidData = {};
    edidData.iSize = sizeof(ADLDisplayEDIDData);

    int result = adlprocs.ADL_Display_EdidData_Get(
        adapterIndex, displayIndex, &edidData);

    if (result != ADL_OK) {
        return 0;
    }

    const unsigned char* edid =
        reinterpret_cast<const unsigned char*>(edidData.cEDIDData);

    return static_cast<unsigned int>(edid[12]) |
        (static_cast<unsigned int>(edid[13]) << 8) |
        (static_cast<unsigned int>(edid[14]) << 16) |
        (static_cast<unsigned int>(edid[15]) << 24);
}

bool find_display_by_serial(
    unsigned int targetSerial,
    DisplayLocation& location)
{
    int numberAdapters = 0;

    if (adlprocs.ADL_Adapter_NumberOfAdapters_Get(&numberAdapters) != ADL_OK ||
        numberAdapters <= 0)
    {
        return false;
    }

    LPAdapterInfo adapterInfo = static_cast<LPAdapterInfo>(
        malloc(sizeof(AdapterInfo) * numberAdapters));

    if (adapterInfo == nullptr) {
        return false;
    }

    memset(adapterInfo, 0, sizeof(AdapterInfo) * numberAdapters);

    if (adlprocs.ADL_Adapter_AdapterInfo_Get(
        adapterInfo,
        sizeof(AdapterInfo) * numberAdapters) != ADL_OK)
    {
        free(adapterInfo);
        return false;
    }

    for (int i = 0; i < numberAdapters; i++) {
        int adapterIndex = adapterInfo[i].iAdapterIndex;
        int numberDisplays = 0;
        LPADLDisplayInfo displayInfo = nullptr;

        int result = adlprocs.ADL_Display_DisplayInfo_Get(
            adapterIndex, &numberDisplays, &displayInfo, 0);

        if (result != ADL_OK || displayInfo == nullptr) {
            continue;
        }

        for (int j = 0; j < numberDisplays; j++) {
            const int requiredFlags =
                ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED |
                ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED;

            if ((displayInfo[j].iDisplayInfoValue & requiredFlags) != requiredFlags) {
                continue;
            }

            if (adapterIndex !=
                displayInfo[j].displayID.iDisplayLogicalAdapterIndex)
            {
                continue;
            }

            int displayIndex =
                displayInfo[j].displayID.iDisplayLogicalIndex;

            unsigned int serial =
                get_edid_serial(adapterIndex, displayIndex);

            if (serial == targetSerial) {
                location.adapterIndex = adapterIndex;
                location.displayIndex = displayIndex;

                ADL_Main_Memory_Free(
                    reinterpret_cast<void**>(&displayInfo));
                free(adapterInfo);
                return true;
            }
        }

        ADL_Main_Memory_Free(
            reinterpret_cast<void**>(&displayInfo));
    }

    free(adapterInfo);
    return false;
}

void print_devices()
{
    int numberAdapters = 0;
    adlprocs.ADL_Adapter_NumberOfAdapters_Get(&numberAdapters);

    if (numberAdapters <= 0) {
        cerr << "No AMD display devices found!" << endl;
        return;
    }

    lpAdapterInfo = static_cast<LPAdapterInfo>(
        malloc(sizeof(AdapterInfo) * numberAdapters));

    memset(lpAdapterInfo, 0, sizeof(AdapterInfo) * numberAdapters);

    adlprocs.ADL_Adapter_AdapterInfo_Get(
        lpAdapterInfo,
        sizeof(AdapterInfo) * numberAdapters);

    for (int i = 0; i < numberAdapters; i++) {
        int adapterIndex = lpAdapterInfo[i].iAdapterIndex;
        int numberDisplays = 0;

        ADL_Main_Memory_Free(
            reinterpret_cast<void**>(&lpAdlDisplayInfo));

        int result = adlprocs.ADL_Display_DisplayInfo_Get(
            adapterIndex,
            &numberDisplays,
            &lpAdlDisplayInfo,
            0);

        cout << "Adapter Index: " << adapterIndex
             << " Adapter Name: "
             << lpAdapterInfo[i].strAdapterName
             << endl;

        if (result != ADL_OK || lpAdlDisplayInfo == nullptr) {
            continue;
        }

        for (int j = 0; j < numberDisplays; j++) {
            const int requiredFlags =
                ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED |
                ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED;

            if ((lpAdlDisplayInfo[j].iDisplayInfoValue & requiredFlags) != requiredFlags) {
                continue;
            }

            if (adapterIndex !=
                lpAdlDisplayInfo[j].displayID.iDisplayLogicalAdapterIndex)
            {
                continue;
            }

            int displayIndex =
                lpAdlDisplayInfo[j].displayID.iDisplayLogicalIndex;

            cout << "\tDisplay Index : " << displayIndex
                 << " Display Name : "
                 << lpAdlDisplayInfo[j].strDisplayName
                 << " EDID Serial : "
                 << get_edid_serial(adapterIndex, displayIndex)
                 << endl;
        }
    }
}

#pragma endregion

int main(int argc, const char* argv[])
{
    if (!InitADL()) {
        return 1;
    }

    Settings settings;

    try {
        settings = parse_settings(argc, argv);
    }
    catch (const runtime_error& error) {
        cerr << "Error: " << error.what() << endl << endl;
        print_help();
        FreeADL();
        return 1;
    }

    if (settings.help) {
        print_help();
        FreeADL();
        return 0;
    }

    int exitCode = 0;

    switch (settings.command) {
    case detect:
        print_devices();
        break;

    case setvcp:
        vSetVcpCommand(
            settings.i2c_subaddress,
            VCP_CODE_SWITCH_INPUT,
            settings.input,
            settings.monitor,
            settings.display);
        break;

    case setvcpserial:
    {
        DisplayLocation location = {};

        if (!find_display_by_serial(settings.serial, location)) {
            cerr << "Error: no active display found with EDID serial "
                 << settings.serial << endl;
            exitCode = 1;
            break;
        }

        if (settings.verbose) {
            cerr << "Matched EDID serial " << dec << settings.serial
                 << " to adapter " << location.adapterIndex
                 << ", display " << location.displayIndex
                 << endl;
        }

        vSetVcpCommand(
            settings.i2c_subaddress,
            VCP_CODE_SWITCH_INPUT,
            settings.input,
            location.adapterIndex,
            location.displayIndex);
        break;
    }

    default:
        print_help();
        exitCode = 1;
        break;
    }

    FreeADL();
    return exitCode;
}
