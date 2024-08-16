#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <png.h>

#define THUMBNAIL_PATH "/tmp/thumb.png"
#define THUMBNAIL_SCALE 16

#define min(a, b) (((a) < (b)) ? (a) : (b))

void handle_error(const char *msg, int code) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(code);
}

int check_notify_send() {
    return system("command -v notify-send > /dev/null 2>&1") == 0;
}

XImage *capture_screen(Display *d, Window w, int x, int y, int width, int height) {
    return XGetImage(d, w, x, y, width, height, AllPlanes, ZPixmap);
}

void save_image_to_png(XImage *i, FILE *fp, int thumb) {
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) handle_error("PNG error", 1);

    png_init_io(png, fp);
    png_set_compression_level(png, 1);

    int scale = thumb ? THUMBNAIL_SCALE : 1;
    int width = i->width / scale, height = i->height / scale;
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_BASE);
    png_write_info(png, info);

    png_bytep row = malloc(width * 3);
    if (!row) handle_error("Memory error", 1);

    for (int y = 0; y < height; y++) {
        png_bytep ptr = row;
        for (int x = 0; x < width; x++) {
            unsigned long pixel = XGetPixel(i, x * scale, y * scale);
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
    if (!fp) handle_error("File error", 1);
    save_image_to_png(i, fp, 1);
    fclose(fp);
}

void copy_to_clipboard(XImage *i) {
    FILE *fp = popen("xclip -selection clipboard -t image/png", "w");
    if (!fp) handle_error("Pipe error", 1);
    save_image_to_png(i, fp, 0);
    pclose(fp);
}

void notify_user(const char *message) {
    if (check_notify_send()) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "notify-send -u low -t 1500 -i %s 'Screenshot' '%s'", THUMBNAIL_PATH, message);
        system(cmd);
    }
}

void screenshot_whole_screen(Display *d) {
    XImage *i = capture_screen(d, DefaultRootWindow(d), 0, 0, DisplayWidth(d, DefaultScreen(d)), DisplayHeight(d, DefaultScreen(d)));
    if (i) {
        save_thumbnail_image(i);
        notify_user("Screenshot taken");
        copy_to_clipboard(i);
        XDestroyImage(i);
    } else {
        handle_error("Failed to capture screen", 1);
    }
}

Window get_active_window(Display *d) {
    XEvent event;
    Window root = DefaultRootWindow(d);
    Cursor cursor = XCreateFontCursor(d, XC_crosshair);
    XGrabPointer(d, root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
    XMaskEvent(d, ButtonPressMask, &event);
    XUngrabPointer(d, CurrentTime);
    XFreeCursor(d, cursor);
    return event.xbutton.subwindow ? event.xbutton.subwindow : root;
}

void screenshot_active_window(Display *d) {
    Window w = get_active_window(d);
    XWindowAttributes attr;
    if (XGetWindowAttributes(d, w, &attr)) {
        XImage *i = capture_screen(d, w, 0, 0, attr.width, attr.height);
        if (i) {
            save_thumbnail_image(i);
            notify_user("Screenshot taken");
            copy_to_clipboard(i);
            XDestroyImage(i);
        } else {
            handle_error("Failed to capture active window", 1);
        }
    } else {
        handle_error("Failed to get window attributes", 1);
    }
}

void screenshot_selected_area(Display *d) {
    XEvent event;
    Window root = DefaultRootWindow(d);
    Cursor cursor = XCreateFontCursor(d, XC_crosshair);
    XGrabPointer(d, root, False, ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
    XMaskEvent(d, ButtonPressMask, &event);
    int x1 = event.xbutton.x_root, y1 = event.xbutton.y_root;
    XMaskEvent(d, ButtonReleaseMask, &event);
    int x2 = event.xbutton.x_root, y2 = event.xbutton.y_root;

    if (x2 == x1 && y2 == y1) {
        notify_user("Screenshot cancelled");
        exit(0);
    }

    int width = abs(x2 - x1), height = abs(y2 - y1);
    int start_x = min(x1, x2), start_y = min(y1, y2);

    XImage *i = capture_screen(d, root, start_x, start_y, width, height);
    if (i) {
        save_thumbnail_image(i);
        notify_user("Screenshot taken");
        copy_to_clipboard(i);
        XDestroyImage(i);
    } else {
        handle_error("Failed to capture selected area", 1);
    }
}

int main(int argc, char *argv[]) {
    int delay = 0, now_flag = 0, active_flag = 0, select_flag = 0;

    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [--now, -n | --inX | --active | --select]\n", argv[0]);
        printf("Options:\n");
        printf("  --now, -n: Take screenshot immediately.\n");
        printf("  --inX: Take screenshot after X seconds.\n");
        printf("  --active: Take screenshot of active window after clicking on it.\n");
        printf("  --select: Select area of the screen to capture using the mouse.\n");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--now") == 0 || strcmp(argv[i], "-n") == 0) now_flag = 1;
        else if (strncmp(argv[i], "--in", 4) == 0) delay = atoi(argv[i] + 4);
        else if (strcmp(argv[i], "--active") == 0) active_flag = 1;
        else if (strcmp(argv[i], "--select") == 0) select_flag = 1;
        else handle_error("Invalid argument", 1);
    }

    if ((now_flag && (delay || active_flag || select_flag)) ||
        (delay && (active_flag || select_flag)) ||
        (active_flag && select_flag)) handle_error("Invalid combination of flags", 1);

    Display *display = XOpenDisplay(NULL);
    if (!display) handle_error("Failed to open display", 1);

    if (now_flag) screenshot_whole_screen(display);
    else if (delay) { sleep(delay); screenshot_whole_screen(display); }
    else if (active_flag) screenshot_active_window(display);
    else if (select_flag) screenshot_selected_area(display);

    XCloseDisplay(display);
    return 0;
}
