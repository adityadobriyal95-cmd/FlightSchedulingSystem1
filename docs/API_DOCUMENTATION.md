# REST API Reference

All endpoints except `/api/health` and `/api/auth/login` require `Authorization: Bearer <token>`.

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api/health` | Verifies C++ server and MySQL database connectivity |
| `POST` | `/api/auth/login` | Authenticates admin credentials and returns session token |
| `POST` | `/api/auth/logout` | Invalidates active session token |
| `GET` | `/api/auth/me` | Returns authenticated administrator profile |
| `GET` | `/api/airports` | Lists all airports with runway and gate counts |
| `POST` | `/api/airports` | Creates an airport and provisions default runway & gate |
| `PUT` | `/api/airports/:id` | Updates an existing airport |
| `DELETE` | `/api/airports/:id` | Deletes an airport (blocked if referenced by active flights) |
| `GET` | `/api/resources` | Lists all runways and terminal gates |
| `POST` | `/api/runways` | Adds a runway to an airport |
| `POST` | `/api/gates` | Adds a terminal gate to an airport |
| `GET` | `/api/aircraft` | Lists fleet aircraft with live availability and active flight counts |
| `POST` | `/api/aircraft` | Adds a new aircraft to the fleet |
| `PUT` | `/api/aircraft/:id` | Updates aircraft details or availability status |
| `DELETE` | `/api/aircraft/:id` | Removes an unassigned aircraft |
| `GET` | `/api/flights` | Lists flights with optional `?search=`, `?status=`, `?origin=` filters |
| `POST` | `/api/flights` | Validates conflicts and schedules a new flight |
| `PUT` | `/api/flights/:id` | Validates conflicts and updates an existing flight |
| `POST` | `/api/flights/:id/cancel` | Cancels a flight and frees associated aircraft/gates/runways |
| `POST` | `/api/conflicts/check` | Dry-run check for aircraft, gate, and runway schedule overlaps |
| `GET` | `/api/passengers` | Lists registered passengers |
| `POST` | `/api/passengers` | Registers a new passenger |
| `PUT` | `/api/passengers/:id` | Updates passenger details |
| `DELETE` | `/api/passengers/:id` | Deletes a passenger without active bookings |
| `GET` | `/api/bookings` | Lists all passenger bookings and PNR codes |
| `GET` | `/api/flights/:id/seats` | Returns occupied active seats for cabin seat map rendering |
| `POST` | `/api/bookings` | Books a seat (blocks duplicate active seat reservations) |
| `POST` | `/api/bookings/:id/cancel` | Cancels a booking and releases the seat |
| `POST` | `/api/scheduler/run` | Executes FCFS, SJF, Priority, or Round Robin scheduling |
| `GET` | `/api/reports/summary` | Returns executive KPI counts, revenue, and status summaries |
