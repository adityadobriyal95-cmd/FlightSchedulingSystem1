# System Architecture — FlightOps

## 1. Three-Tier Architecture
- **Presentation Tier (`frontend/`)**: Responsive Single-Page Operations Dashboard built with semantic HTML5, modern CSS Grid/Flexbox, and modular ES6 JavaScript (`api.js`, `auth.js`, `app.js`, `bookings.js`, `scheduler.js`). Communicates strictly via asynchronous `fetch()` JSON REST calls with `Authorization: Bearer <token>` headers.
- **Application & Algorithm Tier (`backend/`)**: Multi-threaded C++17 HTTP/1.1 REST server (`httplib.h`) serving both `/api/*` endpoints and static frontend assets.
  - `Config`: Loads `config/db_config.env` and environment variable overrides.
  - `Database`: Thread-safe connection factory wrapping `sql::mysql::MySQL_Driver`.
  - `ConflictDetector`: Enforces aircraft turnaround (30m), gate turnaround (45m), and runway wake-turbulence separation (15m) before persisting flights.
  - `FlightScheduler`: Implements FCFS, SJF, Priority Scheduling, and Round Robin algorithms, computing exact Gantt intervals, Completion Time, Turnaround Time, Waiting Time, and Runway Utilization.
- **Data Tier (`database/schema.sql`)**: Normalized MySQL 8.0 InnoDB schema (`flight_ops`) utilizing foreign key referential integrity, check constraints, composite indexes, and a generated-column unique index (`uq_active_flight_seat`) to prevent duplicate active seat assignments at the storage engine level.
