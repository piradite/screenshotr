# ScreenshotR

A command-line tool for capturing screenshots on X11 systems.

## Features

- **Full Screen Capture**: Take a screenshot of the entire screen immediately or after a delay.
- **Active Window Capture**: Capture the currently active window after a click.
- **Area Selection**: Select a specific area of the screen to capture using the mouse.
- **Error Handling**: Handling of file operations and memory issues.
- **PNG Saving**: Screenshots are saved in PNG format and copied to the clipboard.
- **Notifications**: Receive a desktop notification upon saving a screenshot.

## Installation

To use this utility, you need to compile the source code. Ensure you have the necessary dependencies installed:

- `gcc` compiler
- `libX11`
- `libpng`

### Compilation

```bash
gcc -o screenshotr main.c -O3 -lX11 -lpng
```

### How To Use

After compilation, you can run the executable with the following options:

## Options

1. **Immediate Screenshot**: Use `--now` to capture the full screen without delay.
2. **Delayed Screenshot**: Use `--inX` followed by the delay in seconds to capture the full screen after the specified delay.
3. **Active Window**: Use `--active` to capture the window currently in focus after clicking on it.
4. **Selected Area**: Use `--select` to capture a user-defined area of the screen by dragging the mouse.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## How It Works

The utility interacts with the X11 display server to capture screenshots. It uses the `libX11` library to interface with the X server and `libpng` for saving images in PNG format. Notifications are displayed using the `notify-send` command, and screenshots are copied to the clipboard using `xclip`.

## Changelog

For detailed changes and updates, refer to the [CHANGELOG.md](CHANGELOG.md) file.

## Contributing

Contributions are welcome! If you have suggestions, bug reports, or would like to contribute code, please submit an issue or pull request via the GitHub repository.

## Contact

For further inquiries or support, you can reach out via:

- Email: oliviamaxw@gmail.com