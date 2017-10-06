/*
 * andor.c --
 *
 * Implementation of interface to Andor cameras for Yorick.
 *
 *-----------------------------------------------------------------------------
 *
 * This file is part of `YAndor` which is licensed under the MIT "Expat"
 * License.
 *
 * Copyright (C) 2014 Éric Thiébaut <https://github.com/emmt/YAndor>
 */

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include "atcore.h"
#include "yapi.h"
#include "pstdlib.h"

#define TRUE  1
#define FALSE 0

/*---------------------------------------------------------------------------*/
/* UTILITIES */

/* There is no function to query the length of an enumeration string.  Perhaps
   trial and error can work, but let's assume the following fairly large
   values. */
#define ENUM_STRING_MAXLEN    255
#define PIXEL_ENCODING_MAXLEN  63


#define ROUND_UP(a, b) ((((b) - 1 + (a))/(b))*(b))

/* For best performances, frame buffers must be aligned on 8 bytes
   boundaries. */
#define FRAME_ALIGN    8

/* Compute address of first frame in camera queue of buffers. */
#define FIRST_FRAME(cam) ((unsigned char*)ROUND_UP((ptrdiff_t)(cam)->buffer, \
                                                   FRAME_ALIGN))

#define _STRINGIFY(a)  #a
#define STRINGIFY(a)  _STRINGIFY(a)

/* Define some aliases or some function for dealing with Yorick stack
   arguments. */

#define get_int       ygets_i
#define get_long      ygets_l
#define get_double    ygets_d
#define get_string    ygets_q
#define get_boolean   yarg_true

#define push_int      ypush_int
#define push_long     ypush_long
#define push_double   ypush_double
#define push_nil      ypush_nil

static void
push_string(const char* str)
{
  ypush_q(NULL)[0] = (str == NULL ? NULL : p_strcpy(str));
}

static void
push_int64(AT_64 value)
{
  if (value < LONG_MIN || value > LONG_MAX) {
    y_error("integer overflow");
  }
  push_long((long)value);
}

static void
warning(const char* message, ...)
{
  ptrdiff_t length;
  va_list ap;
  length = (message != NULL ? strlen(message) : 0);
  fprintf(stderr, "*** WARNING *** ");
  va_start(ap, message);
  vfprintf(stderr, message, ap);
  va_end(ap);
  if (length < 1 || message[length - 1] != '\n') {
    fprintf(stderr, "\n");
  }
  fflush(stderr);
}

/* Some conversion functions are needed to deal with wide-character
   strings which are used by the Andor SDK while Yorick only knows about
   basic C-strings. */

/**
 * @brief Return a temporary workspace to store a small amount of data.
 *
 * To minimize the number of calls to malloc/free/realloc, this function uses
 * an internal dynamic resizable buffer.  It is not thread safe.  Yorick
 * p_malloc and p_free functions are used to manage the buffer and an error
 * (see y_error) is raised in case of failure.  Thus the function can only
 * return a successful result.
 *
 * @param size The number of bytes to allocate.  As a special case,
 *             when `size` is 0, the internal buffer is released.
 *
 * @return The address of the buffer.
 */
static void*
get_wbuf(size_t size)
{
  static void* wbuf_data = NULL;
  static size_t wbuf_size = 0;
  void* ptr;

  /* Make sure an interruption did not messed up our stuff. */
  if (wbuf_size == 0 || wbuf_data == NULL) {
    wbuf_size = 0;
    wbuf_data = NULL;
  }

  /* Return current buffer if it is large enough. */
  if (0 < size && size <= wbuf_size) {
    return wbuf_data;
  }

  /* Free current buffer (if any). */
  if (wbuf_data != NULL) {
    wbuf_size = 0;
    ptr = wbuf_data;
    wbuf_data = NULL;
    p_free(ptr);
  }

  /* Allocate a new buffer. */
  wbuf_data = p_malloc(size);
  if (wbuf_data == NULL) {
    y_error("insufficient memory");
  } else {
    wbuf_size = size;
  }
  return wbuf_data;
}

static wchar_t*
to_wide(const char* str, int scratch)
{
  int c;
  wchar_t w;
  wchar_t* wcs;
  size_t j, len, size;

  if (str == NULL) {
    return NULL;
  }
  len = strlen(str);
  size = (len + 1)*sizeof(wchar_t);
  if (scratch) {
    wcs = (wchar_t*)ypush_scratch(size, NULL);
  } else {
    wcs = (wchar_t*)get_wbuf(size);
  }
  for (j = 0; j < len; ++j) {
    c = str[j];
    if (c >= 0 && c <= 127 && (w = btowc(c)) != WEOF) {
      wcs[j] = w;
    } else {
      y_error("invalid character in name");
    }
  }
  wcs[len] = L'\0';
  return wcs;
}

static char*
to_char(const wchar_t* wcs, int scratch)
{
  int c;
  char* str;
  size_t j, len, size;

  if (wcs == NULL) {
    return NULL;
  }
  len = wcslen(wcs);
  size = (len + 1)*sizeof(char);
  if (scratch) {
    str = (char*)ypush_scratch(size, NULL);
  } else {
    str = (char*)get_wbuf(size);
  }
  for (j = 0; j < len; ++j) {
    c = wctob(wcs[j]);
    if (c != EOF) {
      str[j] = c;
    } else {
      y_error("invalid wide-character in name");
    }
  }
  str[len] = '\0';
  return str;
}

static wchar_t*
get_wide_string(int iarg, int scratch)
{
  return to_wide(get_string(iarg), scratch);
}

static const char*
get_reason(int code)
{
  const char* str;

#define CASE(a) case AT_##a: str = "AT_"#a; break
  switch (code) {
    CASE(SUCCESS);
    CASE(ERR_NOTINITIALISED);
    CASE(ERR_NOTIMPLEMENTED);
    CASE(ERR_READONLY);
    CASE(ERR_NOTREADABLE);
    CASE(ERR_NOTWRITABLE);
    CASE(ERR_OUTOFRANGE);
    CASE(ERR_INDEXNOTAVAILABLE);
    CASE(ERR_INDEXNOTIMPLEMENTED);
    CASE(ERR_EXCEEDEDMAXSTRINGLENGTH);
    CASE(ERR_CONNECTION);
    CASE(ERR_NODATA);
    CASE(ERR_INVALIDHANDLE);
    CASE(ERR_TIMEDOUT);
  default:
    str = "Unknown code";
  }
#undef CASE
  return str;
}

static void
throw(const char* descr, int code)
{
  static char message[256];
  if (code != AT_SUCCESS) {
    sprintf(message, "failure in %s (%s)",
            descr, get_reason(code));
    y_error(message);
  }
}

/* Initialize the interface and set the number of devices. */
static int number_of_devices = -1;
static void
initialize_library(void)
{
  if (number_of_devices < 0) {
    AT_64 device_count;
    int code;
    code = AT_InitialiseLibrary();
    if (code != AT_SUCCESS) {
      throw("AT_InitialiseLibrary", code);
    }
    code = AT_GetInt(AT_HANDLE_SYSTEM, L"DeviceCount", &device_count);
    if (code != AT_SUCCESS) {
      (void)AT_FinaliseLibrary();
      throw("AT_GetInt \"DeviceCount\"", code);
    }
    if (device_count < 0) {
      (void)AT_FinaliseLibrary();
      y_error("unexpected number of devices");
    }
    if (device_count < INT_MIN || device_count > INT_MAX) {
      (void)AT_FinaliseLibrary();
      y_error("integer overflow");
    }
    number_of_devices = (int)device_count;
  }
}

#define INITIALIZE if (number_of_devices >= 0) ; else initialize_library()

/*---------------------------------------------------------------------------*/
/* PSEUDO-OBJECT MANAGEMENT FOR DEVICES */

/* A Yorick "user object" is created to hold a camera handle (and its
   parameters). */

typedef struct _camera camera_t;

static long get_frame_width(camera_t* cam);
static long get_frame_height(camera_t* cam);
static long get_frame_size(camera_t* cam);
static long get_row_stride(camera_t* cam);
static int get_pixel_encoding(camera_t* cam);
static void start_acquisition(camera_t* cam);
static void stop_acquisition(camera_t* cam, int final);

/* Functions to extract frame data as a Yorick array. */
static void extract_Raw(const camera_t* cam, const unsigned char* src);
static void extract_Mono8(const camera_t* cam, const unsigned char* src);
static void extract_Mono12Packed(const camera_t* cam, const unsigned char* src);
static void extract_Mono12(const camera_t* cam, const unsigned char* src);
static void extract_Mono16(const camera_t* cam, const unsigned char* src);
static void extract_Mono32(const camera_t* cam, const unsigned char* src);
static void extract_RGB8Packed(const camera_t* cam, const unsigned char* src);
static void extract_Mono12Coded(const camera_t* cam, const unsigned char* src);
static void extract_Mono12codedPacked(const camera_t* cam, const unsigned char* src);
static void extract_Mono12parallel(const camera_t* cam, const unsigned char* src);
static void extract_Mono12PackedParallel(const camera_t* cam, const unsigned char* src);

static struct {
  const char* name;
  const wchar_t* wide_name;
  void (*extract)(const camera_t* cam, const unsigned char* src);
} pixel_encoding_table[] = {
#define ROW(a) {#a, L ## #a, extract_##a}
  ROW(Raw),
  ROW(Mono8),
  ROW(Mono12Packed),
  ROW(Mono12),
  ROW(Mono16),
  ROW(Mono32),
  ROW(RGB8Packed),
  ROW(Mono12Coded),
  ROW(Mono12codedPacked),
  ROW(Mono12parallel),
  ROW(Mono12PackedParallel),
#undef ROW
  {NULL, NULL, NULL}
};
static const int number_of_pixel_encodings =
  sizeof(pixel_encoding_table)/sizeof(pixel_encoding_table[0]) - 1;

struct _camera {
  AT_H handle;
  int device;
  int initialized;
  int acquiring;      /* Camera is acquiring? */
  AT_U8* buffer;      /* Current queue buffer. */
  long buffer_size;   /* Current queue buffer size. */
  long queue_length;  /* Number of frames in the queue. */
  long frame_size;    /* Frame size in bytes when acquisition started. */
  long frame_width;   /* Frame width in (super-)pixels when acquisition
                         started. */
  long frame_height;  /* Frame height in (super-)pixels when acquisition
                         started. */
  long row_stride;    /* The size of one row in the image in bytes when
                         acquisition started. */

  /* Method to extract the frame data into a Yorick array which is pushed on
     top of the stack. */
  void (*extract)(const camera_t* cam, const unsigned char* src);

};

/* Get a "camera" from the stack. */
static camera_t* get_camera(int iarg);

/* Method for "camera" objects. */
static void free_camera(void*);
static void print_camera(void*);
static void eval_camera(void*, int);
static void extract_camera(void*, char*);

/* Class definition for "camera" objects. */
y_userobj_t camera_type = {
  "handle to Andor Tech. camera",
  free_camera, print_camera, eval_camera, extract_camera, NULL
};

static void
free_camera(void* ptr)
{
  camera_t* cam = (camera_t*)ptr;
  if (cam->initialized) {
    /* Close camera if object correctly initialized.  When the free_camera
       method is called by Yorick it is probably better to not raise errors,
       so the return code of AT_Close() is ignored. */
    cam->initialized = FALSE;
    if (cam->acquiring) {
      stop_acquisition(cam, TRUE);
    }
    if (cam->buffer != NULL) {
      p_free(cam->buffer);
    }
    (void)AT_Close(cam->handle);
  }
}

static void
print_camera(void* ptr)
{
  char buffer[128];
  camera_t* cam = (camera_t*)ptr;
  sprintf(buffer, " (device = %d, queue_length = %ld, acquiring = %s)",
          cam->device, cam->queue_length,
          (cam->acquiring ? "TRUE" : "FALSE"));
  y_print(camera_type.type_name, 0);
  y_print(buffer, 1);
}

static void
eval_camera(void* ptr, int argc)
{
  push_nil();
}

static void
extract_camera(void* ptr, char* name)
{
  camera_t* cam = (camera_t*)ptr;
  if (name[0] == 'a' && strcmp(name + 1, "cquiring") == 0) {
    push_int(cam->acquiring);
  } else if (name[0] == 'b' && strncmp(name + 1, "uffer", 5) == 0) {
    if (name[6] == '\0') {
       push_long((long)cam->buffer);
   } else if (name[6] == '_' && strcmp(name + 7, "size") == 0) {
      push_long(cam->buffer_size);
    } else {
      goto illegal;
    }
  } else if (name[0] == 'd' && strcmp(name + 1, "evice") == 0) {
    push_long(cam->device);
  } else if (name[0] == 'q' && strcmp(name + 1, "ueue_length") == 0) {
    push_long(cam->queue_length);
  } else if (name[0] == 'r' && strcmp(name + 1, "ow_stride") == 0) {
      push_long(cam->row_stride);
  } else if (name[0] == 'f' && strncmp(name + 1, "rame_", 5) == 0) {
    if (name[6] == 'w' && strcmp(name + 7, "idth") == 0) {
      push_long(cam->frame_width);
    } else if (name[6] == 'h' && strcmp(name + 7, "eight") == 0) {
      push_long(cam->frame_height);
    } else if (name[6] == 's' && strcmp(name + 7, "ize") == 0) {
      push_long(cam->frame_size);
    } else {
      goto illegal;
    }
 } else {
  illegal:
    y_error("illegal member");
  }
}

static camera_t*
get_camera(int iarg)
{
  return (camera_t*)yget_obj(iarg, &camera_type);
}

static AT_H
get_camera_handle(int iarg)
{
  if (yarg_nil(iarg)) {
    return AT_HANDLE_SYSTEM;
  } else {
    return get_camera(iarg)->handle;
  }
}

static long
get_frame_width(camera_t* cam)
{
  AT_64 width;
  wchar_t* feature;
  int code;
  AT_BOOL available;

  feature = L"AOI Width";
  code = AT_IsImplemented(cam->handle, feature, &available);
  if (code != AT_SUCCESS) {
    throw("AT_IsImplemented \"AOI Width\"", code);
  }
  if (! available) {
    feature = L"Sensor Width";
  }
  code = AT_GetInt(cam->handle, feature, &width);
  if (code != AT_SUCCESS) {
    throw((available ? "AT_GetInt \"AOI Width\""
                     : "AT_GetInt \"Sensor Width\""), code);
  }
  if (width < 0 || width > LONG_MAX) {
    y_error("invalid frame width");
  }
  return (long)width;
}

static long
get_frame_height(camera_t* cam)
{
  AT_64 height;
  wchar_t* feature;
  int code;
  AT_BOOL available;

  feature = L"AOI Height";
  code = AT_IsImplemented(cam->handle, feature, &available);
  if (code != AT_SUCCESS) {
    throw("AT_IsImplemented \"AOI Height\"", code);
  }
  if (! available) {
    feature = L"Sensor Height";
  }
  code = AT_GetInt(cam->handle, feature, &height);
  if (code != AT_SUCCESS) {
    throw((available ? "AT_GetInt \"AOI Height\""
                     : "AT_GetInt \"Sensor Height\""), code);
  }
  if (height < 0 || height > LONG_MAX) {
    y_error("invalid frame height");
  }
  return (long)height;
}

/* Get the number of bytes required to store one frame. */
static long
get_frame_size(camera_t* cam)
{
  AT_64 size;
  int code = AT_GetInt(cam->handle, L"ImageSizeBytes", &size);
  if (code != AT_SUCCESS) {
    throw("AT_GetInt \"ImageSizeBytes\"", code);
  }
  if (size < 0 || size > LONG_MAX) {
    y_error("invalid frame size");
  }
  return (long)size;
}

/* Get the number of bytes required to store one frame. */
static long
get_row_stride(camera_t* cam)
{
  AT_64 size;
  int code = AT_GetInt(cam->handle, L"AOIStride", &size);
  if (code != AT_SUCCESS) {
    throw("AT_GetInt \"AOIStride\"", code);
  }
  if (size < 0 || size > LONG_MAX) {
    y_error("invalid row stride");
  }
  return (long)size;
}

static int
get_pixel_encoding(camera_t* cam)
{
  wchar_t pixel_encoding[PIXEL_ENCODING_MAXLEN+1];
  int code, i, index;

  code = AT_GetEnumIndex(cam->handle, L"PixelEncoding", &index);
  if (code != AT_SUCCESS) {
    throw("AT_GetEnumIndex \"PixelEncoding\"", code);
  }
  code = AT_GetEnumStringByIndex(cam->handle, L"PixelEncoding", index,
                                 pixel_encoding, PIXEL_ENCODING_MAXLEN+1);
  if (code != AT_SUCCESS) {
    throw("AT_GetEnumStringByIndex \"PixelEncoding\"", code);
  }
  pixel_encoding[PIXEL_ENCODING_MAXLEN] = L'\0';
  for (i = 0; pixel_encoding_table[i].wide_name; ++i) {
    if (pixel_encoding_table[i].wide_name[0] == pixel_encoding[0] &&
        wcscmp(pixel_encoding_table[i].wide_name, pixel_encoding) == 0) {
      return i;
    }
  }
  return -1;
}

/* Start the acquisition. */
static void
start_acquisition(camera_t* cam)
{
  unsigned char* frame_ptr;
  long buffer_size, frame_stride, k;
  int enc, code;

  /* Check argument. */
  if (cam->acquiring) {
    warning("Camera already acquiring.");
    return;
  }
  if (cam->queue_length <= 0) {
    y_error("set queue length first");
  }

  /* Determine pixel format. */
  enc = get_pixel_encoding(cam);
  if (enc == -1) {
    warning("Unknown pixel encoding.");
    cam->extract = extract_Raw;
  } else {
    cam->extract = pixel_encoding_table[enc].extract;
  }

  /* Make sure no buffers are currently in use. */
  (void)AT_Flush(cam->handle);

  /* Create queue of frame buffers. */
  cam->frame_size = get_frame_size(cam);
  cam->frame_width = get_frame_width(cam);
  cam->frame_height = get_frame_height(cam);
  cam->row_stride = get_row_stride(cam);
  frame_stride = ROUND_UP(cam->frame_size, FRAME_ALIGN);
  buffer_size = (FRAME_ALIGN - 1) + frame_stride*cam->queue_length;
  if (cam->buffer != NULL && cam->buffer_size > 0) {
    if (buffer_size != cam->buffer_size) {
      /* Free existing buffer if not of the correct size. */
      void* ptr = cam->buffer;
      cam->buffer = NULL;
      cam->buffer_size = 0;
      p_free(ptr); /* free *after* updating members (in case of interrupts) */
    }
  } else {
    cam->buffer = NULL;
    cam->buffer_size = 0;
  }
  if (buffer_size != cam->buffer_size) {
    /* Allocate a new buffer. */
    cam->buffer = p_malloc(buffer_size);
    cam->buffer_size = buffer_size;
  }

  /* Queue the buffers. */
  frame_ptr = FIRST_FRAME(cam);
  for (k = 0; k < cam->queue_length; ++k) {
    code = AT_QueueBuffer(cam->handle, (AT_U8*)frame_ptr, cam->frame_size);
    if (code != AT_SUCCESS) {
      /* Cancel the queued buffers and report error. */
      (void)AT_Flush(cam->handle);
      throw("AT_QueueBuffer", code);
    }
    frame_ptr += frame_stride;
  }

  /* Set the camera to continuously acquires frames. */
  code = AT_SetEnumString(cam->handle, L"CycleMode", L"Continuous");
  if (code != AT_SUCCESS) {
    /* Cancel the queued buffers and report error. */
    (void)AT_Flush(cam->handle);
    throw("AT_SetEnumString \"CycleMode\" \"Continuous\"", code);
  }

  /* Start the acquisition. */
  code = AT_Command(cam->handle, L"AcquisitionStart");
  if (code != AT_SUCCESS) {
    /* Cancel the queued buffers and report error. */
    (void)AT_Flush(cam->handle);
    throw("AT_Command \"AcquisitionStart\"", code);
  }
  cam->acquiring = TRUE;
}

static void
stop_acquisition(camera_t* cam, int final)
{
  int code;

  if (! cam->acquiring) {
    warning("Camera not acquiring.");
    return;
  }
  code = AT_Command(cam->handle, L"AcquisitionStop");
  if (code != AT_SUCCESS && ! final) {
    /* We are in trouble if we stop here, so we do not throw any error but
       just issue a warning. */
    warning("Failure of AT_Command \"AcquisitionStop\" (%s).",
            get_reason(code));
  }
  code = AT_Flush(cam->handle);
  if (code != AT_SUCCESS && ! final) {
    /* We are in trouble if we stop here, so we do not throw any error but
       just issue a warning. */
    warning("Failure of AT_Flush (%s).", get_reason(code));
  }
  cam->acquiring = FALSE;
}

/*---------------------------------------------------------------------------*/
/* BUILT-IN FUNCTIONS */

void
Y_andor_count_devices(int argc)
{
  if (argc != 1 || ! yarg_nil(0)) {
    y_error("expecting exactly 1 nil argument");
  }

  /* Make sure library has been initialized. */
  INITIALIZE;

  /* Push the number of devices as a long integer as this is the default
     integer type for Yorick ('int' is for booleans). */
  push_long(number_of_devices);
}

void
Y_andor_list_devices(int argc)
{
#define MAXLEN 127
  long dims[2];
  AT_WC wcs[MAXLEN+1];
  char* str;
  char** result;
  int dev, code;
  AT_H handle;

  if (argc != 1 || ! yarg_nil(0)) {
    y_error("expecting exactly 1 nil argument");
  }

  /* Make sure library has been initialized. */
  INITIALIZE;

  /* Build the list of devices. */
  if (number_of_devices > 0) {
    dims[0] = 1;
    dims[1] = number_of_devices;
    result = ypush_q(dims);
    for (dev = 0; dev < number_of_devices; ++dev) {
      code = AT_Open(dev, &handle);
      if (code != AT_SUCCESS) {
        throw("AT_Open", code);
      }
      code = AT_GetString(handle, L"Camera Model", wcs, MAXLEN);
      if (code != AT_SUCCESS) {
        throw("AT_GetString \"Camera Model\"", code);
      }
      str = to_char(wcs, FALSE);
      result[dev] = (str == NULL ? NULL : p_strcpy(str));
      code = AT_Close(handle);
      if (code != AT_SUCCESS) {
        throw("AT_Close", code);
      }
    }
  } else {
    push_nil();
  }
#undef MAXLEN
}

void
Y_andor_open(int argc)
{
  camera_t* cam;
  int iarg, code, device;

  if (argc != 1) y_error("expecting exactly 1 argument");
  iarg = argc;
  device = get_int(--iarg);

  /* Make sure library has been initialized. */
  INITIALIZE;
  if (device < 0 || device >= number_of_devices) {
    y_error("out of range device index");
  }

  /* First, push object to avoid long-jumps. */
  cam = (camera_t*)ypush_obj(&camera_type, sizeof(camera_t));

  /* Second, open camera. */
  code = AT_Open(device, &cam->handle);
  if (code != AT_SUCCESS) {
    throw("AT_Open", code);
  }
  cam->device = device;
  cam->initialized = TRUE;
  cam->extract = extract_Raw;
}

/* Functions which retrieve a boolean value. */
#define FUNCTION(YFUNC, CFUNC)                                  \
void                                                            \
Y_##YFUNC(int argc)                                             \
{                                                               \
  AT_BOOL value;                                                \
  int code;                                                     \
  if (argc != 2) y_error("expecting exactly 2 arguments");      \
  code = CFUNC(get_camera_handle(1),                            \
               get_wide_string(0, FALSE), &value);              \
  if (code != AT_SUCCESS) throw(#CFUNC, code);                  \
  push_int((value ? TRUE : FALSE));                             \
}
FUNCTION(andor_get_bool,       AT_GetBool)
FUNCTION(andor_is_implemented, AT_IsImplemented)
FUNCTION(andor_is_read_only,   AT_IsReadOnly)
FUNCTION(andor_is_readable,    AT_IsReadable)
FUNCTION(andor_is_writable,    AT_IsWritable)
#undef FUNCTION

/* Functions which retrieve a long integer value. */
#define FUNCTION(YFUNC, CFUNC)                                  \
void                                                            \
Y_##YFUNC(int argc)                                             \
{                                                               \
  AT_64 value;                                                  \
  int code;                                                     \
  if (argc != 2) y_error("expecting exactly 2 arguments");      \
  code = CFUNC(get_camera_handle(1),                            \
               get_wide_string(0, FALSE), &value);              \
  if (code != AT_SUCCESS) throw(#CFUNC, code);                  \
  push_int64(value);                                            \
}
FUNCTION(andor_get_int,     AT_GetInt)
FUNCTION(andor_get_int_min, AT_GetIntMin)
FUNCTION(andor_get_int_max, AT_GetIntMax)
#undef FUNCTION

/* Functions which retrieve an integer value. */
#define FUNCTION(YFUNC, CFUNC)                                  \
void                                                            \
Y_##YFUNC(int argc)                                             \
{                                                               \
  int value, code;                                              \
  if (argc != 2) y_error("expecting exactly 2 arguments");      \
  code = CFUNC(get_camera_handle(1),                            \
               get_wide_string(0, FALSE), &value);              \
  if (code != AT_SUCCESS) throw(#CFUNC, code);                  \
  push_long(value);                                             \
}
FUNCTION(andor_get_enum_index, AT_GetEnumIndex)
#undef FUNCTION

void
Y_andor_get_enum_count(int argc)
{
  int value, code;
  if (argc != 2) y_error("expecting exactly 2 arguments");
  code = AT_GetEnumCount(get_camera_handle(1),
                         get_wide_string(0, FALSE), &value);
  if (code != AT_SUCCESS) {
    if (code != AT_ERR_NOTIMPLEMENTED) {
      throw("AT_GetEnumCount", code);
    }
    value = 0;
  }
  push_long(value);
}

/* Functions which retrieve a floating-point value. */
#define FUNCTION(YFUNC, CFUNC)                                  \
void                                                            \
Y_##YFUNC(int argc)                                             \
{                                                               \
  double value;                                                 \
  int code;                                                     \
  if (argc != 2) y_error("expecting exactly 2 arguments");      \
  code = CFUNC(get_camera_handle(1),                            \
               get_wide_string(0, FALSE), &value);              \
  if (code != AT_SUCCESS) throw(#CFUNC, code);                  \
  push_double(value);                                           \
}
FUNCTION(andor_get_float,     AT_GetFloat)
FUNCTION(andor_get_float_min, AT_GetFloatMin)
FUNCTION(andor_get_float_max, AT_GetFloatMax)
#undef FUNCTION

/* Functions which set a simple value. */
#define FUNCTION(YFUNC, CFUNC, GETTER)                          \
void                                                            \
Y_##YFUNC(int argc)                                             \
{                                                               \
  int code;                                                     \
  if (argc != 3) y_error("expecting exactly 3 arguments");      \
  code = CFUNC(get_camera_handle(2),                            \
               get_wide_string(1, FALSE), GETTER(0));           \
  if (code != AT_SUCCESS) throw(#CFUNC, code);                  \
  push_nil();                                                   \
}
FUNCTION(andor_set_int,        AT_SetInt,       get_long)
FUNCTION(andor_set_float,      AT_SetFloat,     get_double)
FUNCTION(andor_set_bool,       AT_SetBool,      get_boolean)
FUNCTION(andor_set_enum_index, AT_SetEnumIndex, get_int)
#undef FUNCTION

void
Y_andor_get_string(int argc)
{
  AT_H handle;
  wchar_t* feature;
  wchar_t* value;
  int code, length;
  size_t size;

  if (argc != 2) y_error("expecting exactly 2 arguments");
  handle = get_camera_handle(1);
  feature = get_wide_string(0, FALSE);
  if (feature == NULL) y_error("invalid NULL string");
  code = AT_GetStringMaxLength(handle, feature, &length);
  if (code != AT_SUCCESS) throw("AT_GetStringMaxLength", code);

  /* We cannot use the global workspace (already used to store FEATURE), so we
     create a new one large enough to store the wide-character value. */
  size = (length + 1)*sizeof(wchar_t);
  value = ypush_scratch(size, NULL);
  code = AT_GetString(handle, feature, value, length);
  if (code != AT_SUCCESS) throw("AT_GetString", code);
  value[length] = 0;

  /* Now we can use to_char() to convert the wide-character value using the
     global workspace. */
  push_string(to_char(value, FALSE));
}

/* Functions which set a string value. */
#define FUNCTION(YFUNC, CFUNC)                                          \
void                                                                    \
Y_##YFUNC(int argc)                                                     \
{                                                                       \
  AT_H handle;                                                          \
  wchar_t* feature;                                                     \
  wchar_t* value;                                                       \
  int code;                                                             \
                                                                        \
  if (argc != 3) y_error("expecting exactly 3 arguments");              \
  handle = get_camera_handle(2);                                        \
  feature = get_wide_string(1, FALSE); /* use global workspace */       \
  value = get_wide_string(0, TRUE); /* use scratch */                   \
  if (feature == NULL || value == NULL) y_error("invalid NULL string"); \
  code = CFUNC(handle, feature, value);                                 \
  if (code != AT_SUCCESS) throw(#CFUNC, code);                          \
  push_nil();                                                           \
}
FUNCTION(andor_set_string,      AT_SetString)
FUNCTION(andor_set_enum_string, AT_SetEnumString)
#undef FUNCTION

/* Functions which retrieve a boolean value for an enum feature. */
#define FUNCTION(YFUNC, CFUNC)                                \
void                                                          \
Y_##YFUNC(int argc)                                           \
{                                                             \
  AT_BOOL value;                                              \
  int code;                                                   \
  if (argc != 3) y_error("expecting exactly 2 arguments");    \
  code = CFUNC(get_camera_handle(2),                          \
               get_wide_string(1, FALSE),                     \
               get_int(0),                                    \
               &value);                                       \
  if (code != AT_SUCCESS) {                                   \
    if (code != AT_SUCCESS) throw(#CFUNC, code);              \
    value = FALSE;                                            \
  }                                                           \
  push_int((value ? TRUE : FALSE));                           \
}
FUNCTION(andor_is_enum_index_available,   AT_IsEnumIndexAvailable)
FUNCTION(andor_is_enum_index_implemented, AT_IsEnumIndexImplemented)
#undef FUNCTION

void
Y_andor_get_enum_string_by_index(int argc)
{
  wchar_t value[ENUM_STRING_MAXLEN+1];
  wchar_t* feature;
  AT_H handle;
  int code, index;

  if (argc != 3) y_error("expecting exactly 3 arguments");
  handle = get_camera_handle(2);
  feature = get_wide_string(1, FALSE);
  index = get_int(0);
  code = AT_GetEnumStringByIndex(handle, feature, index,
                                 value, ENUM_STRING_MAXLEN+1);
  if (code != AT_SUCCESS) {
    throw("AT_GetEnumStringByIndex", code);
  }
  value[ENUM_STRING_MAXLEN] = L'\0';
  push_string(to_char(value, FALSE));
}

void
Y_andor_get_enum_string(int argc)
{
  wchar_t value[ENUM_STRING_MAXLEN+1];
  wchar_t* feature;
  AT_H handle;
  int code, index;

  if (argc != 2) y_error("expecting exactly 2 arguments");
  handle = get_camera_handle(1);
  feature = get_wide_string(0, FALSE);
  code = AT_GetEnumIndex(handle, feature, &index);
  if (code != AT_SUCCESS) {
    /* FIXME: generalize this type of behavior to other "getters". */
    if (code == AT_ERR_NOTIMPLEMENTED) {
      push_string(NULL);
      return;
    }
    throw("AT_GetEnumIndex", code);
  }
  code = AT_GetEnumStringByIndex(handle, feature, index,
                                 value, ENUM_STRING_MAXLEN+1);
  if (code != AT_SUCCESS) {
    throw("AT_GetEnumStringByIndex", code);
  }
  value[ENUM_STRING_MAXLEN] = L'\0';
  push_string(to_char(value, FALSE));
}

void
Y_andor_command(int argc)
{
  char* command;
  camera_t* cam;
  AT_H handle;
  int code, done;

  if (argc != 2) y_error("expecting exactly 2 arguments");
  if (yarg_nil(1)) {
    cam = NULL;
    handle = AT_HANDLE_SYSTEM;
  } else {
    cam = get_camera(1);
    handle = cam->handle;
  }
  command = get_string(0);
  if (command == NULL) y_error("invalid NULL string for the command");

  done = FALSE;
  if (cam != NULL && strncmp("Acquisition", command, 11) == 0) {
    /* The may be a space between the words. */
    if (strcmp("Start", command + 11) == 0 ||
        strcmp(" Start", command + 11) == 0) {
      start_acquisition(cam);
      done = TRUE;
    } else if (strcmp("Stop", command + 11) == 0 ||
               strcmp(" Stop", command + 11) == 0) {
      stop_acquisition(cam, FALSE);
      done = TRUE;
    }
  }
  if (! done) {
    code = AT_Command(handle, to_wide(command, FALSE));
    if (code != AT_SUCCESS) throw("AT_Command", code);
  }
  push_nil();
}

/* The plugin takes care of managing the acquisition buffers.  To that end, the
   camera instance holds its own buffers and is aware of whether or not
   camera is acquiring.  When acquisition is started, the buffers are
   allocated (if needed) and queued.  This implies that the calls to AT_Command
   have to be filtered to detect when acquisition is started/stopped. */
void
Y_andor_set_queue_length(int argc)
{
  camera_t* cam;
  long queue_length;

  if (argc != 2) y_error("expecting exactly 2 arguments");
  cam = get_camera(1);
  queue_length = get_long(0);
  if (queue_length < 1) y_error("queue length must be >= 1");
  if (cam->acquiring) y_error("acquisition is acquiring");
  cam->queue_length = queue_length;
  push_nil();
}

void
Y_andor_start_acquisition(int argc)
{
  if (argc != 1) y_error("expecting exactly 1 argument");
  start_acquisition(get_camera(0));
}

void
Y_andor_stop_acquisition(int argc)
{
  if (argc != 1) y_error("expecting exactly 1 argument");
  stop_acquisition(get_camera(0), FALSE);
}

/* This function just check the consistency of my assumption about the way
   frame buffers are used by the SDK. */
static void
check_frame(const camera_t* cam, AT_U8* frame_ptr,
            long frame_size, int verbose)
{
  size_t base = ROUND_UP((size_t)cam->buffer, FRAME_ALIGN);
  size_t end = (size_t)cam->buffer + (size_t)cam->buffer_size;

  if (frame_size != cam->frame_size) {
    warning("frame_size (%d) != cam->frame_size (%ld).",
            frame_size, cam->frame_size);
  }
  if ((size_t)frame_ptr < base || (size_t)frame_ptr >= end) {
    warning("Returned frame address is outside our buffers.");
  } else {
    ptrdiff_t offset = (unsigned char*)frame_ptr - (unsigned char*)base;
    ptrdiff_t frame_stride = ROUND_UP(cam->frame_size, FRAME_ALIGN);
    if (offset % frame_stride != 0) {
      warning("Returned frame is not aligned with one of our buffers.");
    } else if (verbose) {
      fprintf(stderr, "*** INFO *** "
              "Returned frame is buffer index %d.\n",
              (int)(offset/frame_stride));
    }
  }
}

void
Y_andor_wait_image(int argc)
{
  int code, frame_size, timeout;
  camera_t* cam;
  AT_U8* frame_ptr;

  /* Get and check arguments. */
  if (argc != 2) y_error("expecting exactly 2 arguments");
  cam = get_camera(1);
  timeout = get_int(0);
  if (! cam->acquiring) y_error("camera is not acquiring");
  if (timeout < 0) timeout = AT_INFINITE;

  /* Sleep in this thread until data is ready. */
  code = AT_WaitBuffer(cam->handle, &frame_ptr, &frame_size, timeout);
  if (code != AT_SUCCESS) {
    throw("AT_WaitBuffer", code);
    /*push_nil();*/
    return;
  }
  check_frame(cam, frame_ptr, frame_size, FALSE);

  /* Extract frame data as a Yorick array. */
  if (cam->extract != NULL) {
    cam->extract(cam, (const unsigned char*)frame_ptr);
  } else {
    push_nil();
  }

  /* Re-queue the buffer. */
  code = AT_QueueBuffer(cam->handle, frame_ptr, frame_size);
  if (code != AT_SUCCESS) {
    throw("AT_QueueBuffer", code);
  }
}

static void
extract_Raw(const camera_t* cam, const unsigned char* src)
{
  long dims[2];
  dims[0] = 1;
  dims[1] = cam->frame_size;
  memcpy(ypush_c(dims), src, cam->frame_size);
}

#define FUNCTION(NAME, DST_TYPE, PUSH, SRC_TYPE)                \
static void                                                     \
NAME(const camera_t* cam, const unsigned char* src)             \
{                                                               \
  DST_TYPE* dst;                                                \
  long dims[3];                                                 \
  long x, y;                                                    \
                                                                \
  /* Create Yorick array. */                                    \
  if (sizeof(DST_TYPE) < sizeof(SRC_TYPE)) {                    \
    y_error("sizeof("#DST_TYPE") < sizeof("#SRC_TYPE")");       \
  }                                                             \
  dims[0] = 2;                                                  \
  dims[1] = cam->frame_width;                                   \
  dims[2] = cam->frame_height;                                  \
  dst = (DST_TYPE*)PUSH(dims);                                  \
                                                                \
  if (sizeof(SRC_TYPE) == sizeof(DST_TYPE)) {                   \
    /* Source and destination pixels have same size. */         \
    size_t row_size = sizeof(DST_TYPE)*cam->frame_width;        \
    if (cam->row_stride == row_size) {                          \
      /* A single copy will do the job. */                      \
      memcpy(dst, src, cam->frame_height*row_size);             \
    } else {                                                    \
      /* Copy row by row. */                                    \
      for (y = 0; y < cam->frame_height; ++y) {                 \
        memcpy(dst + y*cam->frame_width,                        \
               src + y*cam->row_stride,                         \
               row_size);                                       \
      }                                                         \
    }                                                           \
  } else {                                                      \
    /* A conversion is needed, copy pixel by pixel. */          \
    for (y = 0; y < cam->frame_height; ++y) {                   \
      DST_TYPE* dst_row = dst + y*cam->frame_width;             \
      const SRC_TYPE* src_row =                                 \
        (const SRC_TYPE*)(src + y*cam->row_stride);             \
      for (x = 0; x < cam->frame_width; ++x) {                  \
        dst_row[x] = src_row[x];                                \
      }                                                         \
    }                                                           \
  }                                                             \
}
FUNCTION(extract_Mono8,  unsigned char,  ypush_c, uint8_t)
FUNCTION(extract_Mono12, unsigned short, ypush_s, uint16_t)
FUNCTION(extract_Mono16, unsigned short, ypush_s, uint16_t)
FUNCTION(extract_Mono32, unsigned int,   ypush_i, uint32_t)
#undef FUNCTION

#define FUNCTION(FORMAT)                                                \
static void                                                             \
extract_##FORMAT(const camera_t* cam, const unsigned char* src)         \
{                                                                       \
  static int warn = TRUE;                                               \
  if (warn) {                                                           \
    warning(#FORMAT " pixels will be extracted as raw data.");          \
    warn = FALSE;                                                       \
  }                                                                     \
  extract_Raw(cam, src);                                                \
}
FUNCTION(RGB8Packed)
FUNCTION(Mono12Coded)
FUNCTION(Mono12codedPacked)
FUNCTION(Mono12parallel)
FUNCTION(Mono12PackedParallel)
#undef FUNCTION

#define EXTRACTLOWPACKED(ptr)  ((ptr[0] << 4) | (ptr[1] & 0xF))
#define EXTRACTHIGHPACKED(ptr) ((ptr[2] << 4) | (ptr[1] >> 4))

static void
extract_Mono12Packed(const camera_t* cam, const unsigned char* src)
{
  unsigned short* dst;
  long dims[3];
  long y, even_width;
  int odd; /* number of colmuns is odd? */

  fprintf(stderr, "extract_Mono12Packed\n");

  /* Create Yorick array. */
  if (sizeof(short) < 2) {
    y_error("sizeof(short) < 2");
  }
  dims[0] = 2;
  dims[1] = cam->frame_width;
  dims[2] = cam->frame_height;
  dst = (unsigned short*)ypush_s(dims);

  odd = ((cam->frame_width & 1L) != 0L);
  even_width = (cam->frame_width & ~1L);

  for (y = 0; y < cam->frame_height; ++y) {
    const unsigned char* src_ptr = src + y*cam->row_stride;
    unsigned short* dst_ptr = dst + y*cam->frame_width;
    unsigned short* dst_end = dst_ptr + even_width;
    while (dst_ptr < dst_end) {
      dst_ptr[0] = EXTRACTLOWPACKED(src_ptr);
      dst_ptr[1] = EXTRACTHIGHPACKED(src_ptr);
      src_ptr += 3;
      dst_ptr += 2;
    }
    if (odd) {
      /* Extract last pixel of the row. */
      dst_ptr[0] = EXTRACTLOWPACKED(src_ptr);
    }
  }
}

#undef EXTRACTLOWPACKED
#undef EXTRACTHIGHPACKED
