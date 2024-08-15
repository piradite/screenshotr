#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <png.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

void handle_error(const char *msg, int code) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(code);
}

XImage *capture_screen(Display *d, Window w, int x, int y, int width, int height) {
    return XGetImage(d, w, x, y, width, height, AllPlanes, ZPixmap);
}

void save_image_to_png_memory(XImage *image, png_bytep *buffer, size_t *length) {
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) handle_error("Failed to create PNG structure", 1);

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        handle_error("Failed to create PNG info structure", 1);
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        handle_error("Failed during PNG creation", 1);
    }

    // Memory buffer to write PNG data to
    png_set_write_fn(png, buffer, (png_write_callback)png_write_mem, (png_flush_callback)png_flush_mem);

    png_set_IHDR(png, info, image->width, image->height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    png_bytep row_pointers[image->height];
    for (int y = 0; y < image->height; y++) {
        row_pointers[y] = (png_bytep)malloc(image->width * 3 * sizeof(png_byte));
        if (!row_pointers[y]) handle_error("Failed to allocate memory for row", 1);

        for (int x = 0; x < image->width; x++) {
            unsigned long pixel = XGetPixel(image, x, y);
            int offset = x * 3;
            row_pointers[y][offset] = (pixel >> 16) & 0xFF;
            row_pointers[y][offset + 1] = (pixel >> 8) & 0xFF;
            row_pointers[y][offset + 2] = pixel & 0xFF;
        }
    }

    png_write_image(png, row_pointers);

    for (int y = 0; y < image->height; y++) free(row_pointers[y]);

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);

    *length = *buffer;
}

void screenshot_whole_screen(Display *d) {
    XImage *image = capture_screen(d, DefaultRootWindow(d), 0, 0,
                                   DisplayWidth(d, DefaultScreen(d)),
                                   DisplayHeight(d, DefaultScreen(d)));

    png_bytep buffer;
    size_t length;
    save_image_to_png_memory(image, &buffer, &length);
    XDestroyImage(image);

    // Use xclip to copy the buffer to the clipboard
    FILE *pipe = popen("xclip -selection clipboard -t image/png", "wb");
    if (!pipe) handle_error("Failed to open pipe to xclip", 1);
    fwrite(buffer, 1, length, pipe);
    pclose(pipe);

    free(buffer);
}

Window get_active_window(Display *d) {
    XEvent event;
    Window root = DefaultRootWindow(d);
    Cursor cursor = XCreateFontCursor(d, XC_crosshair);

    XGrabPointer(d, root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
    XMaskEvent(d, ButtonPressMask, &event);

    Window clicked_window = event.xbutton.subwindow ? event.xbutton.subwindow : root;
    XUngrabPointer(d, CurrentTime);
    XFreeCursor(d, cursor);

    return clicked_window;
}

void screenshot_active_window(Display *d) {
    Window window = get_active_window(d);
    XWindowAttributes attr;
    XGetWindowAttributes(d, window, &attr);

    XImage *image = capture_screen(d, window, 0, 0, attr.width, attr.height);

    png_bytep buffer;
    size_t length;
    save_image_to_png_memory(image, &buffer, &length);
    XDestroyImage(image);

    // Use xclip to copy the buffer to the clipboard
    FILE *pipe = popen("xclip -selection clipboard -t image/png", "wb");
    if (!pipe) handle_error("Failed to open pipe to xclip", 1);
    fwrite(buffer, 1, length, pipe);
    pclose(pipe);

    free(buffer);
}

void screenshot_selected_area(Display *d) {
    XEvent event;
    int x1, y1, x2, y2, width, height;
    Window root = DefaultRootWindow(d);
    Cursor cursor = XCreateFontCursor(d, XC_crosshair);

    XGrabPointer(d, root, False, ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
    XMaskEvent(d, ButtonPressMask, &event);
    x1 = event.xbutton.x_root;
    y1 = event.xbutton.y_root;

    XMaskEvent(d, ButtonReleaseMask, &event);
    if (event.xbutton.x_root == x1 && event.xbutton.y_root == y1) {
        system("notify-send -u low -t 2000 -i i 'Screenshot' 'Screenshot has been cancelled'");
        exit(0);
    }

    x2 = event.xbutton.x_root;
    y2 = event.xbutton.y_root;

    XUngrabPointer(d, CurrentTime);
    XFreeCursor(d, cursor);

    width = abs(x2 - x1);
    height = abs(y2 - y1);
    int start_x = min(x1, x2);
    int start_y = min(y1, y2);

    XImage *image = capture_screen(d, root, start_x, start_y, width, height);

    png_bytep buffer;
    size_t length;
    save_image_to_png_memory(image, &buffer, &length);
    XDestroyImage(image);

    // Use xclip to copy the buffer to the clipboard
    FILE *pipe = popen("xclip -selection clipboard -t image/png", "wb");
    if (!pipe) handle_error("Failed to open pipe to xclip", 1);
    fwrite(buffer, 1, length, pipe);
    pclose(pipe);

    free(buffer);
}

int main(int argc, char *argv[]) {
    int delay = 0;
    int now_flag = 0;
    int active_flag = 0;
    int select_flag = 0;

    if (argc == 1 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [--now, -n | --inX | --active | --select]\n", argv[0]);
        printf("Options:\n");
        printf("  --now, -n: Take screenshot immediately.\n");
        printf("  --inX: Take screenshot after X seconds.\n");
        printf("  --active: Take screenshot of active window after clicking on it.\n");
        printf("  --select: Select area of the screen to capture using the mouse.\n");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--now", 5) == 0 || strncmp(argv[i], "-n", 2) == 0) {
            now_flag = 1;
        } else if (strncmp(argv[i], "--in", 4) == 0) {
            delay = atoi(argv[i] + 4);
            if (delay <= 0) handle_error("Invalid delay value", 1);
        } else if (strncmp(argv[i], "--active", 8) == 0) {
            active_flag = 1;
        } else if (strncmp(argv[i], "--select", 8) == 0) {
            select_flag = 1;
        } else {
            handle_error("Invalid argument", 1);
        }
    }

    if ((now_flag && delay > 0) || (now_flag && active_flag) || (now_flag && select_flag) ||
        (delay > 0 && active_flag) || (delay > 0 && select_flag) || (active_flag && select_flag)) {
        handle_error("Invalid combination of flags", 1);
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) handle_error("Failed to open display", 1);

    if (now_flag) {
        screenshot_whole_screen(display);
    } else if (delay > 0) {
        sleep(delay);
        screenshot_whole_screen(display);
    } else if (active_flag) {
        screenshot_active_window(display);
    } else if (select_flag) {
        screenshot_selected_area(display);
    }

    XCloseDisplay(display);
    return 0;
}
