# ST7789 Image Diagnostic Specification

## Purpose

The project MUST offer a standalone diagnostic that paints a known RGB565
pattern on the ST7789 panel of the ESP32-S3 carrier, and MUST offer the
software evidence that the pattern is correct. The diagnostic MUST stay
outside the gateway firmware, and MUST NOT claim any physical result.

## Requirements

### Requirement: Standalone diagnostic application

The repository MUST contain a self-contained ESP-IDF project under
`tools/lcd-smoke/` that builds for the `esp32s3` target and depends on
nothing beyond the ESP-IDF components that ship with the toolchain. It MUST
NOT modify the gateway firmware, the production components or the shared
gateway test project.

#### Scenario: The diagnostic is built on its own

- GIVEN a developer with ESP-IDF v5.4 installed
- WHEN they run `idf.py build` inside `tools/lcd-smoke/`
- THEN the application builds without a `set-target` step
- AND no file outside `tools/lcd-smoke/` is needed to build it

#### Scenario: The gateway stays untouched

- GIVEN the diagnostic is present in the repository
- WHEN the gateway test project is inspected
- THEN the gateway components contain no reference to the diagnostic
- AND the gateway test project registers no diagnostic test component

### Requirement: Panel wiring and bus

The diagnostic MUST drive the panel over SPI2 at 10 MHz, with MOSI on GPIO11,
SCLK on GPIO12, data/command on GPIO8, reset on GPIO9 and chip select on
GPIO10. It MUST NOT configure GPIO7, the backlight line.

#### Scenario: The diagnostic initialises the panel

- GIVEN the board is powered and wired as the hardware profile describes
- WHEN the diagnostic starts
- THEN it initialises the SPI bus, the panel IO and the ST7789 controller
- AND it applies the configured gap, inversion and colour order
- AND it enables the display before it draws

### Requirement: One frame, one draw

The diagnostic MUST allocate exactly one frame buffer of 170 x 320 x 2 bytes
in memory the DMA can read, MUST keep it valid until the board is reset, and
MUST hand the frame to the driver once.

#### Scenario: The frame buffer outlives the program

- GIVEN the diagnostic has drawn the frame
- WHEN `app_main` returns
- THEN the frame buffer is still allocated and unchanged
- AND the program does not wait for the end of the transfer

#### Scenario: The program reports the queue, not the picture

- GIVEN the frame has been handed to the panel IO
- WHEN the program logs its result
- THEN it states that the frame was queued and that the visual confirmation
  is pending
- AND it does not state that the panel shows the image

### Requirement: The test pattern

The pattern MUST consist of five horizontal bands of 64 rows each, red,
green, blue, white and black from top to bottom, over a frame of 170 x 320
RGB565 pixels, with a two pixel border of a colour that none of the bands
uses. The border MUST cover the bands it overlaps. The pixels MUST be written
with the high byte first.

#### Scenario: The bands carry the exact colours

- GIVEN a frame buffer filled by the pattern
- WHEN every pixel of every band is read
- THEN each band carries its own colour over its whole 64 rows

#### Scenario: The border wins over the bands

- GIVEN a frame buffer filled by the pattern
- WHEN the outermost two rows and columns are read
- THEN they carry the border colour
- AND the third pixel inwards carries the colour of the band it belongs to

#### Scenario: The wire order is high byte first

- GIVEN a frame buffer filled by the pattern
- WHEN the bytes of a red pixel are read
- THEN they are `0xF8` followed by `0x00`

### Requirement: The pattern refuses to write into an unusable buffer

The pattern MUST reject a null buffer and a buffer that is smaller than the
frame, and MUST NOT modify the memory of the caller in either case.

#### Scenario: An unusable buffer is refused

- GIVEN a frame buffer of the right size
- WHEN the pattern is asked to fill a null pointer or a buffer that is too
  small
- THEN the call returns an error
- AND every byte of the caller memory is unchanged

### Requirement: Host test of the pattern

The pattern MUST be covered by unit tests that run on the host, without a
board, and the same test file MUST be usable in an ESP-IDF test project.

#### Scenario: The tests run without hardware

- GIVEN a developer with ESP-IDF v5.4 installed
- WHEN they run `tools/lcd-smoke/run-host.sh`
- THEN the pattern tests run and report their result
- AND the command fails when a test fails

### Requirement: Provisional settings are labelled as such

The X gap, the Y gap, the colour inversion and the RGB or BGR order MUST be
configurable, and the documentation MUST state that they are software
defaults and not measured facts about the panel in hand. The SPI mode and
the reset polarity MUST be documented as assumptions.

#### Scenario: A reader knows what is verified

- GIVEN the documentation of the diagnostic
- WHEN a reader looks for the gap, the inversion and the colour order
- THEN they find the default value of each one
- AND they find that none of them is verified against the panel

### Requirement: Flashing is a manual decision

The documentation MUST give the exact commands to build, to test, to flash and
to monitor, and MUST leave the serial port as a placeholder that the reader
fills in. It MUST warn that flashing replaces the application on the board and
MUST say how to get the gateway firmware back.

#### Scenario: The reader chooses the port

- GIVEN a developer with the board connected
- WHEN they follow the flashing instructions
- THEN they have to set the port themselves
- AND no command of the diagnostic infers a port
