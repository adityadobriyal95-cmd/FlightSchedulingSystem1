# FlightOps — Flight Scheduling & Airport Management System (C++17 & MySQL)

A full-stack, GitHub-ready academic project built with **C++17**, **MySQL Connector/C++**, **MySQL 8.0**, and a responsive **HTML5/CSS3/Vanilla JS** operations dashboard.

---

## 1. Key Features

1. **Admin Authentication & Session Management**
   - SHA-256 credential verification against MySQL (`admins` table) using prepared statements.
   - Bearer session tokens with automatic expiration and logout invalidation.
2. **Airport, Runway & Gate Infrastructure Management**
   - Complete CRUD for airports (IATA/ICAO codes, timezones, operational status).
   - Automatic provisioning of default runway (`09/27`) and gate (`A1`), plus custom runway/gate creation.
3. **Aircraft Fleet & Availability Tracking**
   - Full CRUD with real-time status tracking (`AVAILABLE`, `IN_FLIGHT`, `MAINTENANCE`, `RETIRED`) and active flight counts.
4. **Conflict-Free Flight Scheduling Engine**
   - Multi-criteria search & filtering (flight number, airline, city, status, origin airport).
   - Automated C++ `ConflictDetector` enforcing:
     - **30-minute aircraft turnaround buffer** (blocks double-booking the same tail number).
     - **45-minute terminal gate turnaround window** (prevents gate collisions).
     - **15-minute ATC runway separation buffer** (prevents simultaneous takeoffs/landings on the same runway).
5. **Passenger & Seat-Locked Booking Management**
   - Interactive cabin seat map displaying occupied vs. available seats.
   - Dual-layer duplicate seat prevention: application validation + MySQL generated column unique constraint (`uq_active_flight_seat`).
   - Cancelling a booking immediately releases the seat for rebooking.
6. **CPU/ATC Scheduling Algorithms Simulator**
   - Implements **FCFS**, **SJF (Non-Preemptive)**, **Priority Scheduling**, and **Round Robin (with configurable time quantum)** in C++.
   - Runs on either **live MySQL flights** or a **custom flight queue**.
   - Renders an interactive **Gantt Chart**, per-flight metrics (`AT`, `BT`, `CT`, `TAT`, `WT`), **Average Waiting Time**, **Average Turnaround Time**, and **Runway Utilization**.
7. **Executive Reports & Analytics**
   - Real-time KPI summaries (airports, aircraft, total/cancelled flights, passengers, confirmed/cancelled bookings, gross revenue, and status breakdowns).

---

## 2. Project Folder Structure

```text
FlightSchedulingSystem/
├── .gitignore
├── CMakeLists.txt
├── README.md
├── config/
│   └── db_config.env.example       # Template for MySQL credentials (copy to db_config.env)
├── database/
│   ├── schema.sql                  # Complete flight_ops MySQL schema, FKs, indexes & constraints
│   └── sample_data.sql             # Pre-populated demo dataset (admins, airports, fleet, flights)
├── backend/
│   ├── include/                    # C++17 headers (Config, Database, Models, Utils, ConflictDetector, FlightScheduler, ApiRoutes)
│   ├── src/                        # C++17 implementation files + main.cpp
│   └── third_party/                # Bundled cross-platform HTTP/1.1 server (httplib.h) & JSON library (json.hpp)
├── frontend/
│   ├── login.html                  # Admin login screen
│   ├── index.html                  # Aviation-themed responsive dashboard
│   ├── css/styles.css              # Navy blue, crisp white & light grey stylesheet
│   └── js/                         # Modular frontend controllers (api.js, auth.js, app.js, bookings.js, scheduler.js)
└── docs/
    ├── ARCHITECTURE.md
    ├── API_DOCUMENTATION.md
    └── TESTING_GUIDE.md
```

---

## 3. Windows 10/11 Prerequisites & Compiler Compatibility

> **Important Compiler Compatibility Note:**
> Legacy **MinGW GCC 6.3.0** uses an older C++ ABI and lacks complete C++17 `<filesystem>` / threading support required by modern **MySQL Connector/C++ 8.4 / 9.x**.
> Use **one** of the two compatible toolchains below on Windows 10/11:

### Option A (Recommended): MSVC (Visual Studio 2022 / Build Tools) + Official MySQL Connector/C++ 8.4/9.x
1. Install **MySQL Server 8.0+** and **MySQL Workbench** via the MySQL Installer for Windows.
2. Install **MySQL Connector/C++ 8.4** (or **9.x**) 64-bit Windows ZIP/MSI into:
   `C:\Program Files\MySQL\MySQL Connector C++ 8.4`
3. Install **Visual Studio 2022** (or *Build Tools for Visual Studio 2022* with *"Desktop development with C++"* and **CMake**).

### Option B: MSYS2 UCRT64 (GCC 13/14)
1. Install **MSYS2** (`C:\msys64`) and open the **MSYS2 UCRT64** terminal.
2. Run:
   ```bash
   pacman -Syu
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-mysql-connector-cpp
   ```

---

## 4. Database Setup (MySQL Workbench or CLI)

1. Open **MySQL Workbench** and connect to your local MySQL Server (`127.0.0.1:3306`).
2. Open and execute `database/schema.sql` (creates the `flight_ops` database and all 8 tables).
3. Open and execute `database/sample_data.sql` (loads demo admins, 6 airports, 12 runways, 13 gates, 8 aircraft, 8 flights, 8 passengers, and 11 bookings).

Or via Windows Command Prompt:
```cmd
mysql -u root -p < database\schema.sql
mysql -u root -p < database\sample_data.sql
```

---

## 5. Secure Credential Configuration (No Hardcoded Passwords)

1. Copy `config/db_config.env.example` to `config/db_config.env`:
   ```cmd
   copy config\db_config.env.example config\db_config.env
   ```
2. Edit `config/db_config.env` and set your MySQL password:
   ```env
   DB_HOST=tcp://127.0.0.1:3306
   DB_USER=root
   DB_PASS=YourActualMySQLPassword
   DB_NAME=flight_ops
   SERVER_HOST=0.0.0.0
   SERVER_PORT=8080
   FRONTEND_DIR=frontend
   ```
   *(Note: `config/db_config.env` is listed in `.gitignore` so your password is never pushed to GitHub. You may also override any setting with Windows environment variables `DB_PASS`, `DB_USER`, etc.)*

---

## 6. Windows Build & Run Instructions

### Building with MSVC + CMake (x64 Native Tools Command Prompt for VS 2022)
```cmd
cd FlightSchedulingSystem
cmake -B build -G "Visual Studio 17 2022" -A x64 -DMYSQL_CONNECTOR_DIR="C:/Program Files/MySQL/MySQL Connector C++ 8.4"
cmake --build build --config Release
```

**Copy Required MySQL Connector DLLs next to the executable** (from `C:\Program Files\MySQL\MySQL Connector C++ 8.4\lib64\`):
```cmd
copy "C:\Program Files\MySQL\MySQL Connector C++ 8.4\lib64\mysqlcppconn*.dll" build\Releasecopy "C:\Program Files\MySQL\MySQL Connector C++ 8.4\lib64\libcrypto-*.dll" build\Releasecopy "C:\Program Files\MySQL\MySQL Connector C++ 8.4\lib64\libssl-*.dll" build\Release```

**Run the Server from the project root:**
```cmd
build\Releaselight_ops_server.exe
```

### Building with MSYS2 UCRT64
```bash
cd /c/path/to/FlightSchedulingSystem
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/flight_ops_server.exe
```

---

## 7. Accessing the Application & Demo Credentials

1. Open your browser and navigate to:
   **`http://localhost:8080/login.html`**
2. Sign in using either demo account:
   - **Super Admin:** `admin` / `Admin@123`
   - **Operations Manager:** `opslead` / `Ops@2026`
