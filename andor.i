/*
 * andor.i --
 *
 * Interface to Andor cameras for Yorick.
 *
 *-----------------------------------------------------------------------------
 *
 * This file is part of `YAndor` which is licensed under the MIT "Expat"
 * License.
 *
 * Copyright (C) 2014 Éric Thiébaut <https://github.com/emmt/YAndor>
 */

/* FIXME: not yet implemented:
   AT_GetEnumStringByIndex
*/

if (is_func(plug_in)) plug_in, "yandor";

local andor_intro;
/* DOCUMENT Support for Andor cameras in Yorick

     YAndor is a Yorick plugin to use Andor Tech. cameras.  Most of the
     routines from Andor SDK3 library have their counterpart provided by this
     plugin (see andor_equiv for a list of equivalence between the SDK
     functions and the Yorick functions).

     To get access to a camera, you must open the corresponding device:

        cam = andor_open(dev);

     where DEV is the device number, from 0 to N-1 with N the number of
     devices which can be queried by andor_count_devices().

     Then you can query the size of the sensor, etc.:

        width = andor_get_int(cam, "SensorWidth");
        height = andor_get_int(cam, "SensorHeight");

     Beware that the plugin raises an error as soon as a routine reports a
     failure.  So you probably want to check whether a feature is implemented
     with andor_is_implemented:

        feature = "AOI Width";
        if (! andor_is_implemented(cam, feature)) feature = "Sensor Width";
        width = andor_get_int(cam, feature);

     There are no needs to "initialize" (AT_InitialiseLibrary) or "finalize"
     (AT_FinaliseLibrary) Andor library, the plugin takes care of that.
     Similarly the plugin takes care of closing a camera handle, so there is
     no counterpart to AT_Close.

     For image(s) acquisition, you must first set the length of the queue of
     frame buffers, then start the acquisition, wait for image(s) and finally
     stop the acquisition.  For instance:

         andor_set_queue_length, cam, 20;
         andor_start_acquisition, cam;
         for (t = 1; t <= 100; ++t) {
            img = andor_wait_image(cam, -1);
            fma;
            pli, img;
            pause, 100;
         }
         andor_stop_acquisition, cam;

     When acquisition is started, the plugin takes care of allocating and
     queuing the frame buffers as well as configuring the camera in continous
     acquisition mode.

     The functions andor_get_single and andor_get_sequence are drivers which
     can be used to simply get a single image or a sequence of images.

   SEE ALSO andor_equiv, andor_open.
*/

local andor_equiv;
/* DOCUMENT Equivalent names of routines in YAndor plugin.

     To follow Yorick coding conventions, the names are prefixed by andor_"
     (or "ANDOR_") with an underscore to separate words, routines names are in
     lower case letters while constant names are in uppercase letters.  For
     instance, "AT_Open" becomes "andor_open" (see andor_equiv for all these
     names).  Note that not all routines from the SDK have an exact
     counterpart in Yorick (in part due to the way acquisition of images is
     implemented under the hood).

     --------------------------------------------------------------
     Andor Tech. SDK              Corresponding Yorick function
     --------------------------------------------------------------
     AT_InitialiseLibrary         (automatically done)
     AT_FinaliseLibrary           (automatically done)
     AT_Open                      andor_open
     AT_Close                     (automatically done)
     AT_RegisterFeatureCallback   -
     AT_UnregisterFeatureCallback -
     AT_IsImplemented             andor_is_implemented
     AT_IsReadOnly                andor_is_read_only
     AT_IsReadable                andor_is_readable
     AT_IsWritable                andor_is_writable
     AT_SetInt                    andor_set_int
     AT_GetInt                    andor_get_int
     AT_GetIntMin                 andor_get_int_min
     AT_GetIntMax                 andor_get_int_max
     AT_SetFloat                  andor_set_float
     AT_GetFloat                  andor_get_float
     AT_GetFloatMin               andor_get_float_min
     AT_GetFloatMax               andor_get_float_max
     AT_SetBool                   andor_set_bool
     AT_SetEnumIndex              andor_set_enum_index
     AT_SetEnumString             andor_set_enum_string
     AT_GetEnumIndex              andor_get_enum_index
     AT_GetEnumCount              andor_get_enum_count
     AT_IsEnumIndexAvailable      andor_is_enum_index_available
     AT_IsEnumIndexImplemented    andor_is_enum_index_implemented
     AT_GetEnumStringByIndex      andor_get_enum_string_by_index
     AT_Command                   andor_command,
                                  andor_start_acquisition,
                                  andor_stop_acquisition
     AT_SetString                 andor_set_string
     AT_MaxStringLength           -
     AT_QueueBuffer               andor_set_queue_length
     AT_WaitBuffer                andor_wait_image
     AT_Flush                     (automatically done)
     --------------------------------------------------------------

     SEE ALSO andor_intro.
 */

/*---------------------------------------------------------------------------*/
/* BUILTIN FUNCTIONS */

extern andor_count_devices;
extern andor_list_devices;
/* DOCUMENT n = andor_count_devices();
         or list = andor_list_devices();

     Get the number of Andor devices or a list of the available devices.

   SEE ALSO: andor_intro, andor_open,
             AT_Open(), AT_Close().
 */

extern andor_open;
/* DOCUMENT cam = andor_open(dev);

     This function returns an instance of an Andor camera.  DEV is the device
     number, from 0 to N-1 with N = andor_count_devices().  There are no needs
     to "close" the camera as Yorick will take care of releasing ressources
     when CAM is no longer in use.

     The returned instance can be used as CAM.MEMBER to query some internal
     parameters:

     cam.acquiring -----------> Camera is acquiring?
     cam.device --------------> The device index.
     cam.queue_length --------> The number of frames in the queue.
     cam.buffer --------------> The address of the queue buffer
                                (*USE WITH CARE*).
     cam.buffer_size ---------> The number of bytes in the queue buffer.
     cam.frame_size ----------> The frame size in bytes.
     cam.frame_width ---------> The frame width in (super-)pixels.
     cam.frame_height --------> The frame height in (super-)pixels.
     cam.row_stride ----------> The size of one row in the image in bytes.

     Note that many of these members only have a meaningful value when
     acqusition is started.

   SEE ALSO: andor_intro, andor_count_devices,
             AT_Open(), AT_Close().
 */

extern andor_set_queue_length;
extern andor_start_acquisition;
extern andor_stop_acquisition;
extern andor_wait_image;
/* DOCUMENT andor_set_queue_length, cam, len;
         or andor_start_acquisition, cam;
         or andor_stop_acquisition, cam;
         or img = andor_wait_image(cam, timeout);

      The subroutine andor_set_queue_length sets the length of the queue of
      frame buffers to be used for acquisition with camera CAM.  LEN is the
      new length of the queue and must be greater or equal 1.  The length of
      the queue cannot be set while camera is acquiring.  To query the current
      queue length:

         cam.queue_length;

      The subroutines andor_start_acquisition and andor_stop_acquisition start
      and stop the acquisition.  Acquisition is automatically stopped (and
      frame buffers flushed and deleted) when the camera is destroyed.  The
      length of the queue of frame buffers must be set before acquisition is
      started.  The acquisition mode is continous to cycle across the queue of
      buffers.

      The function andor_wait_image waits for next frame acquisition and
      returns the result as a Yorick array (whose type depends on the pixel
      format of the camera).  The TIMEOUT parameter can be specified to
      indicate how long in milliseconds you wish to wait for the next
      available image.  If TIMEOUT is strictly less than zero, the function
      will wait until a frame is ready.  If TIMEOUT is zero, the function will
      return immediately.  If no frame is ready after the delay, an empty
      result is returned.

   SEE ALSO:
 */

extern andor_command;
/* DOCUMENT andor_command, cam, name;

     Execute command NAME for camera CAM.  Note that some commands, notably
     "AcquisitionStart" and "AcquisitionStop" which are identical as calling
     andor_start_acquisition and andor_stop_acquisition respectively, perform
     more things than the SDK.  See andor_start_acquisition,
     andor_stop_acquisition for more information.

   SEE ALSO: andor_intro, andor_start_acquisition, andor_stop_acquisition,
             andor_open,
             AT_GetInt(), AT_GetIntMin(), AT_GetIntMax(), AT_SetInt(),
             AT_GetFloat(), AT_GetFloatMin(), AT_GetFloatMax(), AT_SetFloat().
 */

local ANDOR_SYSTEM;
extern andor_get_int;
extern andor_get_int_min;
extern andor_get_int_max;
extern andor_set_int;
extern andor_get_float
extern andor_get_float_min;
extern andor_get_float_max;
extern andor_set_float;
/* DOCUMENT andor_get_int(cam, prop);
         or andor_get_int_min(cam, prop);
         or andor_get_int_max(cam, prop);
         or andor_set_int, cam, prop, value;
         or andor_get_float(cam, prop);
         or andor_get_float_min(cam, prop);
         or andor_get_float_max(cam, prop);
         or andor_set_float, cam, prop, value;

     Query/set the value of numerical feature PROP for Andor camera CAM.  The
     special value ANDOR_SYSTEM can be used for CAM to access features not
     related to a specific camera but which are global properties.

     Note that an "int" (resp. a "float") for the Andor SDK library correspond
     to a "long" (resp. a "double") in Yorick.

   SEE ALSO: andor_intro, andor_is_implemented, andor_open,
             andor_get_bool, andor_get_string, andor_get_enum_index,
             AT_GetInt(), AT_GetIntMin(), AT_GetIntMax(), AT_SetInt(),
             AT_GetFloat(), AT_GetFloatMin(), AT_GetFloatMax(), AT_SetFloat().
 */
ANDOR_SYSTEM = [];

extern andor_is_implemented;
extern andor_is_read_only;
extern andor_is_readable;
extern andor_is_writable;
/* DOCUMENT andor_is_implemented(cam, prop);
         or andor_is(cam, prop);
     Returns whether feature PROP is implemented / read-only / readable /
     writable for camera CAM.
   SEE ALSO: andor_intro, andor_get_int,
             AT_IsImplemented(), AT_IsReadable(), AT_IsReadOnly(),
             AT_IsWritable().
 */

extern andor_get_bool;
extern andor_set_bool;
/* DOCUMENT andor_get_bool(cam, prop);
         or andor_set_bool, cam, prop, value;
     Query/set the value of boolean feature PROP for Andor camera CAM.  The
     special value ANDOR_SYSTEM can be used for CAM to access features not
     related to a specific camera but which are global properties.

   SEE ALSO: andor_intro, andor_is_implemented, andor_open,
             andor_get_int, andor_get_string, andor_get_enum_index,
             AT_GetInt(), AT_GetIntMin(), AT_GetIntMax(), AT_SetInt(),
             AT_GetFloat(), AT_GetFloatMin(), AT_GetFloatMax(), AT_SetFloat().
 */

extern andor_get_string;
extern andor_set_string;
/* DOCUMENT andor_get_string(cam, prop);
         or andor_set_string, cam, prop, value;
     Query/set the value of string feature PROP for Andor camera CAM.  The
     special value ANDOR_SYSTEM can be used for CAM to access features not
     related to a specific camera but which are global properties.

   SEE ALSO: andor_intro, andor_is_implemented, andor_open,
             andor_get_int, andor_get_string, andor_get_enum_index,
             AT_GetInt(), AT_GetIntMin(), AT_GetIntMax(), AT_SetInt(),
             AT_GetFloat(), AT_GetFloatMin(), AT_GetFloatMax(), AT_SetFloat().
 */

extern andor_set_enum_string;
extern andor_get_enum_string;
extern andor_get_enum_string_by_index;
extern andor_set_enum_index;
extern andor_get_enum_index;
extern andor_get_enum_count;
extern andor_is_enum_index_available;
extern andor_is_enum_index_implemented;
/* DOCUMENT andor_set_enum_string, cam, prop, str;
         or str = andor_get_enum_string(cam, prop);
         or str = andor_get_enum_string_by_index(cam, prop, idx);
         or idx = andor_get_enum_index(cam, prop);
         or andor_set_enum_index, cam, prop, idx;
         or idx = andor_get_enum_index(cam, prop);
         or cnt = andor_get_enum_count(cam, prop);
         or andor_is_enum_index_available(cam, prop, idx);
         or andor_is_enum_index_implemented(cam, prop, idx);

     These fucntions deal with enumerated camera features.  In the arguments
     of these functions, CAM is the camera (can be ANDOR_SYSTEM for global
     poroperties), PROP is the name of the property or feature to set or
     query, IDX is the index of the enumerated feature which runs from 0 to
     CNT-1 where CNT is the number of possible values for the enumerated
     feature and STR is the string value of the enumerated feature.

     The functions andor_set_enum_string and andor_get_enum_string set and
     retrieve the string value of an enumerated feature.  If the feature is
     not implemented or has an incorrect type, andor_get_enum_string returns a
     NULL string.

     The function andor_get_enum_string_by_index retrieves the string value of
     an enumerated feature corresponding to the given index.

     The function andor_get_enum_index returns the index of the current value
     an enumerated feature.

     The function andor_get_enum_count returns the number of possible options
     for an enumerated feature.

     The functions andor_is_enum_index_available and
     andor_is_enum_index_implemented determines whether an option is
     temporarily or permanently unavailable.

     It the feature is not implanted as having and enumerated value, the
     functions andor_is_enum_index_available and
     andor_is_enum_index_implemented will return false while the function
     andor_get_enum_count will return a count of zero.

   SEE ALSO: andor_intro, andor_open,  andor_list_enum_string,
             andor_list_enum_implemented, andor_list_enum_available.
 */

/*---------------------------------------------------------------------------*/
/* INTERPRETED FUNCTIONS */

local andor_get_width;
local andor_get_height;
/* DOCUMENT width = andor_get_width(cam);
         or height = andor_get_height(cam);

      These functions retrieve the number of (super-)pixels along the
      dimensions of the images acquired by camera CAM.

   SEE ALSO: andor_is_implemented, andor_get_int.
 */
func andor_get_width(cam)
{
  feature = "AOI Width";
  if (! andor_is_implemented(cam, feature)) feature = "Sensor Width";
  return andor_get_int(cam, feature);
}

func andor_get_height(cam)
{
  feature = "AOI Height";
  if (! andor_is_implemented(cam, feature)) feature = "Sensor Height";
  return andor_get_int(cam, feature);
}

local andor_get_single;
local andor_get_sequence;
/* DOCUMENT img = andor_get_single(cam);
         or ptr = andor_get_sequence(cam, cnt);

      The function andor_get_single() returns a single image IMG from camera
      CAM.  The function andor_get_sequence() returns a sequence of CNT images
      from camera CAM.  The sequence is stored in a pointer vector PTR.

      Keyword TIMEOUT can be used to specify a timeout.

   SEE ALSO: andor_set_queue_length, andor_start_acquisition, andor_wait_image,
             andor_stop_acquisition.
 */
func andor_get_single(cam, timeout=)
{
  if (is_void(timeout)) {
    // default is to wait forever
    timeout = -1;
  }
  if (cam.queue_length < 1) {
    andor_set_queue_length, cam, 1;
  }
  andor_start_acquisition, cam;
  img = andor_wait_image(cam, timeout);
  andor_stop_acquisition, cam;
  return img;
}
func andor_get_sequence(cam, cnt, timeout=)
{
  if (is_void(timeout)) {
    // default is to wait forever
    timeout = -1;
  }
  len = min(cnt, 20);
  if (cam.queue_length < len) {
    andor_set_queue_length, cam, len;
  }
  ptr = array(pointer, cnt);
  andor_start_acquisition, cam;
  for (k = 1; k <= cnt; ++k) {
    ptr(k) = &andor_wait_image(cam, timeout);
  }
  andor_stop_acquisition, cam;
  return ptr;
}

local andor_list_enum_string;
local andor_list_enum_implemented;
local andor_list_enum_available
/* DOCUMENT andor_list_enum_string(cam, prop);
         or andor_list_enum_implemented(cam, prop);
         or andor_list_enum_available(cam, prop);

      The function andor_list_enum_string returns the list of possible string
      values for the enumerated feature PROP of camera CAM.

      The functions andor_list_enum_implemented and andor_list_enum_available
      return an array of CNT integers (CNT is the value returned by
      andor_get_enum_count) which are 1 or 0 depending the corresponding
      option is available or implemented for the enumerated feature PROP of
      camera CAM.

      values for the enumerated feature PROP of camera CAM.


      If PROP is not an enumerated feature, this functions return an empty
      result.

   SEE ALSO: andor_get_enum_count;
 */
func andor_list_enum_string(cam, prop)
{
  cnt = andor_get_enum_count(cam, prop);
  if (cnt < 1) return;
  list = array(string, cnt);
  for (idx = 0; idx < cnt; ++idx) {
    list(idx+1) = andor_get_enum_string_by_index(cam, prop, idx);
  }
  return list;
}

func andor_list_enum_implemented(cam, prop)
{
  cnt = andor_get_enum_count(cam, prop);
  if (cnt < 1) return;
  list = array(int, cnt);
  for (idx = 0; idx < cnt; ++idx) {
    list(idx+1) = andor_is_enum_index_implemented(cam, prop, idx);
  }
  return list;
}

func andor_list_enum_available(cam, prop)
{
  cnt = andor_get_enum_count(cam, prop);
  if (cnt < 1) return;
  list = array(int, cnt);
  for (idx = 0; idx < cnt; ++idx) {
    list(idx+1) = andor_is_enum_index_available(cam, prop, idx);
  }
  return list;
}

func andor_info(cam, unimplemented=, unreadable=, command=)
/* DOCUMENT andor_info, cam;
     Display the list of current features for camera CAM (can be
     ANDOR_SYSTEM).  If the keywords UNIMPLEMENTED, UNREADABLE, or COMMAND are
     set true, the un-implemented, un-readable, or command features are
     respectively also listed.

   SEE ALSO: andor_open, andor_is_implemented, andor_is_readable,
             andor_get_bool, andor_get_int, andor_get_float, andor_get_string.
 */
{
  /* Make some aliases and shortcuts. */
  BOOLEAN    = _ANDOR_BOOLEAN;
  ENUMERATED = _ANDOR_ENUMERATED;
  INTEGER    = _ANDOR_INTEGER;
  FLOAT      = _ANDOR_FLOAT;
  STRING     = _ANDOR_STRING;
  COMMAND    = _ANDOR_COMMAND;
  local names; eq_nocopy, names, _ANDOR_FEATURE_NAMES;
  local types; eq_nocopy, types, _ANDOR_FEATURE_TYPES;

  ndashes = 3 + max(strlen(names));
  dash = strchar(array('-', ndashes));

  cnt = numberof(names);
  for (k = 1; k <= cnt; ++k) {
    prop = names(k);
    type = types(k);
    if (! andor_is_implemented(cam, prop)) {
      if (! unimplemented) continue;
      value = "NOT IMPLEMENTED";
    } else if (! andor_is_readable(cam, prop)) {
      if (! unreadable) continue;
      value = "NOT READABLE";
    } else if (type == BOOLEAN) {
      value = andor_get_bool(cam, prop);
      value = (value ? "TRUE" : "FALSE");
    } else if (type == ENUMERATED) {
      value = andor_get_enum_string(cam, prop);
    } else if (type == INTEGER) {
      value = andor_get_int(cam, prop);
      value = swrite(format="%d", value);
    } else if (type == FLOAT) {
      value = andor_get_float(cam, prop);
      value = swrite(format="%g", value);
    } else if (type == STRING) {
      value = andor_get_string(cam, prop);
    } else if (type == COMMAND) {
      if (! command) continue;
      value = "COMMAND";
    } else {
      error, "unknown feature type (BUG)";
    }
    write, format="  %s %s> %s\n", prop,
      strpart(dash, 1 : ndashes - strlen(prop)), value;
  }
}

func _andor_init
/** DOCUMENT Private function automatically called to initialize global
    data. */
{
  extern _ANDOR_FEATURE_LIST;
  extern _ANDOR_FEATURE_NAMES;
  extern _ANDOR_FEATURE_TYPES;

  /* Shortcuts for the list below. */
  BOOLEAN    = _ANDOR_BOOLEAN;
  ENUMERATED = _ANDOR_ENUMERATED;
  INTEGER    = _ANDOR_INTEGER;
  FLOAT      = _ANDOR_FLOAT;
  STRING     = _ANDOR_STRING;
  COMMAND    = _ANDOR_COMMAND;

  /* List of features. */
  list =_lst("AccumulateCount",             INTEGER,
             "AcquisitionStart",            COMMAND,
             "AcquisitionStop",             COMMAND,
             "AOIBinning",                  ENUMERATED,
             "AOIHBin",                     INTEGER,
             "AOIHeight",                   INTEGER,
             "AOILeft",                     INTEGER,
             "AOIStride",                   INTEGER,
             "AOITop",                      INTEGER,
             "AOIVBin",                     INTEGER,
             "AOIWidth",                    INTEGER,
             "AuxiliaryOutSource",          ENUMERATED,
             "BaselineLevel",               INTEGER,
             "BitDepth",                    ENUMERATED,
             "BufferOverflowEvent",         INTEGER,
             "BytesPerPixel",               FLOAT,
             "CameraAcquiring",             BOOLEAN,
             "CameraDump",                  COMMAND,
             "CameraModel",                 STRING,
             "CameraName",                  STRING,
             "ControllerID",                STRING,
             "CycleMode",                   ENUMERATED,
             "DeviceCount",                 INTEGER, /* system */
             "DeviceVideoIndex",            INTEGER,
             "ElectronicShutteringMode",    ENUMERATED,
             "EventEnable",                 BOOLEAN,
             "EventsMissedEvent",           INTEGER,
             "EventSelector",               ENUMERATED,
             "ExposureTime",                FLOAT,
             "ExposureEndEvent",            INTEGER,
             "ExposureStartEvent",          INTEGER,
             "FanSpeed",                    ENUMERATED,
             "FirmwareVersion",             STRING,
             "FrameCount",                  INTEGER,
             "FrameRate",                   FLOAT,
             "FullAOIControl",              BOOLEAN,
             "ImageSizeBytes",              INTEGER,
             "InterfaceType",               STRING,
             "IOInvert",                    BOOLEAN,
             "IOSelector",                  ENUMERATED,
             "LUTIndex",                    INTEGER,
             "LUTValue",                    INTEGER,
             "MaxInterfaceTransferRate",    FLOAT,
             "MetadataEnable",              BOOLEAN,
             "MetadataFrame",               BOOLEAN,
             "MetadataTimestamp",           BOOLEAN,
             "Overlap",                     BOOLEAN,
             "PixelCorrection",             ENUMERATED,
             "PixelEncoding",               ENUMERATED,
             "PixelHeight",                 FLOAT,
             "PixelReadoutRate",            ENUMERATED,
             "PixelWidth",                  FLOAT,
             "PreAmpGain",                  ENUMERATED,
             "PreAmpGainChannel",           ENUMERATED,
             "PreAmpGainControl",           ENUMERATED,
             "PreAmpGainSelector",          ENUMERATED,
             "ReadoutTime",                 FLOAT,
             "RollingShutterGlobalClear",   BOOLEAN,
             "RowNExposureEndEvent",        INTEGER,
             "RowNExposureStartEvent",      INTEGER,
             "SensorCooling",               BOOLEAN,
             "SensorHeight",                INTEGER,
             "SensorTemperature",           FLOAT,
             "SensorWidth",                 INTEGER,
             "SerialNumber",                STRING,
             "SimplePreAmpGainControl",     ENUMERATED,
             "SoftwareTrigger",             COMMAND,
             "SoftwareVersion",             STRING,
             "SpuriousNoiseFilter",         BOOLEAN,
             "SynchronousTriggering",       BOOLEAN,
             "TargetSensorTemperature",     FLOAT,
             "TemperatureControl",          ENUMERATED,
             "TemperatureStatus",           ENUMERATED,
             "TimestampClock",              INTEGER,
             "TimestampClockFrequency",     INTEGER,
             "TimestampClockReset",         COMMAND,
             "TriggerMode",                 ENUMERATED,
             "VerticallyCenterAOI",         BOOLEAN);

  cnt = _len(list)/2;
  names = array(string, cnt);
  types = array(long, cnt);
  eq_nocopy, _ANDOR_FEATURE_LIST, list;
  eq_nocopy, _ANDOR_FEATURE_NAMES, names;
  eq_nocopy, _ANDOR_FEATURE_TYPES, types;
  for (k = 1; k <= cnt; ++k) {
    names(k) = _car(list);
    list= _cdr(list);
    types(k) = _car(list);
    list= _cdr(list);
  }
}

_ANDOR_BOOLEAN    = 1;
_ANDOR_ENUMERATED = 2;
_ANDOR_INTEGER    = 3;
_ANDOR_FLOAT      = 4;
_ANDOR_STRING     = 5;
_ANDOR_COMMAND    = 6;

_andor_init;
