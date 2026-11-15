# End-to-End Verification & Testing Guide

1. **Authentication Test**
   - Open `http://localhost:8080/login.html`.
   - Submit invalid credentials (`admin` / `wrong`) -> verify `401 Unauthorized` error alert.
   - Click **Auto-Fill** (`admin` / `Admin@123`) and sign in -> verify redirect to `index.html` and green `C++17 • MySQL Connected` indicator.
2. **Conflict Detection Test (Aircraft, Gate & Runway)**
   - Navigate to **Runways & Gates**.
   - Leave the default test values (`DEL` -> `BOM`, Aircraft `VT-EXU`, Departure `2026-11-16T06:05`) which intentionally overlap with existing flight `6E-204` (`06:00`–`08:15`).
   - Click **Verify Slot Availability** -> verify `[CONFLICT DETECTED]` alert identifying the exact resource collision.
3. **Duplicate Seat Booking Prevention Test**
   - Navigate to **Booking & Seat Map** and select flight `AI-101`.
   - Observe seat `04A` is marked red and crossed out.
   - Manually type `04A` into the seat input box and click **Confirm Reservation** -> verify the C++ backend and MySQL unique constraint reject the duplicate seat (`409 Conflict`).
   - Select an open seat (e.g., `05A`), submit -> verify PNR generation and seat turning red immediately.
4. **Scheduling Algorithms Test**
   - Navigate to **Scheduling Algorithms**, select **Round Robin** with Quantum `8`, keep source as **Live Active Flights from MySQL**, and click **Execute C++ Scheduler**.
   - Verify the Gantt chart slices, Waiting Time, Turnaround Time, and averages render accurately.
