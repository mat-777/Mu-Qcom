/** @file
  MsPlatformDevicesLib  - Device specific library.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/AcpiPlatformUpdateLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MsPlatformDevicesLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/DevicePath.h>

#include <Configuration/BootDevices.h>

//
// Predefined platform default console device path
//
BDS_CONSOLE_CONNECT_ENTRY gPlatformConsoles[] =
{
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&KeypadDevicePath,
    CONSOLE_IN
  },
  {
    (EFI_DEVICE_PATH_PROTOCOL *)&DisplayDevicePath,
    CONSOLE_OUT | STD_ERROR
  },
  {
    NULL,
    0
  }
};

EFI_DEVICE_PATH_PROTOCOL *gPlatformConInDeviceList[] = {NULL};

/**
  Library function used to provide the platform SD Card device path
**/
EFI_DEVICE_PATH_PROTOCOL*
EFIAPI
GetSdCardDevicePath(VOID)
{
  return (EFI_DEVICE_PATH_PROTOCOL *)&SdcardDevicePath;
}

/**
  Library function used to determine if the DevicePath is a valid bootable 'USB'
  device. USB here indicates the port connection type not the device protocol.
  With TBT or USB4 support PCIe storage devices are valid 'USB' boot options.
**/
BOOLEAN
EFIAPI
PlatformIsDevicePathUsb(IN EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
  EFI_DEVICE_PATH_PROTOCOL *Node;

  for (Node = DevicePath; !IsDevicePathEnd(Node); Node = NextDevicePathNode(Node)) {
    if ((DevicePathType(Node) == MESSAGING_DEVICE_PATH) && ((DevicePathSubType(Node) == MSG_USB_CLASS_DP) || (DevicePathSubType(Node) == MSG_USB_WWID_DP) || (DevicePathSubType(Node) == MSG_USB_DP))) {
      return TRUE;
    }
  }

  return FALSE;
}

// Dummy function needed for event notification callback
STATIC
VOID
DummyNotify(IN EFI_EVENT Event, IN VOID *Context) {}

/**
  Library function used to provide the list of platform devices that MUST be
  connected at the beginning of BDS
**/
EFI_DEVICE_PATH_PROTOCOL**
EFIAPI
GetPlatformConnectList(VOID)
{
  EFI_STATUS Status;
  EFI_EVENT  UsbConfigEvt;
  EFI_GUID   InitUsbControllerGuid = {0x1c0cffce, 0xfc8d, 0x4e44, {0x8c, 0x78, 0x9c, 0x9e, 0x5b, 0x53, 0xd, 0x36}};

  // Update the ACPI tables with SOC runtime information
  PlatformUpdateAcpiTables();

  // Notify the USB controller to start now
  Status = gBS->CreateEventEx(EVT_NOTIFY_SIGNAL, TPL_CALLBACK, DummyNotify, NULL, &InitUsbControllerGuid, &UsbConfigEvt);
  if (EFI_ERROR(Status)) {
    DEBUG((EFI_D_ERROR, "Usb controller init event not signaled: %r\n", Status));
  } else {
    gBS->SignalEvent(UsbConfigEvt);
    gBS->CloseEvent(UsbConfigEvt);
  }

  // We do not have any device to advertise for connection before BDS begins
  return NULL;
}

/**
  Library function used to provide the list of platform console devices.
**/
BDS_CONSOLE_CONNECT_ENTRY*
EFIAPI
GetPlatformConsoleList(VOID)
{
  return (BDS_CONSOLE_CONNECT_ENTRY *)&gPlatformConsoles;
}

/**
  Library function used to provide the list of platform devices that MUST be
  connected to support ConsoleIn activity.  This call occurs on the ConIn connect
  event, and allows platforms to do specific enablement for ConsoleIn support.
**/
EFI_DEVICE_PATH_PROTOCOL**
EFIAPI
GetPlatformConnectOnConInList(VOID)
{
  return (EFI_DEVICE_PATH_PROTOCOL **)&gPlatformConInDeviceList;
}

/**
  Library function used to provide the console type.  For ConType == DisplayPath,
  device path is filled in to the exact controller to use.  For other ConTypes,
  DisplayPath must NULL. The device path must NOT be freed.
**/
EFI_HANDLE
EFIAPI
GetPlatformPreferredConsole(OUT EFI_DEVICE_PATH_PROTOCOL **DevicePath)
{
  EFI_STATUS                Status;
  EFI_HANDLE                Handle = NULL;
  EFI_DEVICE_PATH_PROTOCOL *TempDevicePath;

  TempDevicePath = (EFI_DEVICE_PATH_PROTOCOL *)&DisplayDevicePath;

  Status = gBS->LocateDevicePath(&gEfiGraphicsOutputProtocolGuid, &TempDevicePath, &Handle);
  if (EFI_ERROR(Status) && !IsDevicePathEnd(TempDevicePath)) {
    DEBUG ((EFI_D_ERROR, "%a - Unable to locate platform preferred console. Code=%r\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
  }

  if (Handle != NULL) {
    //
    // Connect the GOP driver
    //
    gBS->ConnectController(Handle, NULL, NULL, TRUE);

    //
    // Get the GOP device path
    // NOTE: We may get a device path that contains Controller node in it.
    //
    TempDevicePath = EfiBootManagerGetGopDevicePath(Handle);
    *DevicePath    = TempDevicePath;
  }

  return Handle;
}