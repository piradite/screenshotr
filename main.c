#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <png.h>

#define THUMBNAIL_PATH "/tmp/thumb.png"
#define min(a, b) (((a) < (b)) ? (a) : (b))

void handle_error(const char *msg, int code) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(code);
}

XImage *capture_screen(Display *d, Window w, int x, int y, int width, int height) {
    return XGetImage(d, w, x, y, width, height, AllPlanes, ZPixmap);
}

void save_image_to_png(XImage *i, FILE *fp) {
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        handle_error("Failed to create PNG structure", 1);
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        handle_error("Failed to create PNG info structure", 1);
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        handle_error("Failed during PNG creation", 1);
    }

    png_init_io(png, fp);

    png_set_compression_level(png, 0);

    png_set_IHDR(png, info, i->width, i->height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_BASE);
    png_write_info(png, info);

    png_bytep row = (png_bytep)malloc(i->width * 3 * sizeof(png_byte));
    if (!row) {
        png_destroy_write_struct(&png, &info);
        handle_error("Failed to allocate memory for row", 1);
    }

    for (int y = 0; y < i->height; y++) {
        png_bytep ptr = row;
        for (int x = 0; x < i->width; x++) {
            unsigned long pixel = XGetPixel(i, x, y);
            *ptr++ = (pixel >> 16) & 0xFF;
            *ptr++ = (pixel >> 8) & 0xFF;
            *ptr++ = pixel & 0xFF;
        }
        png_write_row(png, row);
    }

    free(row);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
}

void save_thumbnail_image(XImage *i) {
    FILE *fp = fopen(THUMBNAIL_PATH, "wb");
    if (!fp) {
        handle_error("Failed to open file for writing thumbnail", 1);
    }

    int thumb_width = i->width / 16;
    int thumb_height = i->height / 16;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        handle_error("Failed to create PNG structure for thumbnail", 1);
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        handle_error("Failed to create PNG info structure for thumbnail", 1);
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        handle_error("Failed during PNG creation for thumbnail", 1);
    }

    png_init_io(png, fp);

    png_set_compression_level(png, 0);

    png_set_IHDR(png, info, thumb_width, thumb_height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_BASE);
    png_write_info(png, info);

    png_bytep row = (png_bytep)malloc(thumb_width * 3 * sizeof(png_byte));
    if (!row) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        handle_error("Failed to allocate memory for thumbnail row", 1);
    }

    for (int y = 0; y < thumb_height; y++) {
        png_bytep ptr = row;
        for (int x = 0; x < thumb_width; x++) {
            int orig_x = x * 16;
            int orig_y = y * 16;
            unsigned long pixel = XGetPixel(i, orig_x, orig_y);
            *ptr++ = (pixel >> 16) & 0xFF;
            *ptr++ = (pixel >> 8) & 0xFF;
            *ptr++ = pixel & 0xFF;
        }
        png_write_row(png, row);
    }

    free(row);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
}

void copy_to_clipboard(XImage *i) {
    FILE *fp = popen("xclip -selection clipboard -t image/png", "w");
    if (!fp) {
        handle_error("Failed to open pipe to xclip", 1);
    }

    save_image_to_png(i, fp);

    pclose(fp);
}

void notify_user(const char *message) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "notify-send -u low -t 1500 -i %s 'Screenshot' '%s'", THUMBNAIL_PATH, message);
    system(cmd);
}

void screenshot_whole_screen(Display *d) {
    XImage *i = capture_screen(d, DefaultRootWindow(d), 0, 0, DisplayWidth(d, DefaultScreen(d)), DisplayHeight(d, DefaultScreen(d)));
    save_thumbnail_image(i);
    notify_user("Screenshot taken");
    copy_to_clipboard(i);
    XDestroyImage(i);
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
    Window w = get_active_window(d);
    XWindowAttributes attr;
    XGetWindowAttributes(d, w, &attr);
    XImage *i = capture_screen(d, w, 0, 0, attr.width, attr.height);
    save_thumbnail_image(i);
    notify_user("Screenshot taken");
    copy_to_clipboard(i);
    XDestroyImage(i);
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
        notify_user("Screenshot has been cancelled");
        exit(0);
    }

    x2 = event.xbutton.x_root;
    y2 = event.xbutton.y_root;

    XUngrabPointer(d, CurrentTime);
    XFreeCursor(d, cursor);

    width = abs(x2 - x1);
    height = abs(y2 - y1);
    int start_x = x1 < x2 ? x1 : x2;
    int start_y = y1 < y2 ? y1 : y2;

    XImage *i = capture_screen(d, root, start_x, start_y, width, height);
    save_thumbnail_image(i);
    notify_user("Screenshot taken");
    copy_to_clipboard(i);
    XDestroyImage(i);
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

    if ((now_flag && delay > 0) || (now_flag && active_flag) || (now_flag && select_flag) || (delay > 0 && active_flag) || (delay > 0 && select_flag) || (active_flag && select_flag)) {
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
