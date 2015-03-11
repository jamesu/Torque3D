//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "platform/input/oculusVR/oculusVRSensorData.h"
#include "platform/input/oculusVR/oculusVRUtil.h"
#include "console/console.h"

OculusVRSensorData::OculusVRSensorData()
{
   reset();
}

void OculusVRSensorData::reset()
{
   mDataSet = false;
}
/*
typedef struct ovrPoseStatef_
{
    ovrPosef     ThePose;               ///< The body's position and orientation.
    ovrVector3f  AngularVelocity;       ///< The body's angular velocity in radians per second.
    ovrVector3f  LinearVelocity;        ///< The body's velocity in meters per second.
    ovrVector3f  AngularAcceleration;   ///< The body's angular acceleration in radians per second per second.
    ovrVector3f  LinearAcceleration;    ///< The body's acceleration in meters per second per second.
    double       TimeInSeconds;         ///< Absolute time of this state sample.
} ovrPoseStatef;

    /// Predicted head pose (and derivatives) at the requested absolute time.
    /// The look-ahead interval is equal to (HeadPose.TimeInSeconds - RawSensorData.TimeInSeconds).
    ovrPoseStatef  HeadPose;

    /// Current pose of the external camera (if present).
    /// This pose includes camera tilt (roll and pitch). For a leveled coordinate
    /// system use LeveledCameraPose.
    ovrPosef       CameraPose;

    /// Camera frame aligned with gravity.
    /// This value includes position and yaw of the camera, but not roll and pitch.
    /// It can be used as a reference point to render real-world objects in the correct location.
    ovrPosef       LeveledCameraPose;

    /// The most recent sensor data received from the HMD.
    ovrSensorData  RawSensorData;

    /// Tracking status described by ovrStatusBits.
    unsigned int   StatusFlags;

    //// 0.4.1

    // Measures the time from receiving the camera frame until vision CPU processing completes.
    double LastVisionProcessingTime;

    //// 0.4.3

    // Measures the time from exposure until the pose is available for the frame, including processing time.
    double LastVisionFrameLatency;

    /// Tag the vision processing results to a certain frame counter number.
    uint32_t LastCameraFrameCounter;

    ovrStatusBits


       
    ovrStatus_OrientationTracked    = 0x0001,   /// Orientation is currently tracked (connected and in use).
    ovrStatus_PositionTracked       = 0x0002,   /// Position is currently tracked (false if out of range).
    ovrStatus_CameraPoseTracked     = 0x0004,   /// Camera pose is currently tracked.
    ovrStatus_PositionConnected     = 0x0020,   /// Position tracking hardware is connected.
    ovrStatus_HmdConnected          = 0x0080    /// HMD Display is available and connected.
    */

void OculusVRSensorData::setData(ovrTrackingState& data, const F32& maxAxisRadius)
{
   // Sensor rotation & position
   OVR::Posef pose = data.HeadPose.ThePose;
   OVR::Quatf orientation = pose.Rotation;
   OVR::Vector3f position = data.HeadPose.ThePose.Position;

   mPosition = Point3F(position.x, position.y, position.z);

   OVR::Matrix4f orientMat(orientation);
   OculusVRUtil::convertRotation(orientMat.M, mRot);
   mRotQuat.set(mRot);

   // Sensor rotation in Euler format
   OculusVRUtil::convertRotation(orientation, mRotEuler); // mRotEuler == pitch, roll, yaw FROM yaw, pitch, roll

   //mRotEuler = EulerF(0,0,0);
   float hmdYaw, hmdPitch, hmdRoll;
        orientation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&hmdYaw, &hmdPitch, &hmdRoll);

   // Sensor rotation as axis
   OculusVRUtil::calculateAxisRotation(mRot, maxAxisRadius, mRotAxis);

   // Sensor raw values
   OVR::Vector3f accel = data.HeadPose.LinearAcceleration;
   OculusVRUtil::convertAcceleration(accel, mAcceleration);

   OVR::Vector3f angVel = data.HeadPose.AngularVelocity;
   OculusVRUtil::convertAngularVelocity(angVel, mAngVelocity);

   OVR::Vector3f mag = data.RawSensorData.Magnetometer;
   OculusVRUtil::convertMagnetometer(mag, mMagnetometer);

   mDataSet = true;
}

void OculusVRSensorData::simulateData(const F32& maxAxisRadius)
{
   // Sensor rotation
   mRot.identity();
   mRotQuat.identity();
   mRotEuler.zero();

   mPosition = Point3F(0,0,0);

   // Sensor rotation as axis
   OculusVRUtil::calculateAxisRotation(mRot, maxAxisRadius, mRotAxis);

   // Sensor raw values
   mAcceleration.zero();
   mAngVelocity.zero();
   mMagnetometer.zero();

   mDataSet = true;
}

U32 OculusVRSensorData::compare(OculusVRSensorData* other, bool doRawCompare)
{
   S32 result = DIFF_NONE;

   // Check rotation
   if(mRotEuler.x != other->mRotEuler.x || mRotEuler.y != other->mRotEuler.y || mRotEuler.z != other->mRotEuler.z || !mDataSet)
   {
      result |= DIFF_ROT;
   }

   // Check rotation as axis
   if(mRotAxis.x != other->mRotAxis.x || !mDataSet)
   {
      result |= DIFF_ROTAXISX;
   }
   if(mRotAxis.y != other->mRotAxis.y || !mDataSet)
   {
      result |= DIFF_ROTAXISY;
   }

   // Check raw values
   if(doRawCompare)
   {
      if(mAcceleration.x != other->mAcceleration.x || mAcceleration.y != other->mAcceleration.y || mAcceleration.z != other->mAcceleration.z || !mDataSet)
      {
         result |= DIFF_ACCEL;
      }
      if(mAngVelocity.x != other->mAngVelocity.x || mAngVelocity.y != other->mAngVelocity.y || mAngVelocity.z != other->mAngVelocity.z || !mDataSet)
      {
         result |= DIFF_ANGVEL;
      }
      if(mMagnetometer.x != other->mMagnetometer.x || mMagnetometer.y != other->mMagnetometer.y || mMagnetometer.z != other->mMagnetometer.z || !mDataSet)
      {
         result |= DIFF_MAG;
      }
   }

   return result;
}
