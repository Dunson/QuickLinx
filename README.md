# QuickLinx

A fast, lightweight utility for modifying RSLinx Classic Ethernet driver configurations.

<img width="666" height="203" alt="be27567e-7ae7-4b67-9146-cd4ee5e28563" src="https://github.com/user-attachments/assets/f2d6f277-ae3b-4819-994b-3ba38a05a5b3" />

## Overview

QuickLinx is a specialized desktop utility designed to streamline management of RSLinx Classic Ethernet driver configurations.

It provides a safe, intuitive interface for:

- Exporting existing RSLinx driver configurations to CSV
- Importing new configuration files from CSV
- Merging imported drivers with the current configuration
- Overwriting existing configurations when required

QuickLinx is intended for technicians, controls engineers, and automation professionals who need a rapid and reliable way to manage RSLinx Classic Ethernet drivers.

---

## Developer Notes

QuickLinx is currently in an **alpha** state. Before using it on a production system:

- Back up your current Windows Registry so you have a restore point if any unexpected behavior occurs.
- Ensure that **RSLinx Classic is fully closed**, including background services.
  - For best results, use QuickLinx before attempting to connect to controllers using Logix5000, Studio 5000, etc.
  - If you forget to close RSLinx Classic before using QuickLinx, close or kill the RSLinx process (via Task Manager) and then restart it. When RSLinx relaunches, the updated drivers will be visible.

Additional constraints and behavior:

- The only supported driver type is **AB_ETH**.
- Driver names must not exceed the **15-character** limit.
- IP range values must be in a valid format, for example:
  - Invalid: `127.0.0`, `127.0.0.1.1`, `127.0.0.1-`, `127.0.0.1-256`, `127.0.0.15-4`
  - Valid: `127.0.0.1`, `127.0.1.1-15`, `127.0.0.1-254`
- The maximum number of nodes per driver is **254** (255 total minus one reserved station).

*– BRD*

---

## To Build from Source

### Prerequisites

- Windows 10 or later (64-bit)
- Visual Studio 2026 (or a compatible Visual Studio version) with:
  - MSVC toolchain (x64)
- Qt 6 (Widgets) installed with an MSVC-compatible kit
- Git (optional, for cloning the repository)

> Note: QuickLinx is currently developed and built using Visual Studio project files. CMake build files are not provided at this time.

### Build Steps

1. **Clone the repository**

   ```bash
   git clone https://github.com/your-username/QuickLinx.git
   cd QuickLinx
   ```

2. **Install and configure Qt**

   - Install Qt 6 with the MSVC x64 kit.
   - Ensure the Qt Visual Studio integration or environment variables are configured so that Visual Studio can locate Qt includes, libraries, and tools.

3. **Open the solution**

   - Open the `.sln` file (for example, `QuickLinx.sln`) in Visual Studio.

4. **Select configuration**

   - Set the build configuration to `Release` and the platform to `x64`.

5. **Build the project**

   - From the Visual Studio menu:  
     `Build` → `Build Solution`

6. **Locate the executable**

   - After a successful build, the QuickLinx executable will be located in your configured output directory (typically something like `x64\Release\QuickLinx.exe` within the project directory).

7. **Deploy Qt runtime (if needed)**

   - On systems without a full Qt installation, ensure the required Qt DLLs are deployed alongside `QuickLinx.exe` (this can be done using `windeployqt` or an equivalent deployment process).

---

## How to Use

To get started with QuickLinx:

1. **Close RSLinx Classic**

   - Close or kill all RSLinx Classic processes, ensuring it is not running as an application or service.

2. **Create a CSV file of Ethernet drivers**

   Use the following format:

   ```text
   Type,Name,Range
   AB_ETH,Example_One,127.0.0.1
   AB_ETH,Example_Two,127.0.1.1-50
   ...
   ```

3. **Import the CSV in QuickLinx**

   - Use the **Import** button to stage driver changes.
   - QuickLinx validates the integrity and format of the CSV automatically and will refuse to stage changes if errors are detected.

4. **Apply changes (Merge or Overwrite)**

   - **Merge**  
     Updates existing driver nodes or adds new drivers and nodes that do not already exist.  
     Does **not** delete any drivers or nodes.

   - **Overwrite**  
     For each driver name found in the CSV:
     - Overwrites all existing nodes with the nodes specified in the CSV.
     - Adds new drivers that do not already exist.

After applying changes, restart RSLinx Classic to load and view the updated driver configuration.

---

## Built Using

- C++
- Qt 6 (Widgets)
- Visual Studio 2026
- Windows Registry API

## Roadmap / Planned Features

The following items are under consideration for future releases:

- **Additional driver support**  
  Evaluate and add support for additional RSLinx Classic driver types where appropriate.

- **Integrated backup and restore**  
  One-click backup of relevant registry keys before changes are applied, with an option to restore a previous configuration.

- **Dry-run / preview mode**  
  Show a detailed summary of the staged changes that would be applied (new drivers, updated nodes, removed nodes) before committing them.

- **Logging and audit trail**  
  Optional logging of all operations (imports, merges, overwrites) for traceability and troubleshooting.

- **Usability improvements**  
  UI refinements, clearer status messages, and additional safeguards for production environments.




