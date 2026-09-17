# ESP-01 Time Tracker

**A one-button, Wi-Fi time clock that logs your hours straight into Google Sheets.**a

<img src="Docs/Device%20Body%20v8%20Transparent.png" alt="ESP-01 Time Tracker device" width="420"/>

</div>

---

## Overview

If you work remotely or bill by the hour, this little desk companion replaces the "did I remember to start the timer?" problem with a single button.

Press the button once and the device starts counting. Press it again when you are done, and the elapsed time is sent over Wi-Fi to your Google Sheet, where it is added to today's row automatically. Two LEDs tell you at a glance whether the clock is running, and the device also hosts a small web page on your network so you can start and stop it from your phone or laptop.

The enclosure is a 3D-printed Claw'd (the Claude mascot) — all CAD and STL files are included.

## Features

- **One-button operation** — press to start, press again to stop and log.
- **Automatic Google Sheets logging** — hours are added to the row matching today's date.
- **Built-in web dashboard** — live status and duration, plus START/STOP buttons, served from the ESP-01 itself.
- **NTP time sync** — the device gets the correct date on boot, no RTC module needed.
- **Visual feedback** — two LEDs indicate running vs. stopped.
- **Battery powered** — 1200 mAh LiPo with USB-C charging and protection.
- **Fully open hardware** — schematic, CAD, and print-ready STLs are all in the repo.

## Repository Layout

| Folder | Contents |
|---|---|
| `Code/espTimeTriacking/` | Arduino firmware for the ESP-01 |
| `Schematic/` | Wiring schematic (EasyEDA Pro source, PDF, and PNG) |
| `CAD/` | Fusion 360 archive and STEP export |
| `3D Print/STL/` | Print-ready STL files |
| `Docs/` | Renders and images |
| `Timesheet.xlsx` | Google Sheets timesheet template |

## Hardware

| Component | Notes |
|---|---|
| ESP-01 / ESP-01S (ESP8266) | Core Wi-Fi module and MCU |
| AMS1117-3.3 regulator | Supplies the 3.3 V rail |
| TP4056 charger (USB-C, with protection) | LiPo charging and battery protection |
| 1200 mAh LiPo battery | Power source |
| Push button | Start/stop trigger on GPIO0 |
| 2 × LED | Status feedback on GPIO2 |

### Wiring Diagram

The schematic was designed in EasyEDA Professional; the source, PDF, and PNG are all in the [Schematic](Schematic/) folder.

<img src="Schematic/ESP-01%20Time%20Tracker%20Schematic.png" alt="ESP-01 Time Tracker schematic" width="600"/>

> **How the two LEDs work:** both share GPIO2 and are wired in opposite polarity, so exactly one is lit at any time — one colour for *recording*, the other for *stopped*. Pick whichever two colours you prefer.

## 3D Printing

All parts are in [3D Print/STL/](3D%20Print/STL/). The full assembly is also available as a 3MF project and as Fusion 360 / STEP files in [CAD/](CAD/).

| Part | Quantity |
|---|---|
| Base | 1 |
| BaseLid | 1 |
| Claw'd | 1 |
| Claw'dLid | 1 |
| ButtonCover | 1 |
| LightHolder | 2 |
| LightDiffuser | 2 |
| eye | 2 |

Print the **base parts at 50% infill** to add weight and keep the device stable on the desk. Everything else prints fine at default settings.

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/obadakatma/ESP01-Time-Tracker.git
```

### 2. Install ESP8266 board support in Arduino IDE

The ESP-01 isn't recognized by the Arduino IDE out of the box, so you need to add the ESP8266 board package first:

1. Open **File → Preferences** and add the following URL to **Additional Boards Manager URLs**:
   ```
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
2. Open **Tools → Board → Boards Manager**, search for `esp8266`, and install the **esp8266 by ESP8266 Community** package.
3. Select **Tools → Board → ESP8266 Boards → Generic ESP8266 Module**.

Full, up-to-date instructions (including macOS/Linux-specific steps) are available in the [official ESP8266 Arduino core documentation](https://arduino-esp8266.readthedocs.io/en/latest/installing.html#instructions).

### 3. Set up the Google Sheet

1. Import `Timesheet.xlsx` into Google Sheets.
2. Open **Extensions → Apps Script** and paste in the following script:

```javascript
function doGet(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheets()[0];
  var data = sheet.getDataRange().getValues();
  var dateParam = e.parameter.date;
  var incomingHours = parseFloat(e.parameter.hours);

  if (isNaN(incomingHours)) {
    return ContentService.createTextOutput("ERROR: invalid hours value");
  }

  var dateCol = -1, hoursCol = -1, headerRow = -1;

  for (var r = 0; r < data.length; r++) {
    for (var c = 0; c < data[r].length; c++) {
      if (data[r][c] === "DATE(S)") { dateCol = c; headerRow = r; }
      if (data[r][c] === "HOURS WORKED") { hoursCol = c; }
    }
  }

  if (dateCol === -1 || hoursCol === -1) {
    return ContentService.createTextOutput("ERROR: headers not found");
  }

  for (var r = headerRow + 1; r < data.length; r++) {
    var cellVal = data[r][dateCol];
    var cellDateStr = formatDateForMatch(cellVal);
    if (cellDateStr === dateParam) {
      var targetCell = sheet.getRange(r + 1, hoursCol + 1);
      var existingVal = parseFloat(targetCell.getValue());
      if (isNaN(existingVal)) existingVal = 0;

      var newTotal = existingVal + incomingHours; // plain add, no rounding here
      targetCell.setValue(newTotal);

      return ContentService.createTextOutput(
        "OK: row " + (r + 1) + " = " + existingVal + " + " + incomingHours + " = " + newTotal
      );
    }
  }

  return ContentService.createTextOutput("ERROR: date not found -> " + dateParam);
}

function formatDateForMatch(d) {
  if (Object.prototype.toString.call(d) === '[object Date]') {
    return (d.getMonth() + 1) + "/" + d.getDate() + "/" + (d.getFullYear() % 100);
  }
  return String(d);
}
```

3. Click **Deploy → New deployment**, choose **Web app** as the type, set **Who has access** to **Anyone**, and deploy.
4. Copy the resulting web app URL — you will need it in the next step.

> The script looks for the `DATE(S)` and `HOURS WORKED` headers and adds the incoming hours to the row whose date matches today. Make sure the sheet already contains a row for the current date, otherwise the device receives `ERROR: date not found`.

### 4. Configure the firmware

Open [Code/espTimeTriacking/espTimeTriacking.ino](Code/espTimeTriacking/espTimeTriacking.ino) in the Arduino IDE and update the three constants at the top:

```cpp
const char* ssid      = "YOUR_WIFI_SSID";
const char* password  = "YOUR_WIFI_PASSWORD";
const char* scriptURL = "https://script.google.com/macros/s/YOUR_SCRIPT_ID_HERE/exec";
```

**Time zone:** the device syncs its clock over NTP, which returns UTC by default. This offset must be set correctly, otherwise the date used to log your hours will be wrong whenever you work close to midnight in your own time zone — for example, without the offset, working at 12:30 AM local time could be recorded against yesterday's date (or tomorrow's) instead of today's, and the Apps Script would then reject it with `ERROR: date not found` since that row doesn't exist yet. Edit this line to match your own zone — the first argument is your offset in seconds, so UTC+3 is `3 * 3600` and UTC−3 is `-3 * 3600`:

```cpp
configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
```

### 5. Flash the ESP-01

An Arduino board can be used as a USB-to-serial bridge. Wire the ESP-01 on a breadboard as follows:

| ESP-01 pin | Connect to |
|---|---|
| VCC, EN (CH_PD) | 3.3 V |
| GND | GND |
| RST | Push button to GND |
| GPIO0 (IO0) | GND (puts the module into programming mode) |
| TX | Arduino TX |
| RX | Arduino RX |

Also connect the **Arduino's RST pin to GND** so the Arduino's own microcontroller is bypassed and only its USB-to-serial chip is used.

> ⚠️ The ESP-01 runs on **3.3 V** — do not connect it to the Arduino's 5 V rail.

Then:

1. Click **Upload** in the Arduino IDE, and at the same time hold the ESP-01 RST button.
2. When the IDE prints `Connecting.....____.....___`, release the button — the upload should begin.
3. Once it finishes, disconnect GPIO0 from GND and press RST again.
4. Open the Serial Monitor at **115200 baud** to watch the device connect to Wi-Fi and print its IP address.

> **Tip:** assign the device a static IP in your router settings so the web dashboard is always at the same address.

## Usage

**With the button:** one press starts recording, the next press stops it and uploads the elapsed hours to your sheet. The LEDs show which state you are in.

**With the web dashboard:** browse to the device's IP address on the same network to see the live status and duration, and to start or stop recording remotely.

Note that the same GPIO0 pin is used both for the button and for entering flash mode, so keep the button released while the device boots.

## Contributing

Contributions are welcome — a few areas that would be especially useful:

- Support for other time-tracking backends (Notion, Airtable, a self-hosted API, etc.) alongside Google Sheets.
- Enclosure variants or remixes of the Claw'd design.
- Battery life improvements (e.g. deep-sleep between button presses).
- Bug reports for wiring or flashing issues on different ESP-01/ESP-01S revisions.

For anything beyond a small fix, please open an issue first to discuss the change before submitting a pull request.

## License

This project is licensed under the [MIT License](LICENSE).

## Acknowledgments

- Schematic designed in EasyEDA Professional; enclosure modelled in Fusion 360.
- Enclosure inspired by Claw'd, the Claude mascot.
