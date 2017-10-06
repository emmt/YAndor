plug_dir, ".";
include, "./andor.i";

cam = andor_open(0);

write, format="Frame size = %d bytes\n",
  andor_get_int(cam, "ImageSizeBytes");
write, format="Frame width = %d pixels\n",
  andor_get_width(cam);
write, format="Frame height = %d pixels\n",
  andor_get_height(cam);

// Get a single image:
img = andor_get_single(cam);
window, 0; fma; pli, img;

// Get a sequence of images (and display the first and last one):
ptr = andor_get_sequence(cam, 50);
window, 1; fma; pli, *ptr(1);
window, 2; fma; pli, *ptr(0);

write, format="cam.acquiring -----------> %d\n", cam.acquiring;
write, format="cam.device --------------> %d\n", cam.device;
write, format="cam.queue_length --------> %d\n", cam.queue_length;
write, format="cam.buffer --------------> %d\n", cam.buffer;
write, format="cam.buffer_size ---------> %d\n", cam.buffer_size;
write, format="cam.frame_size ----------> %d\n", cam.frame_size;
write, format="cam.frame_width ---------> %d\n", cam.frame_width;
write, format="cam.frame_height --------> %d\n", cam.frame_height;
write, format="cam.row_stride ----------> %d\n", cam.row_stride;

i = andor_get_enum_index(cam, "PixelEncoding");
s = andor_get_enum_string_by_index(cam, "PixelEncoding", i);
write, format="PixelEncoding -----------> %s\n", s;
