// Copyright 2010 Mohamed Mansour. All rights reserved.
// Use of this source code is governed by a GPL license that can
// be found in the LICENSE file.

#include "haptics_device.h"

#include "windows.h"

#include <iostream>

namespace haptics {

HapticsDevice::HapticsDevice()
    : initialized_(false),
      button_servo_(false),
      device_handle_(HDL_INVALID_HANDLE),
      servo_callback_(HDL_INVALID_HANDLE) {
}

HapticsDevice::~HapticsDevice() {
  StopDevice();
}

void HapticsDevice::SendForce(double force[3]) {
  force_servo_[0] = force[0];
  force_servo_[1] = force[1];
  force_servo_[2] = force[2];
}

void HapticsDevice::StartDevice() {
  HDLError err = HDL_NO_ERROR;

  // Gets the default device handle from the hdal.ini file from the driver.
  device_handle_ = hdlInitNamedDevice((const char*)0);
  if (device_handle_ == HDL_INVALID_HANDLE) {
    std::cout << "Device Failure: Could not open device.";
  }
  CheckError("hdlInitNamedDevice");

  // Now that the device is fully initialized, start the servo thread.
  // Failing to do this will result in a non-funtional haptics application.
  hdlStart();
  CheckError("hdlStart");

  // Setup the callback function.
  servo_callback_ = hdlCreateServoOp(OnContactThunk, this, false);
  if (servo_callback_ == HDL_INVALID_HANDLE) {
    std::cout << "Device Failure: Invalid servo operation.";
  }
  CheckError("hdlCreateServoOp");

  // Make the device current. All subsequent calls will
  // be directed towards the current device.
  hdlMakeCurrent(device_handle_);
  CheckError("hdlMakeCurrent");

  // Get the extents of the device workspace.
  // Used to create the mapping between device and application coordinates.
  // Returned dimensions in the array are minx, miny, minz, maxx, maxy, maxz
  //                                      left, bottom, far, right, top, near)
  // Right-handed coordinates.
  //  left-right is the x-axis, right is greater than left 
  //  bottom-top is the y-axis, top is greater than bottom 
  //  near-far is the z-axis, near is greater than far 
  //  workspace center is (0,0,0)
  double haptic_workspace[6];
  hdlDeviceWorkspace(haptic_workspace);

  // Establish the transformation from device space to app space
  // To keep things simple, we will define the app space units as
  // inches, and set the workspace to approximate the physical
  // workspace of the Falcon.  That is, a 4" cube centered on the
  // origin.  Note the Z axis values; this has the effect of
  // moving the origin of world coordinates toward the base of the
  // unit.
  double game_workspace[] = {-2, -2, -2, 2, 2, 3};
  hdluGenerateHapticToAppWorkspaceTransform(haptic_workspace,
                                            game_workspace,
                                            true,
                                            transformation_matrix_);

  // Device initialized!
  initialized_ = true;
}

void HapticsDevice::StopDevice() {
  if (servo_callback_ != HDL_INVALID_HANDLE) {
    hdlDestroyServoOp(servo_callback_);
    servo_callback_ = HDL_INVALID_HANDLE;
  }
  hdlStop();

  if (device_handle_ != HDL_INVALID_HANDLE) {
    hdlUninitDevice(device_handle_);
    device_handle_ = HDL_INVALID_HANDLE;
  }

  initialized_ = false;
}

void HapticsDevice::SynchronizeClient() {
  hdlCreateServoOp(OnStateThunk, this, true);
}

bool HapticsDevice::IsButtonDown() {
  return button_;
}

void HapticsDevice::GetPosition(double pos[3]) {
  pos[0] = position_servo_[0];
  pos[1] = position_servo_[1];
  pos[2] = position_servo_[2];
}

void HapticsDevice::CheckError(const char* message) const {
  HDLError err = hdlGetError();
  if (err != HDL_NO_ERROR) {
    MessageBox(NULL, message, "HDAL ERROR", MB_OK);
    abort();
  }
}

HDLServoOpExitCode HapticsDevice::OnContact() {
  // Get current state of haptic device
  hdlToolPosition(position_servo_);
  hdlToolButton(&(button_servo_));

  // Send forces to device
  hdlSetToolForce(force_servo_);

  // Make sure to continue processing
  return HDL_SERVOOP_CONTINUE;
}

HDLServoOpExitCode HapticsDevice::OnState() {
  // Call JavaScript synchronization that copies data from servo side.

  button_ = button_servo_;

  // Only do this once.  The application will decide when it
  // wants to do it again, and call CreateServoOp with
  // bBlocking = true
  return HDL_SERVOOP_EXIT;
}

}  // namespace haptics
